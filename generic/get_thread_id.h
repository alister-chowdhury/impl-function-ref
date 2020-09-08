#pragma once

#include <cstdint>

#if defined(_MSC_VER) && defined(_M_X64)
    #include <intrin.h>

#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    #include <x86intrin.h>

#else
    #error "Only really intended for x64 gcc/clang/msc"
#endif


// Purpose of this is to allow per-thread caches to be persistent.
// I.e scratch spaces for things like decompression.


inline uint32_t get_thread_id() {
    uint32_t thread_id;
    __rdtscp(&thread_id);
    return thread_id;
}

