#pragma once

#include <cstdint>


template <typename T>
inline bool all_the_same_block(const T c, const T* ptr, const uint32_t n) {

    T same = 0;
    for( uint32_t i = 0; i < n ; ++i ) {
        same |= c ^ ptr[i];
    }
    return same == 0;

}


inline
bool all_the_same(const uint8_t* ptr, const uint32_t n) {

    if( n < 2 ) return true;

    uint32_t remaining = n;

    const uint8_t c = *ptr;
    for( ; remaining > 64; ptr+=64, remaining-=64 ) {
        if(!all_the_same_block<uint8_t>(c, ptr, 64)) return false;
    }

    return all_the_same_block<uint8_t>(c, ptr, remaining);

}


inline
bool all_the_same(const uint16_t* ptr, const uint32_t n) {

    if( n < 2 ) return true;

    uint32_t remaining = n;

    const uint16_t c = *ptr;
    for( ; remaining > 32; ptr+=32, remaining-=32 ) {
        if(!all_the_same_block<uint16_t>(c, ptr, 32)) return false;
    }

    return all_the_same_block<uint16_t>(c, ptr, remaining);

}


inline
bool all_the_same(const uint32_t* ptr, const uint32_t n) {

    if( n < 2 ) return true;

    uint32_t remaining = n;

    const uint32_t c = *ptr;
    for( ; remaining > 16; ptr+=16, remaining-=16 ) {
        if(!all_the_same_block<uint32_t>(c, ptr, 16)) return false;
    }

    return all_the_same_block<uint32_t>(c, ptr, remaining);

}


inline
bool all_the_same(const uint64_t* ptr, const uint64_t n) {

    if( n < 2 ) return true;

    uint64_t remaining = n;

    const uint64_t c = *ptr;
    for( ; remaining > 8; ptr+=8, remaining-=8 ) {
        if(!all_the_same_block<uint64_t>(c, ptr, 8)) return false;
    }

    return all_the_same_block<uint64_t>(c, ptr, remaining);

}