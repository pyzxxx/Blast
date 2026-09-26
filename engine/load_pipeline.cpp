#include "load_pipeline.h"
#include "log.h"

#include <chrono>

bool LoadPipeline::init(JobSystem* js, LoadSets* sets) {
    if (js == nullptr || sets == nullptr) {
        LOGE("LoadPipeline::init: null JobSystem or LoadSets");
        return false;
    }

    jobs_ = js;
    sets_ = sets;
    return true;
}

void LoadPipeline::shutdown() {
    deferred_.clear();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        completed_.clear();
    }

    inflight_.clear();
    jobs_ = nullptr;
    sets_ = nullptr;
}

void LoadPipeline::submit(LoadTask& task, JobPriority p) {
    auto inf = std::make_shared<InFlight>();
    inf->work = std::move(task.work);
    inf->finalize = std::move(task.finalize);
    inf->abort = std::move(task.abort);
    inf->ready = std::move(task.ready);
    inf->set = task.set;
    inf->debug_id = task.debug_id;
    inf->observers = std::move(task.observers);
    inflight_[inf->debug_id] = inf;

    jobs_->submit(inf->group, [this, inf]() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (inf->cancelled) {
                inf->done = true;
                completed_.push_back(inf);
                return;
            }
        }

        const bool ok = inf->work();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            inf->ok = ok;
            inf->done = true;
            completed_.push_back(inf);
        }
    }, p);
}

bool LoadPipeline::decide(const std::shared_ptr<InFlight>& inf, Settle mode) {
    bool cancelled = false;
    bool ok = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cancelled = inf->cancelled;
        ok = inf->ok;
    }

    if (cancelled || !sets_->valid(inf->set) || !ok) {
        inf->settled = true;
        inflight_.erase(inf->debug_id);
        if (inf->abort) {
            inf->abort(inf->set);
        }

        for (LoadSet obs : inf->observers) {
            sets_->note_pending(obs, -1);
        }

        return true;
    }

    if (mode == Settle::Normal && inf->ready && !inf->ready()) {
        return false;
    }

    inf->settled = true;
    inflight_.erase(inf->debug_id);
    if (inf->finalize) {
        inf->finalize(inf->set);
    }

    for (LoadSet obs : inf->observers) {
        if (sets_->valid(obs) && !sets_->has_member(obs, inf->debug_id)) {
            sets_->track(obs, inf->debug_id);
        }

        sets_->note_pending(obs, -1);
    }

    return true;
}

void LoadPipeline::drain(float budget_ms) {
    using Clock = std::chrono::steady_clock;
    const Clock::time_point start = Clock::now();
    const auto over_budget = [&]() {
        return std::chrono::duration<float, std::milli>(Clock::now() - start).count() >= budget_ms;
    };

    std::vector<std::shared_ptr<InFlight>> parked;
    parked.swap(deferred_);
    for (const std::shared_ptr<InFlight>& inf : parked) {
        if (inf->settled) {
            continue;
        }

        if (over_budget()) {
            deferred_.push_back(inf);
            continue;
        }

        if (!decide(inf, Settle::Normal)) {
            deferred_.push_back(inf);
        }
    }

    for (bool first = true;; first = false) {
        if (!first && over_budget()) {
            break;
        }

        std::shared_ptr<InFlight> inf;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (completed_.empty()) {
                break;
            }

            inf = completed_.front();
            completed_.pop_front();
        }

        if (inf->settled) {
            continue;
        }

        if (!decide(inf, Settle::Normal)) {
            deferred_.push_back(inf);
        }
    }
}

void LoadPipeline::cancel_set(LoadSet set) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [key, inf] : inflight_) {
        if (inf->set != set) {
            continue;
        }

        LoadSet survivor = k_loadset_invalid;
        for (size_t i = 0; i < inf->observers.size(); ++i) {
            if (sets_->valid(inf->observers[i])) {
                survivor = inf->observers[i];
                inf->observers.erase(inf->observers.begin() + static_cast<ptrdiff_t>(i));
                break;
            }
        }

        if (survivor != k_loadset_invalid) {
            inf->set = survivor;
        } else {
            inf->cancelled = true;
        }
    }
}

bool LoadPipeline::add_observer(uint64_t handle, LoadSet set) {
    auto it = inflight_.find(handle);
    if (it == inflight_.end()) {
        return false;
    }

    InFlight& inf = *it->second;
    if (inf.set == set) {
        return false;
    }

    for (LoadSet obs : inf.observers) {
        if (obs == set) {
            return false;
        }
    }

    inf.observers.push_back(set);
    sets_->note_pending(set, +1);
    return true;
}

void LoadPipeline::ensure(uint64_t handle) {
    auto it = inflight_.find(handle);
    if (it == inflight_.end()) {
        return;
    }

    std::shared_ptr<InFlight> inf = it->second;

    jobs_->wait(inf->group);
    if (inf->settled) {
        return;
    }

    for (size_t i = deferred_.size(); i-- > 0;) {
        if (deferred_[i] == inf) {
            deferred_[i] = deferred_.back();
            deferred_.pop_back();
            break;
        }
    }

    decide(inf, Settle::Force);
}
