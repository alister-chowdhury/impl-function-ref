#pragma once

#include <cstdint>

#if defined(_MSC_VER) && defined(_M_X64)
    #include <intrin.h>

#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    #include <x86intrin.h>


// TODO NEED TO TEST THIS PROPERLY!!!

// Note: All inputs are assumed to have widths and heights that are
// multiples of 4.
// Black and white input are assumed to be uint8_t and have a value of
// either 0, 1 or 255.


struct bc1_4x4 {

    // End points (4bytes)
    uint64_t s_r:5;
    uint64_t s_g:6;
    uint64_t s_b:5;
    
    uint64_t e_r:5;
    uint64_t e_g:6;
    uint64_t e_b:5;

    // Indicies (4bytes)
    uint64_t i0x0:2;
    uint64_t i0x1:2;
    uint64_t i0x2:2;
    uint64_t i0x3:2;

    uint64_t i1x0:2;
    uint64_t i1x1:2;
    uint64_t i1x2:2;
    uint64_t i1x3:2;

    uint64_t i2x0:2;
    uint64_t i2x1:2;
    uint64_t i2x2:2;
    uint64_t i2x3:2;

    uint64_t i3x0:2;
    uint64_t i3x1:2;
    uint64_t i3x2:2;
    uint64_t i3x3:2;

};


// Basically reduce:
// 0x0000001 -> 0b00000001
// 0x0000100 -> 0b00000100
// 0x0010000 -> 0b00010000
// 0x0100000 -> 0b01000000
// 0x0100001 -> 0b01000001
// etc
inline
uint32_t bw_pack_32_to_8(const uint32_t x) {

    uint32_t v = x & 0x01010101;
    v |= v >> 6;
    v |= v >> 12;
    v &= 0xff;
    return v;

}


// Similar to the above, but we first pack
// each register of 4 32bit numbers into a single
// 32bit number.
// Then we interlace those values.
// Additionally, unlike the above function, we also
// perform a (x |= x << 1) operation
// The inputs a, b, c and d are expected to be as followed:
// a = A[0, 0:3], B[0, 0:3], C[0, 0:3], D[0, 0:3]
// b = A[1, 0:3], B[1, 0:3], C[1, 0:3], D[1, 0:3]
// c = A[2, 0:3], B[2, 0:3], C[2, 0:3], D[2, 0:3]
// d = A[3, 0:3], B[3, 0:3], C[3, 0:3], D[3, 0:3]
// Basically load a, b, c and d from different Y values
// of an image, when there are atleast 4 blocks to encode
inline
__m128i bw_pack_32_to_8_vec(
    __m128i a,
    __m128i b,
    __m128i c,
    __m128i d
) {

    // Convience macro for transposing __128i data
    #define TRANSPOSE4_EPI32(r0, r1, r2, r3) do { \
            __m128 r0f = _mm_castsi128_ps(r0);    \
            __m128 r1f = _mm_castsi128_ps(r1);    \
            __m128 r2f = _mm_castsi128_ps(r2);    \
            __m128 r3f = _mm_castsi128_ps(r3);    \
            _MM_TRANSPOSE4_PS(r0f, r1f, r2f, r3f);\
            r0 = _mm_castps_si128(r0f);           \
            r1 = _mm_castps_si128(r1f);           \
            r2 = _mm_castps_si128(r2f);           \
            r3 = _mm_castps_si128(r3f);           \
    } while(false)


    const __m128i and_mask = _mm_set1_epi32( 0x01010101 );
    const __m128i byte_masks = _mm_set1_epi32(0xff);

    // Transpose to be in the form:
    // a = A[0, 0:3], A[1, 0:3], A[2, 0:3], A[3, 0:3]
    // b = B[0, 0:3], B[1, 0:3], B[2, 0:3], B[3, 0:3]
    // c = C[0, 0:3], C[1, 0:3], C[2, 0:3], C[3, 0:3]
    // d = D[0, 0:3], D[1, 0:3], D[2, 0:3], D[3, 0:3]
    TRANSPOSE4_EPI32(a, b, c, d);

    // Apply mask ops
    a = _mm_and_si128(a, and_mask);
    b = _mm_and_si128(b, and_mask);
    c = _mm_and_si128(c, and_mask);
    d = _mm_and_si128(d, and_mask);


    a = _mm_or_si128(a, _mm_srli_epi32(a, 6));
    a = _mm_or_si128(a, _mm_srli_epi32(a, 12));
    a = _mm_and_si128(a, byte_masks);
    
    b = _mm_or_si128(b, _mm_srli_epi32(b, 6));
    b = _mm_or_si128(b, _mm_srli_epi32(b, 12));
    b = _mm_and_si128(b, byte_masks);

    c = _mm_or_si128(c, _mm_srli_epi32(c, 6));
    c = _mm_or_si128(c, _mm_srli_epi32(c, 12));
    c = _mm_and_si128(c, byte_masks);

    d = _mm_or_si128(d, _mm_srli_epi32(d, 6));
    d = _mm_or_si128(d, _mm_srli_epi32(d, 12));
    d = _mm_and_si128(d, byte_masks);

    // Transpose back into the original form.
    TRANSPOSE4_EPI32(a, b, c, d);

    // Shift everything so it's ready for interlacing.
    // a = _mm_slli_epi32(a, 0);
    b = _mm_slli_epi32(b, 8);
    c = _mm_slli_epi32(c, 16);
    d = _mm_slli_epi32(d, 24);

    // And collapse vectors
    a = _mm_or_si128(a, b);
    c = _mm_or_si128(c, d);
    a = _mm_or_si128(a, c);

    #undef TRANSPOSE4_EPI32

    // 0b01 -> 0b11
    a = _mm_or_si128(a, _mm_slli_epi32(a, 1));

    return a;
}


