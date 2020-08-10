#pragma once

#include <x86intrin.h>


struct alignas(16) bbox {
    float min_x, min_y,
          max_x, max_y;
};


inline
bool test_bbox_intersection( const bbox& a, const bbox& b ) {

    const __m128 a_vec = _mm_load_ps( (const float*)&a );
    const __m128 b_vec = _mm_load_ps( (const float*)&b );

    // AminX, AminY, BminX, BminY
    const __m128 min_values = _mm_movelh_ps( a_vec, b_vec );
    // BmaxX, BmaxY, AmaxX, AmaxY
    const __m128 max_values = _mm_movehl_ps( a_vec, b_vec );

    // AminX < BmaxX, AminY < BmaxY, BminX < AmaxX, BminY < AmaxY
    const __m128 intersections = _mm_cmplt_ps( min_values, max_values );

    // AminX < BmaxX && AminY < BmaxY && BminX < AmaxX && BminY < AmaxY
    return ( _mm_movemask_ps( intersections ) == 0b1111 );
}
