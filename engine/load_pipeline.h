#pragma once

#include "job_system.h"
#include "loadset.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

struct LoadTask {
    using SettleFn = std::function<void(LoadSet set)>;

    std::function<bool()> work;
    SettleFn finalize;
    SettleFn abort;
    std::function<bool()> ready;
    LoadSet set = k_loadset_invalid;
    uint64_t debug_id = 0;
    std::vector<LoadSet> observers;
    JobGroup group;
};

class LoadPipeline {
public:
    bool init(JobSystem* js, LoadSets* sets);
    void shutdown();

    void submit(LoadTask& task, JobPriority p);
    void drain(float budget_ms);
    void cancel_set(LoadSet set);
    bool add_observer(uint64_t handle, LoadSet set);
    void ensure(uint64_t handle);

private:
    enum class Settle : uint8_t { Normal, Force };

    struct InFlight {
        std::function<bool()> work;
        LoadTask::SettleFn finalize;
        LoadTask::SettleFn abort;
        std::function<bool()> ready;
        LoadSet set = k_loadset_invalid;
        uint64_t debug_id = 0;
        std::vector<LoadSet> observers;
        JobGroup group;
        bool done = false;
        bool ok = false;
        bool cancelled = false;
        bool settled = false;
    };

    bool decide(const std::shared_ptr<InFlight>& inf, Settle mode);

    JobSystem* jobs_ = nullptr;
    LoadSets* sets_ = nullptr;

    std::mutex mutex_;
    std::deque<std::shared_ptr<InFlight>> completed_;
    std::unordered_map<uint64_t, std::shared_ptr<InFlight>> inflight_;
    std::vector<std::shared_ptr<InFlight>> deferred_;
};
