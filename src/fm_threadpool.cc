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

constexpr uint_t JOB_MAX_NUMS = 6;
constexpr uint_t WORKER_MAX_NUMS = 100;
constexpr uint_t WORKER_MAX_IDLE_TIME = 60; // 60s

ThreadPool::ThreadPool()
    : __state{
          .__init_worker_nums = 4,
          .__current_worker_nums = 0,
          .__worker_max_threshold = WORKER_MAX_NUMS,
          .__idle_worker_nums = 0,
          .__queue_job_nums = 0,
          .__job_max_threshold = JOB_MAX_NUMS,
          .__cache_mode = CacheMode::Static,
          .__is_pool_running = false,
          .__is_pool_stopping = false} {

    std::cout << "construct threadpool" << std::endl;
}

ThreadPool::~ThreadPool() {
    stop();
    std::cout << "destory threadpool" << std::endl;
}

void ThreadPool::run(uint_t init_worker_nums) {

    if (init_worker_nums > __state.__worker_max_threshold) {
        std::cerr << "init thread nums outbounds" << std::endl;
        return;
    }

    __state.__init_worker_nums = init_worker_nums;
    __state.__current_worker_nums = init_worker_nums;

    for (uint_t i = 0; i < __state.__init_worker_nums; i++) {

        auto new_worker = std::make_unique<Worker>(std::bind(
            &ThreadPool::worker_execute_function, this, std::placeholders::_1));
        std::cout << "start running thread" << new_worker->getId() << std::endl;
        workers.emplace(new_worker->getId(), std::move(new_worker));
    }

    for (uint_t i = 0; i < __state.__init_worker_nums; i++) {
        workers[i]->go();
        __state.__idle_worker_nums++;
    }

    __state.__is_pool_running = true;
}

void ThreadPool::stop() {
    if (__state.__is_pool_running && !__state.__is_pool_stopping) {
        __state.__is_pool_stopping = true;
        std::unique_lock<std::mutex> lock(__job_queue_mutex);

        __job_queue_notempty_cv.notify_all();
        __stop_cv.wait(lock, [&]() -> bool { return workers.empty(); });

        __state.__is_pool_running = false;
        __state.__is_pool_stopping = false;
    }
}

void ThreadPool::set_cache_mode(CacheMode mode) {
    // stall task submit
    std::unique_lock<std::mutex> lock(__job_queue_mutex);
    __state.__cache_mode = mode;
    return;
}

CacheMode ThreadPool::get_cache_mode() const { return __state.__cache_mode; }

void ThreadPool::set_task_max_threshold(uint_t threshhold) {
    std::unique_lock<std::mutex> lock(__job_queue_mutex);
    __state.__job_max_threshold = threshhold;
    return;
}

uint_t ThreadPool::get_task_max_threshold() const {
    return __state.__job_max_threshold;
}

void ThreadPool::set_thread_max_threshold(uint_t threshhold) {
    std::unique_lock<std::mutex> lock(__job_queue_mutex);
    __state.__worker_max_threshold = threshhold;
}

uint_t ThreadPool::get_thread_max_threshold() const {
    return __state.__worker_max_threshold;
}

void ThreadPool::worker_execute_function(uint_t worker_id) {
    auto lastTime = std::chrono::high_resolution_clock().now();

    while (true) {
        JobType job;

        // for getting queue lock
        {
            std::unique_lock<std::mutex> lock(__job_queue_mutex);

            while (__job_queue.empty()) {
                if (__state.__is_pool_stopping) {
                    workers.erase(worker_id);
                    __stop_cv.notify_all();
                    std::cout << "pool stopping" << worker_id << std::endl;
                    return;
                }

                if (__state.__cache_mode == CacheMode::Dynamic) {
                    if (std::cv_status::timeout ==
                        __job_queue_notempty_cv.wait_for(
                            lock, std::chrono::seconds(1))) {
                        auto now = std::chrono::high_resolution_clock().now();
                        auto dur =
                            std::chrono::duration_cast<std::chrono::seconds>(
                                now - lastTime);
                        if (dur.count() >= WORKER_MAX_IDLE_TIME &&
                            __state.__current_worker_nums >
                                __state.__init_worker_nums) {

                            workers.erase(worker_id);
                            __state.__current_worker_nums--;
                            __state.__idle_worker_nums--;

                            return;
                        }
                    }
                } else {
                    std::cout << "wait queue not empty" << worker_id
                              << std::endl;
                    __job_queue_notempty_cv.wait(lock);
                }
            }

            __state.__idle_worker_nums--;
            std::cerr << "worker get job ok" << std::endl;
            job = __job_queue.front();
            __job_queue.pop();
            __state.__queue_job_nums--;

            if (__job_queue.size() > 0) {
                __job_queue_notempty_cv.notify_all();
            }
            __job_queue_notfull_cv.notify_all();

        } // release queue lock

        if (job != nullptr) {
            job();
        }

        __state.__idle_worker_nums++;
        auto lastTime = std::chrono::high_resolution_clock().now();
    }
}

bool ThreadPool::is_running() const { return __state.__is_pool_running; }

} // namespace fm
