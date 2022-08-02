#pragma once

#include <bit>
#include <cstdint>


// valid for 23bits, [0, 8388607]
// NB: Not sure how useful this is in practice, for shaders f32=>i32 is full-rate on AMD
constexpr float uint_to_float(const uint32_t x)
{
    return std::bit_cast<float>(x + 0x4b000000) - std::bit_cast<float>(0x4b000000);
}


constexpr uint32_t float_to_uint(const float x)
{
    return std::bit_cast<uint32_t>(x + std::bit_cast<float>(0x4b000000)) & 0x7fffff;
}