inline
void stream_bw_pack_cell4_bc1(
    const uint8_t* row_1,
    const uint8_t* row_2,
    const uint8_t* row_3,
    const uint8_t* row_4,
    uint64_t* out_pixels
) {

    __m128i a = _mm_lddqu_si128((const __m128i*)row_1);
    __m128i b = _mm_lddqu_si128((const __m128i*)row_2);
    __m128i c = _mm_lddqu_si128((const __m128i*)row_3);
    __m128i d = _mm_lddqu_si128((const __m128i*)row_4);
    
    __m128i block = bw_pack_32_to_8_vec(a, b, c, d);
    __m128i header = _mm_set1_epi32(0xffff0000);

    _mm_storeu_si128((__m128i*)out_pixels, _mm_unpacklo_epi32(header, block) );
    _mm_storeu_si128((__m128i*)&out_pixels[2], _mm_unpackhi_epi32(header, block));
}


inline
void stream_bw_pack_row_bc1(
    const uint8_t* row_1,
    const uint8_t* row_2,
    const uint8_t* row_3,
    const uint8_t* row_4,
    bc1_4x4* out_pixels,
    // How many 4x4 there are per width
    const uint32_t out_count
) {

    uint32_t remaining = out_count;
    uint64_t* out_pixels_u64 = (uint64_t*)out_pixels;

    // Do 4 at a time.
    if( remaining >=4 ) {

        for(; remaining > 4; remaining-=4) {
            stream_bw_pack_cell4_bc1(row_1, row_2, row_3, row_4, out_pixels_u64);
            row_1 += 4*4;
            row_2 += 4*4;
            row_3 += 4*4;
            row_4 += 4*4;
            out_pixels_u64 += 4;
        }

    }

    // Do remaining
    if( remaining ) {

        for(; remaining; --remaining) {

            // Write out black/white end point
            uint32_t* out_pixels_u32 = (uint32_t*)out_pixels_u64;
            out_pixels_u32[0] = 0xffff0000;

            uint32_t indices(
                bw_pack_32_to_8(*(const uint32_t*)row_4)
                | (bw_pack_32_to_8(*(const uint32_t*)row_3) << 8)
                | (bw_pack_32_to_8(*(const uint32_t*)row_2) << 16)
                | (bw_pack_32_to_8(*(const uint32_t*)row_1) << 24)
            );
            // 0b01 -> 0b11
            indices |= (indices << 1);

            // Write back.
            out_pixels_u32[1] = indices;

            ++out_pixels_u64;
            row_1 += 4;
            row_2 += 4;
            row_3 += 4;
            row_4 += 4;
        }

    }

}


inline
void bwu8_to_bc1(
    const uint8_t* in_pixels,
    bc1_4x4* out_pixels,
    const uint32_t source_width,
    const uint32_t source_height
) {

    for(uint32_t y=0; y<source_height; y+=4) {
        const uint8_t* row_1 = &in_pixels[y*source_width];
        const uint8_t* row_2 = row_1 + source_width;
        const uint8_t* row_3 = row_2 + source_width;
        const uint8_t* row_4 = row_3 + source_width;
        bc1_4x4* row_out = &out_pixels[source_width/4];

        stream_bw_pack_row_bc1(
            row_1,
            row_2,
            row_3,
            row_4,
            row_out,
            source_width/4
        );
    }

}
