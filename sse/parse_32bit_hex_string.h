#pragma once

#if defined(_MSC_VER) && defined(_M_X64)
    #include <intrin.h>

#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    #include <x86intrin.h>

#else
    #error "Only really intended for x64 gcc/clang/msc"
#endif


#include <cstdint>


inline
uint32_t f_parse_32b_hex_string(const char* x)
{
    uint64_t y = _bswap64(*((const uint64_t*)x)) - 0x3030303030303030ULL;
    uint64_t sh = (y >> 4) & 0x0101010101010101ULL;
    y -= sh * 8;
    y += sh;
    return (uint32_t)_pext_u64(y, 0x0f0f0f0f0f0f0f0fULL);
}
