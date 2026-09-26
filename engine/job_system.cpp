#include "job_system.h"
#include "log.h"

#include <algorithm>
#include <cstdio>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <pthread.h>
#endif

namespace {

void set_thread_name(uint32_t index) {
#if defined(_WIN32)
    using SetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    static const auto set_thread_description = reinterpret_cast<SetThreadDescriptionFn>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription")));
    if (set_thread_description == nullptr) {
        return;
    }

    wchar_t name[32];
    swprintf_s(name, L"blast-job-%u", index);
    set_thread_description(GetCurrentThread(), name);
#elif defined(__APPLE__)
    char name[32];
    snprintf(name, sizeof(name), "blast-job-%u", index);
    pthread_setname_np(name);
#elif defined(__linux__)
    char name[32];
    snprintf(name, sizeof(name), "blast-job-%u", index);
    pthread_setname_np(pthread_self(), name);
#else
    (void)index;
#endif
}

} // namespace

bool JobSystem::init(uint32_t worker_count) {
    if (worker_count == 0) {
        const uint32_t hw = std::thread::hardware_concurrency();
        worker_count = hw > 1 ? hw - 1 : 1;
        worker_count = std::min(worker_count, 15u);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_) {
            LOGE("JobSystem::init: already initialized");
            return false;
        }

        stopping_.store(false, std::memory_order_release);
        initialized_ = true;
    }

    workers_.reserve(worker_count);
    for (uint32_t i = 0; i < worker_count; ++i) {
        workers_.emplace_back(&JobSystem::worker_main, this, i);
    }

    LOGI("JobSystem: started %u workers", worker_count);
    return true;
}

void JobSystem::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) {
            return;
        }

        initialized_ = false;
        stopping_.store(true, std::memory_order_release);
    }

    cv_.notify_all();
    for (std::thread& t : workers_) {
        t.join();
    }

    workers_.clear();
    stopping_.store(false, std::memory_order_release);
    LOGI("JobSystem: shutdown complete");
}

void JobSystem::submit(JobFn fn, JobPriority p) {
    enqueue(std::move(fn), p);
}

void JobSystem::submit(JobGroup& g, JobFn fn, JobPriority p) {
    g.pending.fetch_add(1, std::memory_order_relaxed);
    JobFn wrapper = [this, &g, fn = std::move(fn)]() mutable {
        fn();
        if (g.pending.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
            }

            cv_.notify_all();
        }
    };
    if (!enqueue(std::move(wrapper), p)) {
        g.pending.fetch_sub(1, std::memory_order_acq_rel);
    }
}

bool JobSystem::enqueue(JobFn fn, JobPriority p) {
    if (p >= JobPriority::k_count) {
        LOGE("JobSystem::submit: invalid priority %u, demoted to Normal", static_cast<uint32_t>(p));
        p = JobPriority::Normal;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) {
            LOGE("JobSystem::submit: not initialized, job dropped");
            return false;
        }

        queues_[static_cast<size_t>(p)].push_back(std::move(fn));
    }

    cv_.notify_one();
    return true;
}

void JobSystem::wait(JobGroup& g) {
    for (;;) {
        JobFn fn;
        if (try_pop(fn)) {
            fn();
            continue;
        }

        if (g.pending.load(std::memory_order_acquire) == 0) {
            return;
        }

        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&] {
            return g.pending.load(std::memory_order_acquire) == 0 || queues_nonempty_locked();
        });
    }
}

bool JobSystem::try_pop(JobFn& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    return pop_locked(out);
}

bool JobSystem::pop_locked(JobFn& out) {
    for (std::deque<JobFn>& q : queues_) {
        if (!q.empty()) {
            out = std::move(q.front());
            q.pop_front();
            return true;
        }
    }

    return false;
}

bool JobSystem::queues_nonempty_locked() const {
    for (const std::deque<JobFn>& q : queues_) {
        if (!q.empty()) {
            return true;
        }
    }

    return false;
}

void JobSystem::worker_main(uint32_t index) {
    set_thread_name(index);
    for (;;) {
        JobFn fn;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [&] {
                return stopping_.load(std::memory_order_acquire) || queues_nonempty_locked();
            });
            if (!pop_locked(fn)) {
                return;
            }
        }

        fn();
    }
}

