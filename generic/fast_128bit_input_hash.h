#pragma once

#include <cstdint>

#if defined(_MSC_VER) && defined(_M_X64)
    #include <intrin.h>
    #pragma intrinsic(_umul128)
#endif


// Very fast hash for 128bits of data, basically
// the top half of what gets emited for wyhash, when
// saftey=0
// Really intended to be used for storing lines in a hashmap.

// Prevent unwanted vectorization
#if defined(__GNUC__) && defined(__i386__)
  __attribute__((__target__("no-sse")))
#endif
inline
uint64_t fast_128bit_input_hash(const void* data) {

    /*
        Target assembly (more-or-less):
        movabs  rax, -1800455987208640293
        xor     rax, QWORD PTR [rdi]
        movabs  rdx, -6884282663029611473
        xor     rdx, QWORD PTR [rdi+8]
        mul     rdx
        xor     rax, rdx
        ret
    */

    const uint64_t* ptr = (const uint64_t*)data;
    const uint64_t A = 0xe7037ed1a0b428dbULL ^ ptr[0];
    const uint64_t B = 0xa0761d6478bd642fULL ^ ptr[1];

    // gcc / clang
    #ifdef __SIZEOF_INT128__
        __uint128_t A128 = A;
        __uint128_t B128 = B;

        A128 *= B128;
        A128 ^= (A128 >> 64);

        return A128;

    // msc intrinstic
    #elif defined(_MSC_VER) && defined(_M_X64)
        uint64_t high;
        uint64_t low = _umul128(A, B, &high);
        return low ^ high;

    #else
        #error "Only really intended for x64 gcc/clang/msc"
    #endif

}


// The below two functions seem to interchange between which one performs better
// when I've run tests, both are faster than the above.
// The AES hasher is probably the more prefered one, due to it hashing a bit better
// as long as you're not in a tight loop and batch hashing 128bits, as something
// about the crc32 one seems to handle that a bit better.


#ifdef __SSE4_2__ 

// Faster than above, but doesn't hash as well, but should be for something
// like a generating bucket ids.
inline
uint64_t fast_128bit_input_hash_2(const void* data) {
    uint64_t r0 = _mm_crc32_u64(0, P[0]);
    uint64_t r1 = _mm_crc32_u64(r0, P[1]);
    return (r1 | (r0 << 32));

#endif


#ifdef __AES__

// Requires, AES support, which I think most CPUs these days have?
inline
uint64_t fast_128bit_input_hash_3(const void* data) {

    __m128i value = _mm_lddqu_si128((const __m128i*)data);
    __m128i s0 = _mm_aesenc_si128(value, value);
    __m128i s1 = _mm_aesdec_si128(s0, value);
    return _mm_extract_epi64(s1, 0);

}

#endif
