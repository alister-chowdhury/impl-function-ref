#pragma once

#include <atomic>
#include <thread>


struct spinlock
{
    void lock()     { while(flag.test_and_set()) [[unlikely]] { std::this_thread::yield(); } }
    void unlock()   { flag.clear(std::memory_order_relaxed); }
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

