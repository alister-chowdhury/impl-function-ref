#pragma once

#include <cstdint>

#if defined(_MSC_VER) && defined(_M_X64)
    #include <intrin.h>

#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    #include <x86intrin.h>

#else
    #error "Only really intended for x64 gcc/clang/msc"
#endif


struct vec2 {
    float x, y;
};


struct vec2i {
    int32_t x, y;
};


// Only really clang seems to optimize the naive version in a reasonable way.
inline
void uv_to_udim_ids(const vec2* src, vec2i* dst, const uint32_t count) {

    uint32_t written = 0;

    // Do 8 at a time
    for( ; written < (count & -8) ; written += 8 ) {

        const __m128 R0 = _mm_floor_ps(_mm_loadu_ps( (const float*)&src[written+0] ));
        const __m128 R1 = _mm_floor_ps(_mm_loadu_ps( (const float*)&src[written+2] ));
        const __m128 R2 = _mm_floor_ps(_mm_loadu_ps( (const float*)&src[written+4] ));
        const __m128 R3 = _mm_floor_ps(_mm_loadu_ps( (const float*)&src[written+6] ));

        _mm_storeu_si128((__m128i*)&dst[written+0], _mm_cvtps_epi32( R0 ));
        _mm_storeu_si128((__m128i*)&dst[written+2], _mm_cvtps_epi32( R1 ));
        _mm_storeu_si128((__m128i*)&dst[written+4], _mm_cvtps_epi32( R2 ));
        _mm_storeu_si128((__m128i*)&dst[written+6], _mm_cvtps_epi32( R3 ));

    }

    // Do the rest 1 at a time
    for(; written < (count & 8) ; ++written ) {

        __m128 uv = _mm_castpd_ps(_mm_load_sd((const double*)&src[written]));
        const __m128 udim = _mm_floor_ps(uv);
        _mm_storel_epi64((__m128i*)&dst[written], _mm_cvtps_epi32( udim ));

    }

}


inline
bool uvs_to_udim_ids_and_test_for_same_udim(const vec2* src, vec2i* dst, const uint32_t count) {

    // If there is nothing to stream, really there is a case for both the udims all being
    // the same, and them not being the same.
    if( !count ) return true;

    uint32_t written = 0;

    // Start off with a flag = true, and do an initial calculation of the first udim
    __m128 same_flag = _mm_castsi128_ps(_mm_set1_epi32(0xffffffff));
    __m128 first_udim = _mm_floor_ps(_mm_castpd_ps(_mm_load1_pd((const double*)&src[0])));

    // Do 8 at a time
    for( ; written < (count & -8) ; written += 8 ) {

        const __m128 R0 = _mm_floor_ps(_mm_loadu_ps( (const float*)&src[written+0] ));
        const __m128 R1 = _mm_floor_ps(_mm_loadu_ps( (const float*)&src[written+2] ));
        const __m128 R2 = _mm_floor_ps(_mm_loadu_ps( (const float*)&src[written+4] ));
        const __m128 R3 = _mm_floor_ps(_mm_loadu_ps( (const float*)&src[written+6] ));

        _mm_storeu_si128((__m128i*)&dst[written+0], _mm_cvtps_epi32( R0 ));
        _mm_storeu_si128((__m128i*)&dst[written+2], _mm_cvtps_epi32( R1 ));
        _mm_storeu_si128((__m128i*)&dst[written+4], _mm_cvtps_epi32( R2 ));
        _mm_storeu_si128((__m128i*)&dst[written+6], _mm_cvtps_epi32( R3 ));

        same_flag = _mm_and_ps( same_flag, _mm_cmpeq_ps( first_udim, R0 ) );
        same_flag = _mm_and_ps( same_flag, _mm_cmpeq_ps( first_udim, R1 ) );
        same_flag = _mm_and_ps( same_flag, _mm_cmpeq_ps( first_udim, R2 ) );
        same_flag = _mm_and_ps( same_flag, _mm_cmpeq_ps( first_udim, R3 ) );

    }

    // Do the rest 1 at a time
    for(; written < (count & 8) ; ++written ) {

        __m128 uv = _mm_castpd_ps(_mm_load_sd((const double*)&src[written]));
        const __m128 udim = _mm_floor_ps(uv);
        _mm_storel_epi64((__m128i*)&dst[written], _mm_cvtps_epi32( udim ));

        same_flag = _mm_and_ps( same_flag, _mm_cmpeq_ps( first_udim, udim ) );
    }


    // All udims ended up being the same
    return ( _mm_movemask_ps( same_flag ) == 0b1111 );
}


// returns [uf32, vf32] packed (for use in a set etc)
inline
uint64_t uv_to_packed_udim(const vec2* uv) {
    __m128 udim = _mm_floor_ps(_mm_castpd_ps(_mm_load_sd((const double*)uv)));
    return (uint64_t)_mm_cvtsi128_si64(_mm_castps_si128(udim));
}
