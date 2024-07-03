

#include "test.h"

#include <chrono>
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
    pool->run(5);

    auto res1 = pool->submit(sum1, 1, 2);
    auto res2 = pool->submit(sum2, 1, 2, 3);
    auto res3 = pool->submit(
        [](int d, int e) -> int {
            int sum = 0;
            for (int i = d; i < e; i++)
                sum += i;
            return sum;
        },
        1, 100);

    auto res4 = pool->submit(
        [](int d, int e) -> int {
            int sum = 0;
            for (int i = d; i < e; i++)
                sum += i;
            return sum;
        },
        1, 100);

    auto res5 = pool->submit(
        [](int d, int e) -> int {
            int sum = 0;
            for (int i = d; i < e; i++)
                sum += i;
            return sum;
        },
        1, 100);

    std::cout << (res1.has_value() ? res1.value().get() : -1) << std::endl;
    std::cout << (res2.has_value() ? res2.value().get() : -1) << std::endl;
    std::cout << (res3.has_value() ? res3.value().get() : -1) << std::endl;
    std::cout << (res4.has_value() ? res4.value().get() : -1) << std::endl;
    std::cout << (res5.has_value() ? res5.value().get() : -1) << std::endl;
}
} // namespace fm::test