#pragma once

#include <cstdint>

// NB: This really isn't that useful, and is just more of a
// "Oh that's kinda cool", sort of impl.


// This is based of 'Conditionally negate a value without branching' from
// from Sean Eron Andersons' 'Bit Twiddling Hacks' document.
// https://graphics.stanford.edu/~seander/bithacks.html

// Original implementation:

// bool fNegate;  // Flag indicating if we should negate v.
// int v;         // Input value to negate if fNegate is true.
// int r;         // result = fNegate ? -v : v;

// r = (v ^ -fNegate) + fNegate;

// Typical assembly output (clang 10.0.1):
// mov     eax, esi     // 89 f0
// neg     eax          // f7 d8
// xor     eax, edi     // 31 f8
// add     eax, esi     // 01 f0


inline
int negate_if_not(const int value, const int flag) {

    // NB this does the opposite of the original bithack
    // i.e flag = 0 means negate.
    // We are also not using a bool for the flag, as it
    // seems to cause a test and cmove.

    // Typical assembly output (clang 10.0.1):
    //  imul    esi, edi            // 0f af f7
    //  lea     eax, [rsi + rsi]    // 8d 04 36
    //  sub     eax, edi            // 29 f8

    // Also appears (atleast on the surface), to perform a
    // bit better
    // https://godbolt.org/z/rez6To
    // https://godbolt.org/z/nonon4

    return flag*value*2 - value;
}
