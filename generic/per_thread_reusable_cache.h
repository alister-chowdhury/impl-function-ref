#pragma once

#include <cstdint>
#include <thread>
#include <vector>


#if defined(_MSC_VER) && defined(_M_X64)
    #include <intrin.h>

#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    #include <x86intrin.h>

#else
    #error "Only really intended for x64 gcc/clang/msc"
#endif



// Basically something akin to using thread_local
// but without the object being destroyed when the thread dies.
// (Allowing you to reuse the same objects between threads.)
template <typename T>
struct per_thread_reusable_cache {

    per_thread_reusable_cache()
    : m_data( std::thread::hardware_concurrency() )
    {}

    template <typename... DefaultArgTs>
    explicit per_thread_reusable_cache(DefaultArgTs... args)
    : m_data( std::thread::hardware_concurrency(), args... )
    {}

    T& get(void) {

        uint32_t thread_id;
        __rdtscp(&thread_id);
        return m_data[thread_id];
    }

private:
    std::vector< T > m_data;

};
