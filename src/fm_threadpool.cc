

#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <queue>
#include <unordered_map>

#include "fm_threadpool.h"

namespace fm {

uint_t InternalThread::__internal_id = 0;

constexpr uint_t TASK_MAX_NUMS = 2;
constexpr uint_t THREAD_MAX_NUMS = 100;
constexpr uint_t THREAD_MAX_IDLE_TIME = 60; // 60s

ThreadPool::ThreadPool()
    : __state{
          .__init_internal_thread_nums = 4,
          .__current_thread_nums = 0,
          .__thread_max_threshold = THREAD_MAX_NUMS,
          .__idle_thread_nums = 0,
          .__queue_task_nums = 0,
          .__task_max_threshold = TASK_MAX_NUMS,
          .__cache_mode = CacheMode::Static,
          .__is_pool_running = false,
          .__is_pool_stopping = false} {

    std::cout << "construct threadpool" << std::endl;
}

ThreadPool::~ThreadPool() { stop(); }

void ThreadPool::run(uint_t init_thread_nums) {

    if (init_thread_nums > __state.__thread_max_threshold) {
        std::cerr << "init thread nums outbounds" << std::endl;
        return;
    }

    __state.__init_internal_thread_nums = init_thread_nums;
    __state.__current_thread_nums = init_thread_nums;

    for (uint_t i = 0; i < __state.__init_internal_thread_nums; i++) {

        auto new_internal_thread = std::make_unique<InternalThread>(std::bind(
            &ThreadPool::thread_execute_function, this, std::placeholders::_1));
        std::cout << "start running thread" << new_internal_thread->getId()
                  << std::endl;
        __internal_threads.emplace(
            new_internal_thread->getId(), std::move(new_internal_thread));
    }

    for (uint_t i = 0; i < __state.__init_internal_thread_nums; i++) {
        __internal_threads[i]->go();
        __state.__idle_thread_nums++;
    }

    __state.__is_pool_running = true;
}

void ThreadPool::stop() {
    if (__state.__is_pool_running && !__state.__is_pool_stopping) {
        __state.__is_pool_stopping = true;
        std::unique_lock<std::mutex> lock(__task_queue_mutex);

        __task_queue_notempty_cv.notify_all();
        __stop_cv.wait(
            lock, [&]() -> bool { return __internal_threads.empty(); });

        __state.__is_pool_running = false;
        __state.__is_pool_stopping = false;
    }
}

void ThreadPool::set_cache_mode(CacheMode mode) {
    // stall task submit
    std::unique_lock<std::mutex> lock(__task_queue_mutex);
    __state.__cache_mode = mode;
    return;
}

CacheMode ThreadPool::get_cache_mode() const { return __state.__cache_mode; }

void ThreadPool::set_task_max_threshold(uint_t threshhold) {
    std::unique_lock<std::mutex> lock(__task_queue_mutex);
    __state.__task_max_threshold = threshhold;
    return;
}

uint_t ThreadPool::get_task_max_threshold() const {
    return __state.__task_max_threshold;
}

void ThreadPool::set_thread_max_threshold(uint_t threshhold) {
    std::unique_lock<std::mutex> lock(__task_queue_mutex);
    __state.__thread_max_threshold = threshhold;
}

uint_t ThreadPool::get_thread_max_threshold() const {
    return __state.__thread_max_threshold;
}

void ThreadPool::thread_execute_function(uint_t threadid) {
    auto lastTime = std::chrono::high_resolution_clock().now();

    while (true) {
        Task_type task;

        // for getting queue lock
        {
            std::unique_lock<std::mutex> lock(__task_queue_mutex);

            while (__task_queue.empty()) {
                if (__state.__is_pool_stopping) {
                    __internal_threads.clear();
                    __stop_cv.notify_all();
                    return;
                }

                if (__state.__cache_mode == CacheMode::Dynamic) {
                    if (std::cv_status::timeout ==
                        __task_queue_notempty_cv.wait_for(
                            lock, std::chrono::seconds(1))) {
                        auto now = std::chrono::high_resolution_clock().now();
                        auto dur =
                            std::chrono::duration_cast<std::chrono::seconds>(
                                now - lastTime);
                        if (dur.count() >= THREAD_MAX_IDLE_TIME &&
                            __state.__current_thread_nums >
                                __state.__init_internal_thread_nums) {

                            __internal_threads.erase(threadid);
                            __state.__current_thread_nums--;
                            __state.__idle_thread_nums--;

                            return;
                        }
                    }
                } else {
                    __task_queue_notempty_cv.wait(lock);
                }
            }

            __state.__idle_thread_nums--;

            task = __task_queue.front();
            __task_queue.pop();
            __state.__queue_task_nums--;

            if (__task_queue.size() > 0) {
                __task_queue_notempty_cv.notify_all();
            }
            __task_queue_notfull_cv.notify_all();

        } // release queue lock

        if (task != nullptr) {
            task();
        }

        __state.__idle_thread_nums++;
        auto lastTime = std::chrono::high_resolution_clock().now();
    }
}

bool ThreadPool::isRunning() const { return __state.__is_pool_running; }

} // namespace fm
