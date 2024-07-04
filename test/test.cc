

#include "test.h"

#include <chrono>
#include <future>
#include <iostream>
#include <thread>

namespace fm::test {

int sum1(int a, int b) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return a + b;
}

int sum2(int a, int b, int c) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return a + b + c;
}

auto testCase1() -> void {
    auto pool = fm::ThreadPool::instance();
    // auto t = std::async(
    //     std::launch::async, std::bind(&fm::ThreadPool::run, pool, 5));
    pool->run(5);

    auto res1 = pool->submit(sum1, 1, 2);
    auto res2 = pool->submit(sum2, 1, 2, 3);
    auto res3 = pool->submit(
        [](int d, int e) -> int {
            int sum = 0;
            for (int i = d; i < e; i++) {
                // std::cout << " task 3" << std::endl;
                sum += i;
            }
            std::cout << " task 3 ok" << std::endl;
            return sum;
        },
        1, 100);

    auto res4 = pool->submit(
        [](int d, int e) -> int {
            int sum = 0;
            for (int i = d; i < e; i++) {
                // std::cout << " task 4" << std::endl;
                sum += i;
            }
            std::cout << " task 4 ok" << std::endl;
            return sum;
        },
        1, 100);

    auto res5 = pool->submit(
        [](int d, int e) -> int {
            int sum = 0;
            for (int i = d; i < e; i++) {
                // std::cout << " task 5" << std::endl;
                sum += i;
            }
            std::cout << " task 5 ok" << std::endl;
            return sum;
        },
        1, 100);
    std::cout << " get value 0 " << res1.has_value() << res2.has_value()
              << res3.has_value() << res4.has_value() << res5.has_value()
              << std::endl;
    std::cout << " res1 " << (res1.has_value() ? res1.value().get() : -1)
              << std::endl;
    std::cout << " get value 1" << std::endl;
    std::cout << " res2 " << (res2.has_value() ? res2.value().get() : -1)
              << std::endl;
    std::cout << " get value 2" << std::endl;
    std::cout << " res3 " << (res3.has_value() ? res3.value().get() : -1)
              << std::endl;
    std::cout << " get value 3" << std::endl;
    std::cout << " res4 " << (res4.has_value() ? res4.value().get() : -1)
              << std::endl;
    std::cout << " get value 4" << std::endl;
    std::cout << " res5 " << (res5.has_value() ? res5.value().get() : -1)
              << std::endl;
    std::cout << " get value 5" << std::endl;

    // while (1) {
    // }

    return;
}
} // namespace fm::test