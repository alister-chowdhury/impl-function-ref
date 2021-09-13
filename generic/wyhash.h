#pragma once

// This is a port of the fantastic wyhash (wyhash_final_version_3), which supports constexpr hashing of strings.
// Example usage:
//
//      constexpr uint64_t hash = wyhash::wyhash("Hello");
//
// mum32 (WYHASH_32BIT_MUM) and condom (WYHASH_CONDOM) can be passed as template arguments
// to wyhash::wyhash.
//
// This does not include any of the additional goodies that comes with wyhash.h
// 
// A compiler with C++20 is required, or a newer compiler with C++17 and access to cheeky intrinsics.


#include <type_traits>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string_view>

#if defined(_MSC_VER) && defined(_M_X64)
  #include <intrin.h>
  #pragma intrinsic(_umul128)
#endif


#if (defined(_MSVC_LANG) && (_MSVC_LANG >= 201811L)) || __cplusplus >= 201811L
    #define WYHASH_IS_CONSTANT_EVALUATED std::is_constant_evaluated
#else
    #define WYHASH_IS_CONSTANT_EVALUATED __builtin_is_constant_evaluated
#endif

#if (defined(_MSVC_LANG) && (_MSVC_LANG >= 201803L)) || __cplusplus >= 201803L
    #define WYHASH_IF_LIKELY(x)         if(x)       [[likely]]
    #define WYHASH_IF_UNLIKELY(x)       if(x)       [[unlikely]]
    #define WYHASH_DOWHILE_LIKELY(x)    while(x);   [[likely]]
    #define WYHASH_WHILE_UNLIKELY(x)    while(x)    [[unlikely]]

#elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
    #define WYHASH_IF_LIKELY(x)         if(__builtin_expect(x, 1))
    #define WYHASH_IF_UNLIKELY(x)       if(__builtin_expect(x, 0))
    #define WYHASH_DOWHILE_LIKELY(x)    while(__builtin_expect(x, 1));
    #define WYHASH_WHILE_UNLIKELY(x)    while(__builtin_expect(x, 0))

#else
    #define WYHASH_IF_LIKELY(x)         if(x)
    #define WYHASH_IF_UNLIKELY(x)       if(x)
    #define WYHASH_DOWHILE_LIKELY(x)    while(x);
    #define WYHASH_WHILE_UNLIKELY(x)    while(x)

#endif

#if defined(_MSC_VER)
    #define WYHASH_INLINE __forceinline
#elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
    #define WYHASH_INLINE __attribute__((always_inline)) inline
#else
    #define WYHASH_INLINE inline
#endif

#define WYHASH_CONSTEXPR constexpr WYHASH_INLINE

#ifndef WYHASH_LITTLE_ENDIAN
  #if defined(_WIN32) || defined(__LITTLE_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    #define WYHASH_LITTLE_ENDIAN 1
  #elif defined(__BIG_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    #define WYHASH_LITTLE_ENDIAN 0
  #else
    #warning could not determine endianness! Falling back to little endian.
    #define WYHASH_LITTLE_ENDIAN 1
  #endif
#endif


