#pragma once

#include <x86intrin.h>


struct vec3 {
    float x, y, z;
};

struct alignas(16) packed4_vec3 {

    float x[4];
    float y[4];
    float z[4];

};


inline
void pack4_vec3s(const vec3* vec3s, packed4_vec3* out) {

    // Ax, Ay, Az, Bx
    // By, Bz, Cx, Cy
    // Cz, Dx, Dy, Dz
    const float* vec3_f = (const float*)vec3s;
    const __m128 row_1 = _mm_loadu_ps( vec3_f );
    const __m128 row_2 = _mm_loadu_ps( &vec3_f[4] );
    const __m128 row_3 = _mm_loadu_ps( &vec3_f[8] );

    // Ax Ay Cx Cy
    // By Bz Dy Dz
    // Az Bx Cz Dx
    const __m128 R0 = _mm_blend_ps(row_1, row_2, 0b1100);
    const __m128 R1 = _mm_blend_ps(row_2, row_3, 0b1100);
    const __m128 R2 = _mm_shuffle_ps(row_1, row_3, 0b01001110);

    // By Ay Dy Cy
    const __m128 R3 = _mm_blend_ps(R1, R0, 0b1010);

    // Ax Bx Cx Dx
    // Ay By Cy Dy
    // Az Bz Cz Dz
    const __m128 x = _mm_blend_ps(R0, R2, 0b1010);
    const __m128 y = _mm_shuffle_ps(R3, R3, 0b10110001);
    const __m128 z = _mm_blend_ps(R2, R1, 0b1010);

    _mm_store_ps((float*)&out->x, x);
    _mm_store_ps((float*)&out->y, y);
    _mm_store_ps((float*)&out->z, z);
}


inline
void pack4_vec3s_inplace(vec3* vec3s) {
    pack4_vec3s(vec3s, (packed4_vec3*)vec3s);
}
