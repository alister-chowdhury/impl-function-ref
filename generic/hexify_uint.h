#pragma once


// Parallel hexifying of uints without needing a lookup loop.
// + simple unrolled impl for copypaste goodness
//
// e.g:
//
// char buffer[9] = {0};
//
// parallel_hexify_u32(0x1a2b, buffer);
// printf("%s\n", buffer); // 00001a2b
//
// parallel_hexify_u32<false>(0x1a2b, buffer);
// printf("%s\n", buffer); // 00001A2B
//

// Parallel method can be faster than looping on GCC:
// https://quick-bench.com/q/rouGPa_DRs2rWtmwaWuRFQmnDPQ
//
// Ultimatley manually unrolling (as clang does) is faster.
// https://quick-bench.com/q/KVRX9VwP5VuqP7S8B9jVRiTMdSs
//

#include <cstdint>
#include <cstring>

#ifdef _MSC_VER
#include <stdlib.h> // _byteswap_uint64
#endif


template<bool lowercase=true>
uint64_t parallel_hexify_u32(uint32_t x_)
{
    uint64_t x = x_;

    // Pad every other nibble with 0
    // i.e:
    // 0x1234 => 0x01020304
    x = (x | (x << 16)) & 0x0000ffff0000ffffULL; // 64
    x = (x | (x << 8 )) & 0x00ff00ff00ff00ffULL; // 32
    x = (x | (x << 4 )) & 0x0f0f0f0f0f0f0f0fULL; // 16

    // Numeric baseline 0 => '0', ... , 9 => '9'
    uint64_t numeric = x + 0x3030303030303030ULL;

    // Mask for alphas (a-f)
    uint64_t alpha = (x >> 2) | (x >> 1);
             alpha &= (x >> 3);
             alpha &= 0x0101010101010101ULL;

    // For uppercase we need to add 0x07 (0b00000111) to each byte
    if(!lowercase)
    {
        alpha |= (alpha << 1) | (alpha << 2);
    }
    // For lowercase we need to add 0x27 (0b01000111) to each byte
    else
    {
        alpha |= (alpha << 1) | (alpha << 2) | (alpha << 5);
    }

    x = numeric + alpha;

    // Assuming a little endian system, we need to do a byte swap
    #ifdef _MSC_VER
    x = _byteswap_uint64(x);
    #else
    x = __builtin_bswap64(x);
    #endif

    return x;
}


template<bool lowercase=true>
inline void parallel_hexify_u32(uint32_t x, char* output)
{
    const uint64_t data = parallel_hexify_u32<lowercase>(x);
    std::memcpy(output, &data, sizeof(uint64_t));
}

template<bool lowercase=true>
inline void parallel_hexify_u64(uint64_t x, char* output)
{
    parallel_hexify_u32(uint32_t(x >> 32), output);
    parallel_hexify_u32(uint32_t(x), output + sizeof(uint64_t));
}


template<bool lowercase=true>
void hexify_u32(uint32_t x, char* output)
{
    const static char upperhex[] = "0123456789ABCDEF";
    const static char lowerhex[] = "0123456789abcdef";

    const char* hex = lowercase ? lowerhex : upperhex;

    output += 7;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
}


template<bool lowercase=true>
void hexify_u64(uint64_t x, char* output)
{
    const static char upperhex[] = "0123456789ABCDEF";
    const static char lowerhex[] = "0123456789abcdef";

    const char* hex = lowercase ? lowerhex : upperhex;

    output += 15;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
    *output-- = hex[x & 0xf]; x >>= 4;
}
