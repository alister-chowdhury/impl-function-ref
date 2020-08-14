#pragma once

#include <cstdint>
#include <x86intrin.h>


// For reference:
// https://www.geogebra.org/m/ytnhzgzb


struct alignas(16) packed_lines {

    float x0[4];
    float y0[4];
    float x1[4];
    float y1[4];

};


struct alignas(16) line {

    float x0, y0, x1, y1;

};


// Need to test this properly (should also be possible to do a 4x4 test, returning a uint64)
inline
uint8_t test_intersection_4(const line& ab, const packed_lines& cd) {

    const __m128 a_x = _mm_set1_ps( ab.x0 );
    const __m128 a_y = _mm_set1_ps( ab.y0 );
    const __m128 b_x = _mm_set1_ps( ab.x1 );
    const __m128 b_y = _mm_set1_ps( ab.y1 );
    
    const __m128 c_x = _mm_load_ps( (const float*)&cd.x0 );
    const __m128 c_y = _mm_load_ps( (const float*)&cd.y0 );
    const __m128 d_x = _mm_load_ps( (const float*)&cd.x1 );
    const __m128 d_y = _mm_load_ps( (const float*)&cd.y1 );

    // AB = A - B (could be pre stored in the lines directly, 
    // instead of B, as a further opt)
    const __m128 ab_x = a_x - b_x;
    const __m128 ab_y = a_y - b_y;

    // CD = C - D (could be pre stored in the lines directly, 
    // instead of D, as a further opt)
    const __m128 cd_x = c_x - d_x;
    const __m128 cd_y = c_y - d_y;

    const __m128 ac_x = a_x - c_x;
    const __m128 ac_y = a_y - c_y;

    // d = cross(AB, CD)
    const __m128 norm_factor = ab_x * cd_y - ab_y * cd_x;

    // u = cross(AC, AB) * sign(d)
    //     cross(AC, AB) ^ first bit of d
    // t = cross(AC, CD) * sign(d)
    //     cross(AC, CD) ^ first bit of d
    const __m128 abs_mask = _mm_castsi128_ps(
        _mm_set1_epi32(0x80000000)
    );
    const __m128 norm_factor_sign = _mm_and_ps(
        norm_factor,
        abs_mask
    );
    const __m128 u = _mm_xor_ps(ac_x * ab_y - ac_y * ab_x, norm_factor_sign);
    const __m128 t = _mm_xor_ps(ac_x * cd_y - ac_y * cd_x, norm_factor_sign);


    // in_lower_bounds = min(u, t) > 0
    // in_upper_bounds = max(u, t) < abs(norm_factor)
    const __m128 abs_norm_factor = _mm_andnot_ps(
        norm_factor,
        abs_mask
    );
    const __m128 in_lower_bounds = _mm_cmpgt_ps(_mm_min_ps(u, t), _mm_setzero_ps());
    const __m128 in_upper_bounds = _mm_cmplt_ps(_mm_max_ps(u, t), abs_norm_factor);

    return _mm_movemask_ps(_mm_and_ps(in_lower_bounds, in_upper_bounds));
}