namespace wyhash
{

template<typename Char>
WYHASH_CONSTEXPR uint64_t wyr8(const Char* p)
{
    if(WYHASH_IS_CONSTANT_EVALUATED())
    {
        if(WYHASH_LITTLE_ENDIAN)
        {
            uint64_t v = uint8_t(p[7]);
                     v <<= 8; v |= uint8_t(p[6]);
                     v <<= 8; v |= uint8_t(p[5]);
                     v <<= 8; v |= uint8_t(p[4]);
                     v <<= 8; v |= uint8_t(p[3]);
                     v <<= 8; v |= uint8_t(p[2]);
                     v <<= 8; v |= uint8_t(p[1]);
                     v <<= 8; v |= uint8_t(p[0]);
            return v;
        }
        else
        {
            uint64_t v = uint8_t(p[0]);
                     v <<= 8; v |= uint8_t(p[1]);
                     v <<= 8; v |= uint8_t(p[2]);
                     v <<= 8; v |= uint8_t(p[3]);
                     v <<= 8; v |= uint8_t(p[4]);
                     v <<= 8; v |= uint8_t(p[5]);
                     v <<= 8; v |= uint8_t(p[6]);
                     v <<= 8; v |= uint8_t(p[7]);
            return v;
        }
    }
    else
    {
        uint64_t v = 0;
        std::memcpy(&v, p, 8);
        #if !WYHASH_LITTLE_ENDIAN
            #if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
                __builtin_bswap64(v);
            #elif defined(_MSC_VER)
                _byteswap_uint64(v);
            #else
                v = (((v >> 56) & 0xff)| ((v >> 40) & 0xff00)| ((v >> 24) & 0xff0000)| ((v >>  8) & 0xff000000)| ((v <<  8) & 0xff00000000)| ((v << 24) & 0xff0000000000)| ((v << 40) & 0xff000000000000)| ((v << 56) & 0xff00000000000000));
            #endif
        #endif
        return v;
    }
}

template<typename Char>
WYHASH_CONSTEXPR uint64_t wyr4(const Char* p)
{
    if(WYHASH_IS_CONSTANT_EVALUATED())
    {
        if(WYHASH_LITTLE_ENDIAN)
        {
            uint32_t v = uint8_t(p[3]);
                     v <<= 8; v |= uint8_t(p[2]);
                     v <<= 8; v |= uint8_t(p[1]);
                     v <<= 8; v |= uint8_t(p[0]);
            return v;
        }
        else
        {
            uint32_t v = uint8_t(p[0]);
                     v <<= 8; v |= uint8_t(p[1]);
                     v <<= 8; v |= uint8_t(p[2]);
                     v <<= 8; v |= uint8_t(p[3]);
            return v;
        }
    }
    else
    {
        uint32_t v = 0;
        std::memcpy(&v, p, 4);
        #if !WYHASH_LITTLE_ENDIAN
            #if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
                __builtin_bswap32(v);
            #elif defined(_MSC_VER)
                _byteswap_ulong(v);
            #else
                v = (((v >> 24) & 0xff)| ((v >>  8) & 0xff00)| ((v <<  8) & 0xff0000)| ((v << 24) & 0xff000000));
            #endif
        #endif
        return v;
    }
}

template<typename Char>
WYHASH_CONSTEXPR uint64_t wyr3(const Char* p, size_t k)
{
    return (((uint64_t)p[0]) << 16) | (((uint64_t)p[k >> 1]) << 8) | p[k - 1];
}


WYHASH_CONSTEXPR uint64_t wyrot(uint64_t x)
{
    return (x>>32) | (x<<32);
}


// 128bit multiply function
template<bool mum32=false, int condom=1>
WYHASH_CONSTEXPR void wymum(uint64_t* A, uint64_t* B)
{
    if constexpr(mum32)
    {
        uint64_t hh = (*A >> 32) * (*B >> 32);
        uint64_t hl = (*A >> 32) * (uint32_t)*B;
        uint64_t lh = (uint32_t)*A * (*B >> 32);
        uint64_t ll = (uint64_t)((uint32_t)*A * (uint32_t)*B);

        if constexpr(condom > 1)
        {
            *A ^= wyrot(hl) ^ hh;
            *B ^= wyrot(lh) ^ ll;
        }
        else
        {
              *A = wyrot(hl) ^ hh;
              *B = wyrot(lh) ^ ll;
        }
        return;
    }

    // __uint128_t seems to happily be constexpr friendly
    #if defined(__SIZEOF_INT128__)
        __uint128_t r = *A;
                    r *= *B;

        if constexpr(condom > 1)
        {
            *A ^= (uint64_t)r;
            *B ^= (uint64_t)(r>>64);
        }
        else
        {
            *A = (uint64_t)r;
            *B = (uint64_t)(r>>64);
        }
        return;

    // _umul128 however is not, so we need to guard against it
    #elif defined(_MSC_VER) && defined(_M_X64)
        if(!WYHASH_IS_CONSTANT_EVALUATED())
        {
            if constexpr(condom > 1)
            {
                uint64_t a;
                uint64_t b;
                a= _umul128(*A,*B, &b);
                *A ^= a;
                *B ^= b;
            }
            else
            {
                *A= _umul128(*A, *B, B);
            }
            return;
        }
    #endif

    uint64_t ha = *A >> 32;
    uint64_t hb = *B >> 32;
    uint64_t la = (uint32_t)*A;
    uint64_t lb = (uint32_t)*B;
    
    uint64_t rh = ha * hb;
    uint64_t rm0 = ha * lb;
    uint64_t rm1 = hb * la;
    uint64_t rl = la * lb;
    uint64_t t = rl + (rm0 << 32);
    uint64_t c = t < rl;
    uint64_t lo = t + (rm1 << 32);
    c += lo < t;
    uint64_t hi = rh + (rm0 >> 32) + (rm1 >> 32) + c;

    if constexpr(condom > 1)
    {
        *A ^= lo;
        *B ^= hi;
    }
    else
    {
        *A = lo;
        *B = hi;
    }
}

// multiply and xor mix function, aka MUM
template<bool mum32=false, int condom=1>
WYHASH_CONSTEXPR uint64_t wymix(uint64_t A, uint64_t B)
{
    wymum<mum32, condom>(&A, &B);
    return A ^ B;
}

//the default secret parameters
constexpr inline uint64_t wyp[4] = {0xa0761d6478bd642full, 0xe7037ed1a0b428dbull, 0x8ebc6af09c88c6e3ull, 0x589965cc75374cc3ull};

// wyhash main function
template <bool mum32 = false, int condom = 1>
WYHASH_CONSTEXPR uint64_t wyhash(const char *key, size_t len, uint64_t seed = 0, const uint64_t *secret = wyp)
{
    const char *p = key;
    seed ^= *secret;
    uint64_t a = 0;
    uint64_t b = 0;
    WYHASH_IF_LIKELY(len <= 16)
    {
        WYHASH_IF_LIKELY(len >= 4)
        {
            a = (wyr4(p) << 32) | wyr4(p + ((len >> 3) << 2));
            b = (wyr4(p + len - 4) << 32) | wyr4(p + len - 4 - ((len >> 3) << 2));
        }
        else WYHASH_IF_LIKELY(len > 0)
        {
            a = wyr3(p, len);
            b = 0;
        }
        else
        {
            a = 0;
            b = 0;
        }
    }
    else
    {
        size_t i = len;
        WYHASH_IF_UNLIKELY(i > 48)
        {
            uint64_t see1 = seed;
            uint64_t see2 = seed;
            do
            {
                seed = wymix<mum32, condom>(wyr8(p) ^ secret[1], wyr8(p + 8) ^ seed);
                see1 = wymix<mum32, condom>(wyr8(p + 16) ^ secret[2], wyr8(p + 24) ^ see1);
                see2 = wymix<mum32, condom>(wyr8(p + 32) ^ secret[3], wyr8(p + 40) ^ see2);
                p += 48;
                i -= 48;
            }
            WYHASH_DOWHILE_LIKELY(i > 48)

            seed ^= see1 ^ see2;
        }
        WYHASH_WHILE_UNLIKELY(i > 16)
        {
            seed = wymix<mum32, condom>(wyr8(p) ^ secret[1], wyr8(p + 8) ^ seed);
            i -= 16;
            p += 16;
        }
        a = wyr8(p + i - 16);
        b = wyr8(p + i - 8);
    }
    return wymix<mum32, condom>(secret[1] ^ len, wymix<mum32, condom>(a ^ secret[1], b ^ seed));
}

template<bool mum32=false, int condom=1>
WYHASH_CONSTEXPR uint64_t wy2u0k(uint64_t A, uint64_t B)
{
    wymum<mum32, condom>(&A, &B);
    return B;
}

// Non constexpr vesion
template<bool mum32=false, int condom=1>
inline uint64_t wyhash(const void* key, size_t len, uint64_t seed=0, const uint64_t* secret=wyp)
{
    return wyhash<mum32, condom>((const char*)key, len, seed, secret);
}

template<bool mum32=false, int condom=1>
WYHASH_CONSTEXPR uint64_t wyhash(std::string_view sv, uint64_t seed=0, const uint64_t* secret=wyp)
{
    return wyhash<mum32, condom>(sv.data(), sv.size(), seed, secret);
}


}  // namespace wyhash
