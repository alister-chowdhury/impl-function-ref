#pragma once

#include <cstdint>

#if defined(_MSC_VER) && defined(_M_X64)
    #include <intrin.h>

#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    #include <x86intrin.h>

#else
    #error "Only really intended for x64 gcc/clang/msc"
#endif



struct vec4
{
    __m128 native;
};

struct mat4x4
{
    mat4x4 operator*(const mat4x4 other) const
    {
        mat4x4 result;
        __m128 c0, c1, c2, c3;

        #define ROW_MUL_HELPER(rowX)\
        c0 = _mm_shuffle_ps(rowX, rowX, 0b00000000);\
        c1 = _mm_shuffle_ps(rowX, rowX, 0b01010101);\
        c2 = _mm_shuffle_ps(rowX, rowX, 0b10101010);\
        c3 = _mm_shuffle_ps(rowX, rowX, 0b11111111);\
        result. rowX = _mm_add_ps(\
            _mm_add_ps(_mm_mul_ps(c0, other.row0), _mm_mul_ps(c1, other.row1)),\
            _mm_add_ps(_mm_mul_ps(c2, other.row2), _mm_mul_ps(c3, other.row3))\
        );
        ROW_MUL_HELPER(row0);
        ROW_MUL_HELPER(row1);
        ROW_MUL_HELPER(row2);
        ROW_MUL_HELPER(row3);
        #undef ROW_MUL_HELPER

        return result;
    }


    vec4 operator*(const vec4 other) const
    {
        __m128 c0 = _mm_shuffle_ps(other.native, other.native, 0b00000000);
        __m128 c1 = _mm_shuffle_ps(other.native, other.native, 0b01010101);
        __m128 c2 = _mm_shuffle_ps(other.native, other.native, 0b10101010);
        __m128 c3 = _mm_shuffle_ps(other.native, other.native, 0b11111111);
        __m128 result = _mm_add_ps(
            _mm_add_ps(_mm_mul_ps(c0, row0), _mm_mul_ps(c1, row1)),
            _mm_add_ps(_mm_mul_ps(c2, row2), _mm_mul_ps(c3, row3))
        );
        return { result };
    }

    __m128 row0,
           row1,
           row2,
           row3;
};

