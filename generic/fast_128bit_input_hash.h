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
