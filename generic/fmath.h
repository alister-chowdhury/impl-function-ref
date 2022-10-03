#pragma once


#include <cmath>
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


// y = 2^x
CONSTEXPRINLINE float fPow2(const int x)
{
    return std::bit_cast<float>(0x3f800000 + (x << 23));
}


// y = 2^-x
CONSTEXPRINLINE float fInvPow2(const int x)
{
    return std::bit_cast<float>(0x3f800000 - (x << 23));
}


CONSTEXPRINLINE float u8ToF32(uint32_t y)
{
    // 1x shift_add
    // 1x mad
    y = 0x3f800000u + (y << 15);
    float x = 256.0f/255.0f * std::bit_cast<float>(y) - 256.0f/255.0f;
    return x;
}


CONSTEXPRINLINE uint32_t f32ToU8(float y)
{
    // 1x mad
    // 1x shift_add
    y = y * (255.5f/256.0f) + 1.000979431929481f; // 255.5f/256.0f * 256.0f/255.25f
    int x = std::bit_cast<int32_t>(y);
    return std::bit_cast<uint32_t>(-0x7f00 + (x >> 15));
}


CONSTEXPRINLINE float u16ToF32(uint32_t y)
{
    // 1x shift_add
    // 1x mad
    y = 0x3f800000u + (y << 7);
    float x = 65536.0f/65535.0f * std::bit_cast<float>(y) - 65536.0f/65535.0f;
    return x;
}


// [0, 1) = [0, 0.996094]
CONSTEXPRINLINE float u8LinearBounded(uint32_t y)
{
    // 1x shift_add
    // 1x add
    y = 0x3f800000u + (y << 15);
    float x = std::bit_cast<float>(y) - 1.0f;
    return x;
}


// [0, 1) = [0, 0.999985]
CONSTEXPRINLINE float u16LinearBounded(uint32_t y)
{
    // 1x shift_add
    // 1x add
    y = 0x3f800000u + (y << 7);
    float x = std::bit_cast<float>(y) - 1.0f;
    return x;
}


// Uses the last 23bits to construct a linear range
// [0, 1) = [0, 0.9999998808]
CONSTEXPRINLINE float randomBounded(uint32_t seed)
{
    // 1x shift_add
    // 1x add
    seed = 0x3f800000u + (seed & 0x7fffffu);
    float x = std::bit_cast<float>(seed) - 1.0f;
    return x;
}


CONSTEXPRINLINE float floorLog2(float x)
{
    return std::bit_cast<float>(std::bit_cast<uint32_t>(x) & 0xff800000u);
}


// calculates the next 2^exponent after x
// (0, 0)   => 1
// (0.1, 0) => 1
// (1, 0)   => 2
// (0, 1)   => 2
// (0.1, 1) => 2
// (2, 1)   => 4
CONSTEXPRINLINE float next2n(float x, int n)
{
    // 1x shift
    // 2x add
    // per dim: 1x mul
    // per dim: 1x floor
    // per dim: 1x mad
    float lower = std::bit_cast<float>(0x3f800000 - (n << 23));
    float raise = std::bit_cast<float>(0x3f800000 + (n << 23));
    // x = (std::floor(x * lower) + 1.0f) * raise;
    x = (std::floor(x * lower) * raise + raise);
    return x;
}

CONSTEXPRINLINE uint32_t next2n_u32(uint32_t x, uint32_t n)
{
    return (x | ((1 << n) - 1)) + 1;
}


// Example usage:
// C = randomBounded(pcgHash(x + y * width));
CONSTEXPRINLINE uint32_t pcgHash(uint32_t a)
{
    uint32_t state = a * 747796405u + 2891336453u;
    uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}


// Simplified hash, based of the first stage of wyhash32 with a fixed 12byte payload
CONSTEXPRINLINE uint32_t simpleHash32(uint32_t x, uint32_t y, uint32_t z)
{
    uint32_t hx = (0xb543c3a6u ^ x);
    uint32_t hy = (0x526f94e2u ^ y);
    uint32_t hxy = hx * hy;
    uint32_t hz0 = 0x53c5ca59u ^ (hxy >> 5u);
    uint32_t hz1 = (0x74743c1bu ^ z);
    uint32_t h = hz0 * hz1;
    return h;
}

CONSTEXPRINLINE void simpleHash32x3(uint32_t x, uint32_t y, uint32_t z, uint32_t* out)
{
    uint32_t hx = (0xb543c3a6u ^ x);
    uint32_t hy = (0x526f94e2u ^ y);
    uint32_t hxy = hx * hy;
    uint32_t hz0 = 0x53c5ca59u ^ (hxy >> 5u);
    uint32_t hz1 = (0x74743c1bu ^ z);
    uint32_t ha = hz0 * hz1;
    uint32_t hb = hz0 * (0x53c5ca59u ^ ha);
    uint32_t hc = hz0 * (0x74743c1bu ^ hb);
    out[0] = ha;
    out[1] = hb;
    out[2] = hc;
}
