#pragma once

#include <algorithm>
#include <cmath>


struct vec2 {
    float x, y;
};


struct vec3 {
    float x, y, z;
};

// Given a triangle and a scanline.
// Calculate:
//      * The X coordinates the triangle intersects the line.
//      * The the amount to multiply A, B and C at both these
//        X intervals for the purpose of linearly interpolating
//        vertex attributes.
// Returns false if the scanline doesn't intersect the
// triangle.

// The motivation for this is to basically assist in scanline
// rendering triangles.

// Rather than calculating barycentric coordinates per pixel, this approach
// opts to basically figure out the x values to iterate between and
// linear combinations that can be applied to vertex attributes
// to allow faster lerping when flooding pixels.

// i.e (untested example):

#if 0
void draw_scanline(
        // y = image_y + 0.5
        const float y,
        // uv coords are expected to have been scaled to
        // the images dimensions and offseted accordingly
        const vec2 uv_0,
        const vec2 uv_1,
        const vec2 uv_2,
        // Preferably, you'd be writing out multiple
        // vertex attributes at the same time, in this
        // example we're using using one (P).
        const vec3 p_0,
        const vec3 p_1,
        const vec3 p_2,
        vec3* p_buffer,
        const float fwidth
) {

    float x_start;
    float x_end;
    vec3 interp_start;
    vec3 interp_end;

    if(!tri_scanline_barycoordinates(
                y,
                uv_0, uv_1, uv_2,
                &x_start, &x_end,
                &interp_start, &interp_end)) {
        return;
    }

    // Guard against out of range intersections
    if ( ( x_start <= 0 && x_end <= 0 ) ||
         ( x_start >= fwidth && x_end >= fwidth ) ) {
        return;
    }

    uint32_t iter_x = uint32_t(std::max( std::round( x_start ), 0.0f ));
    const uint32_t iter_x_end = uint32_t(std::min( std::round( x_end ), fwidth ));

    // Guard against intervals where we do nothing
    if(iter_x >= iter_x_end) {
        return;
    }

    vec3 P0 = p_0 * interp_start.x + p_1 * interp_start.y + p_2 * interp_start.z;
    vec3 P1 = p_0 * interp_end.x + p_1 * interp_end.y + p_2 * interp_end.z;
    vec3 P_interval = (P1-P0) / (x_end - x_start);

    // Etc
    for(; iter_x<iter_x_end; ++iter_x) {
        p_buffer[iter_x] = P0 + P_interval * (x_start-iter_x);
    }
}
#endif


inline
bool tri_scanline_barycoordinates(

    // Y value of the scanline
    const float y,

    // Triangle vertices
    const vec2 A,
    const vec2 B,
    const vec2 C,

    // X intervals that the triangle intersects
    // with the scanline.
    float* x_start,
    float* x_end,

    // ABC multiplication combinations
    vec3* interp_start,
    vec3* interp_end

) {

    // Create offsets from y.
    const float o_Ay = A.y - y;
    const float o_By = B.y - y;
    const float o_Cy = C.y - y;

    const bool A_y_side = std::signbit(o_Ay);
    const bool B_y_side = std::signbit(o_By);
    const bool C_y_side = std::signbit(o_Cy);


    // TODO: I don't like all the branching buisness
    // it would be nice to be able to simplify it a bit more.

    // Figure out which vertex has two edges crossing Y
    // (if there aren't two edges crossing Y, it means it doesn't
    // intersect the scanline)
    if(A_y_side == B_y_side) {

        // All vertices are one side of the scanline
        if(A_y_side == C_y_side) {
            return false;
        }

        // Vertex with two connections is C
        const float interp_bc = o_Cy / (o_Cy - o_By);
        const float interp_ca = o_Ay / (o_Ay - o_Cy);

        *interp_start = vec3{0.0f, interp_bc, 1.0f-interp_bc};
        *interp_end = vec3{1.0f-interp_ca, 0.0f, interp_ca};
    }

    else if(A_y_side == C_y_side) {

        // Vertex with two connections is B
        const float interp_ab = o_By / (o_By - o_Ay);
        const float interp_bc = o_Cy / (o_Cy - o_By);

        *interp_start = vec3{0.0f, interp_bc, 1.0f-interp_bc};
        *interp_end = vec3{interp_ab, 1.0f-interp_ab, 0.0f};

    }
    else {

        // Vertex with two connections is A
        const float interp_ab = o_By / (o_By - o_Ay);
        const float interp_ca = o_Ay / (o_Ay - o_Cy);

        *interp_start = vec3{interp_ab, 1.0f-interp_ab, 0.0f};
        *interp_end = vec3{1.0f-interp_ca, 0.0f, interp_ca};

    }

    // Figure out the intervals by simply applying the previously
    // calculated interpolations
    *x_start = interp_start->x*A.x + interp_start->y*B.x + interp_start->z*C.x;
    *x_end = interp_end->x*A.x + interp_end->y*B.x + interp_end->z*C.x;

    if(*x_start > *x_end) {

        std::swap(*x_start, *x_end);
        std::swap(*interp_start, *interp_end);
    }

    return true;

}
