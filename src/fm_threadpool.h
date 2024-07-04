#pragma once
#ifndef FALLMENT_THREADPOOL_H
#define FALLMENT_THREADPOOL_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <unordered_map>

#include "fm_singleton.h"

namespace fm {

namespace { // internal linkage

using uint_t = unsigned int;

enum class CacheMode {
    Static,  // fixed thread nums
    Dynamic, // dynamic thread nums
};

// for manage thread pool state
struct PoolState {
    uint_t __init_worker_nums;
    std::atomic_int __current_worker_nums;
    uint_t __worker_max_threshold;
    std::atomic_int __idle_worker_nums;
    std::atomic_int __queue_job_nums;
    uint_t __job_max_threshold;
    CacheMode __cache_mode;
    std::atomic_bool __is_pool_running;
    std::atomic_bool __is_pool_stopping;
};

class Worker {
    static uint_t __internal_id;
    using WokerFunctionType = std::function<void(uint_t)>;

  public:
    Worker(WokerFunctionType func)
        : __worker_function(func), __worker_id(__internal_id++) {}

    ~Worker() { std::cout << "worker destory " << __worker_id << std::endl; };

    void go() {

        std::thread t(__worker_function, __worker_id);
        t.detach(); // [mabe todo]
                    // 这里忽略job中途中止时的资源回收操作，控制task的资源回收需要对task本身进行修改，由task提交者负责，
                    // 一般来讲pool worker
                    // 需要主动signal信号，但目前的设计则是需要等待job queue
                    // empty后才能析构worker和thread pool,
                    // 也就是暂时不考虑task的中途中止机制
                    // 注: 线程池外部提交为task，内部理解为job
    }

    uint_t getId() const { return __worker_id; }

  private:
    WokerFunctionType __worker_function;
    uint_t __worker_id;
};

uint_t Worker::__internal_id = 0;

} // namespace

class ThreadPool : public SingletonStackPolicy<ThreadPool> {
    friend class SingletonStackPolicy<ThreadPool>;
    using JobType = std::function<void()>;

  private:
    ThreadPool();
    ~ThreadPool();

  public:
    template <typename Func, typename... Args>
    auto submit(Func&& func, Args&&... args)
        -> std::optional<std::future<decltype(func(args...))>> {

        using RType = decltype(func(args...));
        auto job = std::make_shared<std::packaged_task<RType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

        std::unique_lock<std::mutex> lock(__job_queue_mutex);

        if (!__job_queue_notfull_cv.wait_for(
                lock, std::chrono::seconds(1), [&]() -> bool {
                    return __job_queue.size() < __state.__job_max_threshold;
                })) {

            std::cerr << "submit task failed" << std::endl;

            return std::nullopt; // queue is full and return null value
        }
        __job_queue.emplace([job]() { (*job)(); });

        __state.__queue_job_nums++;
        std::cout << "submit task success" << std::endl;

        if (__state.__cache_mode == CacheMode::Dynamic &&
            __state.__queue_job_nums > __state.__idle_worker_nums &&
            __state.__current_worker_nums < __state.__worker_max_threshold) {

            auto new_worker = std::make_unique<Worker>(std::bind(
                &ThreadPool::worker_execute_function, this,
                std::placeholders::_1));
            uint_t worker_id = new_worker->getId();
            workers.emplace(worker_id, std::move(new_worker));
            workers[worker_id]->go(); // 启动线程

            __state.__current_worker_nums++;
            __state.__idle_worker_nums++;
        }

        __job_queue_notempty_cv.notify_all();

        return std::make_optional(std::move(job->get_future()));
    }

    void run(uint_t init_worker_nums = std::thread::hardware_concurrency());

    void stop();

  public:
    void set_cache_mode(CacheMode mode);

    CacheMode get_cache_mode() const;

    void set_task_max_threshold(uint_t threshhold);

    uint_t get_task_max_threshold() const;

    void set_thread_max_threshold(uint_t threshhold);

    uint_t get_thread_max_threshold() const;

  private:
    void worker_execute_function(uint_t worker_id);
    bool is_running() const;

  private:
    // use unique ptr for internal thread destruction
    std::unordered_map<uint_t, std::unique_ptr<Worker>> workers;

    std::queue<JobType> __job_queue;

    std::mutex __job_queue_mutex;
    std::condition_variable __job_queue_notfull_cv;
    std::condition_variable __job_queue_notempty_cv;
    std::condition_variable __stop_cv;

    PoolState __state;
};

} // namespace fm

#endif