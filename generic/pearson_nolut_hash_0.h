#pragma once

#include <cstdint>


// TODO: All of this needs to be looked at a bit better
//       and using pext explicitly
//
// After testing with SMHasher, results are only faster than
// say xxhash, when you use pext, and only specialise to a
// fixed data size. 

inline
uint8_t hash_round_u8(const uint8_t c) {

    // Adjustment to prevent 0->0
    const uint8_t C = (c + 2);

    // Basically, deinterlace bits
    // and then pack them together.
    uint8_t hi = C & 0x55;
    hi = (hi | (hi >> 1)) & 0x33;
    hi = (hi | (hi >> 2)) & 0x0f;

    uint8_t lo = (C>>1) & 0x55;
    lo = (lo | (lo >> 1)) & 0x33;
    lo = (lo | (lo >> 2)) & 0x0f;

    return (hi << 4) | lo;

}


inline
uint16_t hash_round_u16(const uint16_t c) {

    // Adjustment to prevent 0->0
    const uint16_t C = (c + 3);

    // Basically, deinterlace bits
    // and then pack them together.
    uint16_t hi = C & 0x5555;
    hi = (hi | (hi >> 1)) & 0x3333;
    hi = (hi | (hi >> 2)) & 0x0f0f;
    hi = (hi | (hi >> 4)) & 0x00ff;

    uint16_t lo = (C>>1) & 0x5555;
    lo = (lo | (lo >> 1)) & 0x3333;
    lo = (lo | (lo >> 2)) & 0x0f0f;
    lo = (lo | (lo >> 4)) & 0x00ff;

    return (hi << 8) | lo;

}


inline
uint32_t hash_round_u32(const uint32_t c) {

    // Adjustment to prevent 0->0
    const uint32_t C = (c + 2);

    // Basically, deinterlace bits
    // and then pack them together.
    uint32_t hi = C & 0x55555555;
    hi = (hi | (hi >> 1)) & 0x33333333;
    hi = (hi | (hi >> 2)) & 0x0f0f0f0f;
    hi = (hi | (hi >> 4)) & 0x00ff00ff;
    hi = (hi | (hi >> 8)) & 0x0000ffff;

    uint32_t lo = (C>>1) & 0x55555555;
    lo = (lo | (lo >> 1)) & 0x33333333;
    lo = (lo | (lo >> 2)) & 0x0f0f0f0f;
    lo = (lo | (lo >> 4)) & 0x00ff00ff;
    lo = (lo | (lo >> 8)) & 0x0000ffff;

    return (hi << 16) | lo;

}


inline
uint64_t hash_round_u64(const uint64_t c) {

    // Adjustment to prevent 0->0
    const uint64_t C = (c + 2);

    // Basically, deinterlace bits
    // and then pack them together.
    uint64_t hi = C & 0x5555555555555555;
    hi = (hi | (hi >> 1)) & 0x3333333333333333;
    hi = (hi | (hi >> 2)) & 0x0f0f0f0f0f0f0f0f;
    hi = (hi | (hi >> 4)) & 0x00ff00ff00ff00ff;
    hi = (hi | (hi >> 8)) & 0x0000ffff0000ffff;
    hi = (hi | (hi >> 16)) & 0x00000000ffffffff;

    uint64_t lo = (C>>1) & 0x5555555555555555;
    lo = (lo | (lo >> 1)) & 0x3333333333333333;
    lo = (lo | (lo >> 2)) & 0x0f0f0f0f0f0f0f0f;
    lo = (lo | (lo >> 4)) & 0x00ff00ff00ff00ff;
    lo = (lo | (lo >> 8)) & 0x0000ffff0000ffff;
    lo = (lo | (lo >> 16)) & 0x00000000ffffffff;

    return (hi << 32) | lo;

}

