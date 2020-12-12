#pragma once

#if defined(_MSC_VER) && defined(_M_X64)
    #include <intrin.h>

#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    #include <x86intrin.h>

#else
    #error "Only really intended for x64 gcc/clang/msc"
#endif



inline
int log10(uint32_t value) {

    alignas(16) const uint32_t  r0[4]= { 0, 9, 99, 999 };
    alignas(16) const uint32_t  r1[4]= { 9999, 99999, 999999, 9999999 };
    alignas(16) const uint32_t  r2[4]= { 99999999, 999999999, 0x7fffffff, 0x7fffffff };

    __m128i base = _mm_set1_epi32(value);

    __m128i result = _mm_add_epi32(
        _mm_add_epi32(
            _mm_cmpgt_epi32(base, _mm_load_si128((const __m128i*)r0)),
            _mm_cmpgt_epi32(base, _mm_load_si128((const __m128i*)r1))
        ),
        _mm_cmpgt_epi32(base, _mm_load_si128((const __m128i*)r2))
    );

    #if 1
        alignas(16) int buffer[4];
        _mm_store_si128((__m128i*)buffer, result);
        int ret = ~(buffer[0] + buffer[1] + buffer[2] + buffer[3]);

    // Both GCC and Clang really struggle to precompute the below
    #else
        result = _mm_hadd_epi32(result, result);
        result = _mm_hadd_epi32(result, result);    
        int ret = ~_mm_extract_epi32(result, 0);
    #endif

    // Handle a lack of unsigned compare AFTER computation, to stop
    // the generated opcodes from being crappy due to it favouring an
    // early exit (on clang atleast, gcc still put it at the top :()
    if(value > 0x7fffffff) { ret = 9; }
    return ret;

}


// The no-sse version in generic seems to be faster
inline
char* uint2str(const unsigned int x, char* out)
{

    char b[10];
    int l10 = log10(x);
    if(l10 == -1) { l10 = 0; }

    // Basically move the pointer were going to write to
    // to be relative to log10(x), so we can simply fallthrough it.
    char* ptr = out - 9 + l10;

    switch(l10) {
        case 9:
            ptr[0] = (x/1000000000) % 10 + '0';
        case 8:
            ptr[1] = (x/100000000) % 10 + '0';
        case 7:
            ptr[2] = (x/10000000) % 10 + '0';
        case 6:
            ptr[3] = (x/1000000) % 10 + '0';
        case 5:
            ptr[4] = (x/100000) % 10 + '0';
        case 4:
            ptr[5] = (x/10000) % 10 + '0';
        case 3:
            ptr[6] = (x/1000) % 10 + '0';
        case 2:
            ptr[7] = (x/100) % 10 + '0';
        case 1:
            ptr[8] = (x/10) % 10 + '0';
        case 0:
            ptr[9] = x % 10 + '0';
            break;
        default:
            __builtin_unreachable();
    }

    return out + l10;
}
