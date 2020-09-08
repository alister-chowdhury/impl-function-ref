#pragma once

#include <cstdint>


inline bool f16_between_0_and_1(const uint16_t value) {
    return value <= 0x3c00;
}

