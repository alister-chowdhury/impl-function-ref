#pragma once

#include <cstdint>
#include <x86intrin.h>


// Note: All input floats assumed to be in range of [0, 1]
// anything outside this, will lead to aliasing between channels.


/* 'pack_vec3.h' */
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

/* 'pack_vec3.h' */

struct vec4 {
    float x, y, z, w;
};


struct rgb10a2 {
    uint32_t r:10;
    uint32_t g:10;
    uint32_t b:10;
    uint32_t a:2;
};



// RGBf -> RGB10A2 functions

inline
void rgbf_to_rgb10a2( const vec3& in_pix, rgb10a2& out_pix) {

    // Basically:
    // out_pix.r = uint32_t(in_pix.x * 1023);
    // out_pix.g = uint32_t(in_pix.y * 1023);
    // out_pix.b = uint32_t(in_pix.z * 1023);
    //
    // But without the additional & operations that would be
    // required.
    uint32_t* rgb10a2_packed = ((uint32_t*)&out_pix);
    *rgb10a2_packed = (
        (uint32_t(in_pix.x * 1023) << 20)
        | (uint32_t(in_pix.y * 1023) << 10)
        | uint32_t(in_pix.z * 1023)
    );

}


inline
void rgbf_to_rgb10a2( const vec3* in_pixels, rgb10a2* out_pixels, const uint32_t size) {

    const vec3* in_iter = in_pixels;
    const vec3* in_end = in_iter + size;
    const vec3* in_end4 = in_iter + size / 4;

    rgb10a2* out_iter = out_pixels;

    // Attempting to use (1023 << 2), (1023 << 12) etc off the bat
    // leads to bit aliasing, so for now, we're having to a mul and a
    // shift, rather than just a mul.
    __m128 vec_1023 = _mm_set1_ps(1023);

    // Do 4 pixels at a time
    for(; in_iter < in_end4 ; in_iter+=4, out_iter+=4 ) {

        packed4_vec3 packed_rgb;
        pack4_vec3s( in_iter, &packed_rgb );

        __m128 R = _mm_load_ps( (float*)&packed_rgb.x );
        __m128 G = _mm_load_ps( (float*)&packed_rgb.y );
        __m128 B = _mm_load_ps( (float*)&packed_rgb.z );

        R = _mm_mul_ps(R, vec_1023);
        G = _mm_mul_ps(G, vec_1023);
        B = _mm_mul_ps(B, vec_1023);

        // (uint32(R*1023) << 20)
        // | (uint32(G*1023) << 10)
        // | uint32(B*1023)
        __m128i Ri = _mm_slli_epi32(_mm_cvtps_epi32(R), 20);
        __m128i Gi = _mm_slli_epi32(_mm_cvtps_epi32(G), 10);
        __m128i Bi = _mm_cvtps_epi32(B);

        __m128i rgb10a2_vec = _mm_or_si128(_mm_or_si128(Ri, Bi), Gi);

        // Not going to assume the output is aligned, even though it
        // probably should be.
        _mm_storeu_si128((__m128i*)out_iter, rgb10a2_vec);

    }

    // Do whatevers left over
    for(; in_iter < in_end ; ++in_iter, ++out_iter) {
        rgbf_to_rgb10a2(*in_iter, *out_iter);
    }

}


// RGBAf -> RGB10A2 functions

inline
void rgbaf_to_rgb10a2( const vec4& in_pix, rgb10a2& out_pix) {

    // Basically:
    // out_pix.r = uint32_t(in_pix.x * 1023);
    // out_pix.g = uint32_t(in_pix.y * 1023);
    // out_pix.b = uint32_t(in_pix.z * 1023);
    // out_pix.a = uint32_t(in_pix.w * 3);
    //
    // But without the additional & operations that would be
    // required.
    uint32_t* rgb10a2_packed = ((uint32_t*)&out_pix);
    *rgb10a2_packed = (
        (uint32_t(in_pix.x * 1023) << 20)
        | (uint32_t(in_pix.y * 1023) << 10)
        | uint32_t(in_pix.z * 1023)
        | (uint32_t(in_pix.w * 3) << 30)
    );

}


inline
void rgbaf_to_rgb10a2( const vec4* in_pixels, rgb10a2* out_pixels, const uint32_t size) {

    const vec4* in_iter = in_pixels;
    const vec4* in_end = in_iter + size;
    const vec4* in_end4 = in_iter + size / 4;

    rgb10a2* out_iter = out_pixels;

    __m128 vec_1023 = _mm_set1_ps(1023);
    __m128 vec_3 = _mm_set1_ps(3);


    // Do 4 pixels at a time
    for(; in_iter < in_end4 ; in_iter+=4, out_iter+=4 ) {

        // Load as RGBA, RGBA etc, then instantly transpose correctly
        __m128 R = _mm_loadu_ps( (float*)&in_iter[0] );
        __m128 G = _mm_loadu_ps( (float*)&in_iter[1] );
        __m128 B = _mm_loadu_ps( (float*)&in_iter[2] );
        __m128 A = _mm_loadu_ps( (float*)&in_iter[3] );

        _MM_TRANSPOSE4_PS(R, G, B, A);

        R = _mm_mul_ps(R, vec_1023);
        G = _mm_mul_ps(G, vec_1023);
        B = _mm_mul_ps(B, vec_1023);
        A = _mm_mul_ps(A, vec_3);

        // (uint32(R*1023) << 20)
        // | (uint32(G*1023) << 10)
        // | uint32(B*1023)
        // | (uint32(A*3) << 30)
        __m128i Ri = _mm_slli_epi32(_mm_cvtps_epi32(R), 20);
        __m128i Gi = _mm_slli_epi32(_mm_cvtps_epi32(G), 10);
        __m128i Bi = _mm_cvtps_epi32(B);
        __m128i Ai = _mm_slli_epi32(_mm_cvtps_epi32(A), 30);

        __m128i rgb10a2_vec = _mm_or_si128(_mm_or_si128(Ri, Bi), _mm_or_si128(Gi, Ai));

        // Not going to assume the output is aligned, even though it
        // probably should be.
        _mm_storeu_si128((__m128i*)out_iter, rgb10a2_vec);

    }

    // Do whatevers left over
    for(; in_iter < in_end ; ++in_iter, ++out_iter) {
        rgbaf_to_rgb10a2(*in_iter, *out_iter);
    }

}
