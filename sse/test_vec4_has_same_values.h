#pragma once

#include <cstdint>
#include <x86intrin.h>


struct alignas(16) vec4i {

    uint32_t x, y, z, w;

};


inline
bool test_vec4_has_same_value(const vec4i* v) {

    const __m128i all = _mm_loadu_si128((const __m128i*)v);
    const __m128i first = _mm_shuffle_epi32(all, 0);
    const __m128 equal = _mm_castsi128_ps(_mm_cmpeq_epi32(all, first));
    return _mm_movemask_ps(equal) == 0b1111;

}
