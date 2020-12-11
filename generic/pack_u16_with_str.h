#pragma once


// This basically takes a uint16, stringifies it then stores
// the first 6 bytes with its string representation and stores
// the original uint16 in the last 2 bytes
// The purpose of this was so an atomic_uint64 could be stored as a cache
// of port->string and fed into getaddrinfo

#include <cstdint>
#include <cstring>


uint64_t pack_u16_with_str(const uint16_t x) {

    uint64_t rvalue;
    char b[8];

    // Store u16 at the back
    std::memcpy(&b[6], &x, sizeof(x));

    // Stringify
    b[0] = x / 10000 + '0';
    b[1] = (x % 10000) / 1000 + '0';
    b[2] = (x % 1000) / 100 + '0';
    b[3] = (x % 100) / 10 + '0';
    b[4] = x % 10 + '0';
    b[5] = 0;

    // trim leading 0's
    int i = 0;
    for(; i < 4 && b[i] == '0'; ++i);
    switch(i)
    {
        case 1:
            b[4] = b[4+i];
        case 2:
            b[3] = b[3+i];
        case 3:
            b[2] = b[2+i];
        case 4:
            b[1] = b[1+i];
            b[0] = b[0+i];
        case 0:
            break;
    }

    std::memcpy(&rvalue, b, sizeof(rvalue));
    return rvalue;

}
