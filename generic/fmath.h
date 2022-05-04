#pragma once


#include <cstdint>
#include <bit>


#if defined(_MSC_VER)
    #define FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
    #define FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define FORCE_INLINE inline
#endif


#define CONSTEXPRINLINE constexpr FORCE_INLINE


// y = 1/x
// Only works if x is a power of 2
// if x = 0, result it 1.70141183E+38
CONSTEXPRINLINE float rcpForPowersOf2(const float* x)
{
    return std::bit_cast<float>(0x7f000000 - std::bit_cast<uint32_t>(*x));
}
