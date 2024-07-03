#pragma once
#ifndef FALLMENT_SINGLETON
#define FALLMENT_SINGLETON

#include <mutex>

namespace fm {

template <typename T> class SingletonStackPolicy {
  public:
    static T* instance() {
        static T instance;
        return &instance;
    }

    SingletonStackPolicy(SingletonStackPolicy&&) = delete;
    SingletonStackPolicy(const SingletonStackPolicy&) = delete;
    void operator=(const SingletonStackPolicy&) = delete;

  protected:
    SingletonStackPolicy() = default;
    virtual ~SingletonStackPolicy() = default;
};

template <typename T> class SingletonHeapPolicy {
  public:
    static SingletonHeapPolicy* instance() {
        static std::once_flag createFlag;
        static T* instance;
        std::call_once(createFlag, []() { instance = new T{}; });
        return instance;
    }

    SingletonHeapPolicy(SingletonHeapPolicy&&) = delete;
    SingletonHeapPolicy(const SingletonHeapPolicy&) = delete;
    void operator=(const SingletonHeapPolicy&) = delete;

  protected:
    SingletonHeapPolicy() = default;
    virtual ~SingletonHeapPolicy() = default;
};
} // namespace fm

#endif