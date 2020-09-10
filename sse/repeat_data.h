#pragma once

#include <cstdint>

#if defined(_MSC_VER) && defined(_M_X64)
    #include <intrin.h>

#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    #include <x86intrin.h>

#else
    #error "Only really intended for x64 gcc/clang/msc"
#endif


// TODO need to fully test this


// Basically, figure out how many times we need to
// repeat an element, to fit inside another data type
// (allowing for the other data type to also repeat)
template <uint32_t element_size, uint32_t fit_size>
uint32_t constexpr repeats_needed_to_fit() {
    uint32_t i = 1;

    // TODO: Do this analytically.
    while( (i*element_size) % fit_size ) {
        ++i;
    }

    return i;
}


template <typename T>
struct m128_repeater {
    const static uint32_t t_repeats_needed = repeats_needed_to_fit<sizeof(T), 16>();
    const static uint32_t m128_repeats = (t_repeats_needed * sizeof(T)) / 16;

    // Returns back how many elements were repeated
    static inline uint32_t try_to_repeat(const T& data, T* output, const uint32_t n) {

        // Not enough entries to repeat
        if( n < t_repeats_needed ) return 0;
        // Too many registers would be used
        if( m128_repeats > 16 ) return 0;


        // Start off by filling up a temporary buffer
        // and loading that into the registers
        T temp_buffer[t_repeats_needed];
        __m128i registers[m128_repeats];

        for(uint32_t i=0; i<t_repeats_needed; ++i) {
            temp_buffer[i] = data;
        }
        for(uint32_t i=0; i<m128_repeats; ++i) {
            __m128i* ptr = (__m128i*)&temp_buffer[0];
            registers[i] = _mm_lddqu_si128(ptr + i);
        }


        // Start copying
        const uint32_t iterations_needed = n / t_repeats_needed;

        __m128i* out_ptr = (__m128i*)output;

        uint32_t i=0;

        // Attempt to do a 8 at a time for as long as we can
        for(; i<(iterations_needed & -8u); i+=8, out_ptr+=8*m128_repeats) {
            for(uint32_t j=0; j<m128_repeats; ++j) {
                _mm_storeu_si128(out_ptr + m128_repeats*0 + j, registers[j]);
            }
            for(uint32_t j=0; j<m128_repeats; ++j) {
                _mm_storeu_si128(out_ptr + m128_repeats*1 + j, registers[j]);
            }
            for(uint32_t j=0; j<m128_repeats; ++j) {
                _mm_storeu_si128(out_ptr + m128_repeats*2 + j, registers[j]);
            }
            for(uint32_t j=0; j<m128_repeats; ++j) {
                _mm_storeu_si128(out_ptr + m128_repeats*3 + j, registers[j]);
            }
            for(uint32_t j=0; j<m128_repeats; ++j) {
                _mm_storeu_si128(out_ptr + m128_repeats*4 + j, registers[j]);
            }
            for(uint32_t j=0; j<m128_repeats; ++j) {
                _mm_storeu_si128(out_ptr + m128_repeats*5 + j, registers[j]);
            }
            for(uint32_t j=0; j<m128_repeats; ++j) {
                _mm_storeu_si128(out_ptr + m128_repeats*6 + j, registers[j]);
            }
            for(uint32_t j=0; j<m128_repeats; ++j) {
                _mm_storeu_si128(out_ptr + m128_repeats*7 + j, registers[j]);
            }
        }

        // Do the remainder
        for(; i<iterations_needed; ++i, out_ptr+=m128_repeats) {
            for(uint32_t j=0; j<m128_repeats; ++j) {
                _mm_storeu_si128(out_ptr + j, registers[j]);
            }
        }

        // Return back how many things were copied
        return t_repeats_needed * iterations_needed;
    }
};


template<typename T>
void repeat_data(const T& data, T* output, const uint32_t count) {

    // Attempt to copy as many as we can in blocks
    uint32_t i = m128_repeater<T>::try_to_repeat(data, output, count);

    // Do the rest 1 by 1
    for(; i<count; ++i) {
        output[i] = data;
    }

}
