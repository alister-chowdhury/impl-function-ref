#pragma once

#if defined(_MSC_VER) && defined(_M_X64)
    #include <intrin.h>

#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    #include <x86intrin.h>

#else
    #error "Only really intended for x64 gcc/clang/msc"
#endif


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
void unpack_vec3s(vec3* vec3s, const packed4_vec3* ins) {

        // Ax Bx Cx Dx
        // Ay By Cy Dy
        // Az Bz Cz Dz
        const __m128 row_1 = _mm_loadu_ps((const float*)&ins->x);
        const __m128 row_2 = _mm_loadu_ps((const float*)&ins->y);
        const __m128 row_3 = _mm_loadu_ps((const float*)&ins->z);

        // Ax Dx Az Dz
        // By Ay Cx Bx
        // Cz Bz Dy Cy
        const __m128 R0 = _mm_shuffle_ps(row_1, row_3, 0b11001100);
        const __m128 R1 = _mm_shuffle_ps(row_2, row_1, 0b01100001);
        const __m128 R2 = _mm_shuffle_ps(row_3, row_2, 0b10110110);

        // Ax, Ay, Az, Bx
        // By, Bz, Cx, Cy
        // Cz, Dx, Dy, Dz
        const __m128 out_0 = _mm_blend_ps(R0, R1, 0b1010);
        const __m128 out_1 = _mm_blend_ps(R1, R2, 0b1010);
        const __m128 out_2 = _mm_blend_ps(R2, R0, 0b1010);

        float* vec3_f = (float*)vec3s;
        _mm_store_ps( vec3_f, out_0);
        _mm_store_ps( vec3_f + 4, out_1);
        _mm_store_ps( vec3_f + 8, out_2);
}


inline
void pack4_vec3s_inplace(vec3* vec3s) {
    pack4_vec3s(vec3s, (packed4_vec3*)vec3s);
}

