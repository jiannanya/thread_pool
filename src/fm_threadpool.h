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

using uint_t = unsigned int;

enum class CacheMode {
    Static,  // fixed thread nums
    Dynamic, // dynamic thread nums
};

// for manage thread pool state
struct ThreadPoolState {
    uint_t __init_internal_thread_nums;
    std::atomic_int __current_thread_nums;
    uint_t __thread_max_threshold;
    std::atomic_int __idle_thread_nums;
    std::atomic_int __queue_task_nums;
    uint_t __task_max_threshold;
    CacheMode __cache_mode;
    std::atomic_bool __is_pool_running;
    std::atomic_bool __is_pool_stopping;
};

class InternalThread {
    static uint_t __internal_id;
    using ThreadFunctionType = std::function<void(uint_t)>;

  public:
    InternalThread(ThreadFunctionType func)
        : __thread_function(func), __thread_id(__internal_id++) {}

    ~InternalThread() {
        std::cout << "inter thread destory " << __thread_id << std::endl;
    };

    void go() {

        std::thread t(__thread_function, __thread_id);
        t.detach();
    }

    uint_t getId() const { return __thread_id; }

  private:
    ThreadFunctionType __thread_function;
    uint_t __thread_id;
};

class ThreadPool : public SingletonStackPolicy<ThreadPool> {
    friend class SingletonStackPolicy<ThreadPool>;
    using Task_type = std::function<void()>;

  private:
    ThreadPool();
    ~ThreadPool();

  public:
    template <typename Func, typename... Args>
    auto submit(Func&& func, Args&&... args)
        -> std::optional<std::future<decltype(func(args...))>> {

        using RType = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<RType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

        std::unique_lock<std::mutex> lock(__task_queue_mutex);

        if (!__task_queue_notfull_cv.wait_for(
                lock, std::chrono::seconds(1), [&]() -> bool {
                    return __task_queue.size() < __state.__task_max_threshold;
                })) {

            std::cerr << "submit task failed" << std::endl;

            return std::nullopt; // queue is full and return null value
        }
        __task_queue.emplace([task]() { (*task)(); });

        __state.__queue_task_nums++;
        std::cout << "submit task success" << std::endl;

        if (__state.__cache_mode == CacheMode::Dynamic &&
            __state.__queue_task_nums > __state.__idle_thread_nums &&
            __state.__current_thread_nums < __state.__thread_max_threshold) {

            auto new_internal_thread =
                std::make_unique<InternalThread>(std::bind(
                    &ThreadPool::thread_execute_function, this,
                    std::placeholders::_1));
            uint_t threadId = new_internal_thread->getId();
            __internal_threads.emplace(
                threadId, std::move(new_internal_thread));
            __internal_threads[threadId]->go(); // 启动线程

            __state.__current_thread_nums++;
            __state.__idle_thread_nums++;
        }

        __task_queue_notempty_cv.notify_all();

        return std::make_optional(std::move(task->get_future()));
    }

    void run(uint_t init_thread_nums = std::thread::hardware_concurrency());

    void stop();

  public:
    void set_cache_mode(CacheMode mode);

    CacheMode get_cache_mode() const;

    void set_task_max_threshold(uint_t threshhold);

    uint_t get_task_max_threshold() const;

    void set_thread_max_threshold(uint_t threshhold);

    uint_t get_thread_max_threshold() const;

  private:
    void thread_execute_function(uint_t threadid);
    bool isRunning() const;

  private:
    // use unique ptr for internal thread destruction
    std::unordered_map<uint_t, std::unique_ptr<InternalThread>>
        __internal_threads;

    std::queue<Task_type> __task_queue;

    std::mutex __task_queue_mutex;
    std::condition_variable __task_queue_notfull_cv;
    std::condition_variable __task_queue_notempty_cv;
    std::condition_variable __stop_cv;

    ThreadPoolState __state;
};

} // namespace fm

#endif