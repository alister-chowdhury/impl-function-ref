#pragma once

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


// Given a point P which lies within the triangle ABC
// Solve (u,v) where: P = A + u (B - A) + v (C - A)
//
// I came up with this to help me resolve Embrees
// u and v paramaters, when calling rtcInterpolate.
// (When I had a texcoord, and really wanted to get
// what P or N lied on that texcoord).
//
// Additionally the point P is in/on ABC iff:
// (u >= 0) and (v >= 0) and (u + v <= 1)
inline
vec2 find_parametrization_on_triangle_2d(
    vec2 A,
    vec2 B,
    vec2 C,
    vec2 P
) {

    // p = a + u (b-a) + v (c-a)
    //
    // | [bx-ax] [cx-ax] |^-1   | [px-ax] |
    // | [by-ay] [cy-ay] |    * | [py-ay] | = | u v |


    // Non sse version for reference
#if 0

    vec2 BA = vec2{ B.x-A.x, B.y-A.y };
    vec2 CA = vec2{ C.x-A.x, C.y-A.y };
    vec2 PA = vec2{ P.x-A.x, P.y-A.y };

    const float inv_det = 1.0 / (BA.x * CA.y - BA.y * CA.x );

    const float M00 =  inv_det * CA.y;
    const float M10 = -inv_det * CA.x;
    const float M01 = -inv_det * BA.y;
    const float M11 =  inv_det * BA.x;

    const float u = M00 * PA.x + M10 * PA.y;
    const float v = M01 * PA.x + M11 * PA.y;

    return vec2{ u, v };

#else

    __m128 a_vec = _mm_castpd_ps(_mm_loaddup_pd( (double*)&A ));
    __m128 p_vec = _mm_castpd_ps(_mm_loaddup_pd( (double*)&P ));

    __m128 bc = _mm_movelh_ps(
        _mm_castpd_ps(_mm_loaddup_pd( (double*)&B )),
        _mm_castpd_ps(_mm_loaddup_pd( (double*)&C ))
    );
    
    // BA.x BA.y CA.x CA.y
    // CA.y CA.x BA.y BA.x
    __m128 bc_a_1 = _mm_sub_ps( bc, a_vec );
    __m128 bc_a_2 = _mm_shuffle_ps( bc_a_1, bc_a_1, 0b00011011);

    // PA.x PA.y PA.x PA.y
    __m128 p_a = _mm_sub_ps(p_vec, a_vec);

    // BA.x*CA.y BA.y*CA.x BA.y*CA.x BA.x*CA.y
    // BA.y*CA.x BA.x*CA.y BA.x*CA.y BA.y*CA.x
    // det, -det, det, -det
    // 1/det, -1/det, 1/det, -1/det
    __m128 det_0 = _mm_mul_ps( bc_a_1, bc_a_2 );
    __m128 det_1 = _mm_shuffle_ps(det_0, det_0, 0b01000001 );
    __m128 det = _mm_sub_ps( det_0, det_1 );
    __m128 inv_det = _mm_rcp_ps( det );

    // M00 M10 M01 M11
    __m128 M = _mm_mul_ps( bc_a_2, inv_det );
    
    // M00 * PA.x, M10 * PA.y, M01 * PA.x, M11 * PA.y
    __m128 uv_parts = _mm_mul_ps( M, p_a );

    // uv[0] = M00 * PA.x + M10 * PA.y
    // uv[1] = M01 * PA.x + M11 * PA.y
    __m128 uv = _mm_hadd_ps( uv_parts, uv_parts );

    vec2 ruv;
    *((double*)&ruv) = _mm_cvtsd_f64( _mm_castps_pd(uv) );

    return ruv; 
#endif

}
