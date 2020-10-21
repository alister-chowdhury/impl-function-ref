#pragma once

#include <cstdint>

#if defined(_MSC_VER) && defined(_M_X64)
    #include <intrin.h>

#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    #include <x86intrin.h>

#else
    #error "Only really intended for x64 gcc/clang/msc"
#endif


inline
void ascii_tolower_inplace(char* data, const uint32_t count) {

    uint32_t i = 0;

    // Do one xmm register at a time
    if(count >= 16) {

        // There is no cmple or cmpge intrinsic for epi8
        const __m128i A = _mm_set1_epi8('A' - 1);
        const __m128i Z = _mm_set1_epi8('Z' + 1);
        const __m128i space = _mm_set1_epi8(' ');

        for(; i < (count & (0u - 16u)); i+= 16) {

            __m128i block = _mm_lddqu_si128((__m128i*)&data[i]);
            __m128i in_range = _mm_and_si128(_mm_cmpgt_epi8(block, A), _mm_cmplt_epi8(block, Z));

            const bool any_capitals = _mm_testc_si128(in_range, in_range);

            if(any_capitals) {
                __m128i lowered = _mm_add_epi8(block, space);
                __m128i result = _mm_blendv_epi8(block, lowered, in_range);
                _mm_storeu_si128((__m128i*)&data[i], result);
            }
        }
    }

    // Do the remaining
    for(; i < count ; ++i) {
        char c = data[i];
        if( c >= 'A' && c <= 'Z') {
            data[i] = c + ' ';
        }
    }
}


inline
void ascii_tolower(char* dst, const char* src, const uint32_t count) {

    if(dst == src) {
        ascii_tolower_inplace(dst, count);
        return;
    }

    uint32_t i = 0;

    // Do one xmm register at a time
    if(count >= 16) {

        // There is no cmple or cmpge intrinsic for epi8
        const __m128i A = _mm_set1_epi8('A' - 1);
        const __m128i Z = _mm_set1_epi8('Z' + 1);
        const __m128i space = _mm_set1_epi8(' ');

        for(; i < (count & (0u - 16u)); i+= 16) {

            __m128i block = _mm_lddqu_si128((__m128i*)&src[i]);
            __m128i in_range = _mm_and_si128(_mm_cmpgt_epi8(block, A), _mm_cmplt_epi8(block, Z));
            
            const bool any_capitals = _mm_testc_si128(in_range, in_range);

            if(any_capitals) {
                __m128i lowered = _mm_add_epi8(block, space);
                block = _mm_blendv_epi8(block, lowered, in_range);
            }

            _mm_storeu_si128((__m128i*)&dst[i], block);

        }
    }

    // Do the remaining
    for(; i < count ; ++i) {
        char c = src[i];
        if( (c >= 'A') && (c <= 'Z') ) {
            c = c + ' ';
        }
        dst[i] = c;
    }

}
