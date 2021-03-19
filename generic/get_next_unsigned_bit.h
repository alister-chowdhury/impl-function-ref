#pragma once

#include <cstddef>


template<typename UnsignedT>
UnsignedT repeat_bits_backward(UnsignedT x, size_t count) {
    if(sizeof(UnsignedT) >= 8) {
        if(count & 32) {
            x |= (x >> 1);
            x |= (x >> 2);
            x |= (x >> 4);
            x |= (x >> 8);
            x |= (x >> 16);
        }
    }
    if(sizeof(UnsignedT) >= 4) {
        if(count & 16) {
            x |= (x >> 1);
            x |= (x >> 2);
            x |= (x >> 4);
            x |= (x >> 8);
        }
    }
    if(sizeof(UnsignedT) >= 2) {
        if(count & 8) {
            x |= (x >> 1);
            x |= (x >> 2);
            x |= (x >> 4);
        }
    }
    if(count & 4) {
        x |= (x >> 1);
        x |= (x >> 2);
    }
    if(count & 2) {
        x |= (x >> 1);
        x |= (x >> 1);
    }
    if(count & 1) {
        x |= (x >> 1);
    }
    return x;
}

template<typename UnsignedT>
UnsignedT repeat_bits_forward(UnsignedT x, size_t count) {
    if(sizeof(UnsignedT) >= 8) {
        if(count & 32) {
            x |= (x << 1);
            x |= (x << 2);
            x |= (x << 4);
            x |= (x << 8);
            x |= (x << 16);
        }
    }
    if(sizeof(UnsignedT) >= 4) {
        if(count & 16) {
            x |= (x << 1);
            x |= (x << 2);
            x |= (x << 4);
            x |= (x << 8);
        }
    }
    if(sizeof(UnsignedT) >= 2) {
        if(count & 8) {
            x |= (x << 1);
            x |= (x << 2);
            x |= (x << 4);
        }
    }
    if(count & 4) {
        x |= (x << 1);
        x |= (x << 2);
    }
    if(count & 2) {
        x |= (x << 1);
        x |= (x << 1);
    }
    if(count & 1) {
        x |= (x << 1);
    }
    return x;
}


// Return back a mask of the next unsigned bit.
// i.e:
// 00000000 -> 00000001
// 00000001 -> 00000010
// 00000010 -> 00000001
// 00000011 -> 00000100
// 00000100 -> 00000001
// 00000101 -> 00000010
// 00000110 -> 00000001
// 01101111 -> 00010000
// 10000110 -> 00000001
// 10111111 -> 01000000
// 11111110 -> 00000001
//
// If it returns back 0, there are no free bits left.
template<typename UnsignedT>
UnsignedT get_next_unsigned_bit(const UnsignedT x) {
#if 1
    return (x + 1) & ~x;
#else
    return (x | (x+1)) - x;
#endif
}


template<typename UnsignedT>
UnsignedT get_next_unsigned_bits(const UnsignedT x, size_t count) {
    UnsignedT y = x;
    if(count > sizeof(UnsignedT)*8) { count = sizeof(UnsignedT)*8; }
    while(count--) {
        y |= (y + 1);
    }
    return y & ~x;
}


// Return the next batch of consecutive unsigned bits
// when count = 0, the result is undefined
// when count = 1, there is no difference between this and get_next_unsigned_bit
template<typename UnsignedT>
UnsignedT get_consecutive_next_unsigned_bits(const UnsignedT x, size_t count) {
    --count;
    return repeat_bits_forward(
        get_next_unsigned_bit(repeat_bits_backward(x, count)),
        count
    );
}
