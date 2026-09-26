#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

using JobFn = std::function<void()>;

enum class JobPriority : uint8_t { High = 0, Normal, Background, k_count };

struct JobGroup {
    std::atomic<uint32_t> pending{0};
};

class JobSystem {
public:
    bool init(uint32_t worker_count = 0);
    void shutdown();

    void submit(JobFn fn, JobPriority p = JobPriority::Normal);
    void submit(JobGroup& g, JobFn fn, JobPriority p = JobPriority::Normal);
    void wait(JobGroup& g);

private:
    bool try_pop(JobFn& out);

    void worker_main(uint32_t index);
    bool enqueue(JobFn fn, JobPriority p);
    bool pop_locked(JobFn& out);
    bool queues_nonempty_locked() const;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<JobFn> queues_[static_cast<size_t>(JobPriority::k_count)];
    std::vector<std::thread> workers_;
    std::atomic<bool> stopping_{false};
    bool initialized_ = false;
};
