#pragma once

#include <x86intrin.h>


struct vec2 {
    float x, y;
};

struct alignas(16) packed4_vec2 {

    float x[4];
    float y[4];

};


inline
void pack4_vec2s(const vec2* vec2s, packed4_vec2* out) {
    // Ax, Ay, Bx, By
    // Cx, Cy, Dx, Dy
    const __m128 row_1 = _mm_loadu_ps( (const float*) vec2s );
    const __m128 row_2 = _mm_loadu_ps( (const float*) &vec2s[2] );

    // Ax Bx Cx Dx
    // Ay By Cy Dy
    const __m128 x = _mm_shuffle_ps( row_1, row_2, 0b10001000);
    const __m128 y = _mm_shuffle_ps( row_1, row_2, 0b11011101);

    _mm_store_ps((float*)&out->x, x);
    _mm_store_ps((float*)&out->y, y);
}


inline
void pack4_vec2s_inplace(vec2* vec2s) {
    pack4_vec2s(vec2s, (packed4_vec2*)vec2s);
}
