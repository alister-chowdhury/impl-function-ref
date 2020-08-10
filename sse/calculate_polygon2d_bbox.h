#pragma once

#include <x86intrin.h>


struct alignas(16) bbox {
    float min_x, min_y,
          max_x, max_y;
};


struct vec2 {
    float x, y;
};


inline
void calculate_quad_bbox(const vec2& a, const vec2& b, const vec2& c, const vec2& d, bbox* out_bbox) {

    // Load vec2s into registers in the format of: (X, Y, X, Y)
    const __m128 a0 = _mm_castpd_ps(_mm_load1_pd((const double*)&a));
    const __m128 b0 = _mm_castpd_ps(_mm_load1_pd((const double*)&b));
    const __m128 c0 = _mm_castpd_ps(_mm_load1_pd((const double*)&c));
    const __m128 d0 = _mm_castpd_ps(_mm_load1_pd((const double*)&d));

    const __m128 mins = _mm_min_ps( _mm_min_ps(a0, b0), _mm_min_ps(c0, d0) );
    const __m128 maxs = _mm_max_ps( _mm_max_ps(a0, b0), _mm_max_ps(c0, d0) );

    // [MinX, MinY, MaxX, MaxY]
    const __m128 blended = _mm_blend_ps(mins, maxs, 0b1100);

    _mm_store_ps((float*)out_bbox, blended);
}


inline
void calculate_tri_bbox(const vec2& a, const vec2& b, const vec2& c, bbox* out_bbox) {

    // Load vec2s into registers in the format of: (X, Y, X, Y)
    const __m128 a0 = _mm_castpd_ps(_mm_load1_pd((const double*)&a));
    const __m128 b0 = _mm_castpd_ps(_mm_load1_pd((const double*)&b));
    const __m128 c0 = _mm_castpd_ps(_mm_load1_pd((const double*)&c));

    const __m128 mins = _mm_min_ps( _mm_min_ps(a0, b0), c0 );
    const __m128 maxs = _mm_max_ps( _mm_max_ps(a0, b0), c0 );

    // [MinX, MinY, MaxX, MaxY]
    const __m128 blended = _mm_blend_ps(mins, maxs, 0b1100);

    _mm_store_ps((float*)out_bbox, blended);
}
