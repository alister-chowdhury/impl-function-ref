#pragma once

// This contains logic for variable length encoding u32 numbers using a prefix code (varint).
// This is done by storing the following codes in the first byte:
//  0xxxxxxx = 1 bytes
//  10xxxxxx = 2 bytes
//  110xxxxx = 3 bytes
//  1110xxxx = 4 bytes
//  1111xxxx = 5 bytes
//
//
// This differs from the another common method of storing one bit per byte.
//  1xxxxxxxx = need to load the next byte
//  0xxxxxxxx = end of byte run
//
//////
//
// Example usage:
//
//
//      // Encode a list of numbers into a data buffer:
//      uint32_t totalWritten = 0;
//      for(uint32_t number : numbers)
//      {
//          uint32_t bytesWritten = VLEEnc(number, &buffer[totalWritten], bufferEnd);
//          if(bytesWritten == 0)
//          {
//              // Ran out write space
//              break;
//          }
//          totalWritten += bytesWritten;
//      }
//
//
//
//      // Decoding an encoded stream back out
//      uint32_t i = 0;
//      while(bufferPtr < bufferEnd)
//      {
//          numbers[i++] = VLEDec(bufferPtr, bufferEnd);
//      }
//
/////
//
// Packing metadata:
// When we fail to compress a number and end up having to use 5 bytes, there are
// 4 bits of redundancy in the first byte.
//
//  1111xxxx = 5 bytes
//      ^^^^
//
// This can be used for a number of things, but one idea would be to target when
// big numbers are close together:
//
// 1111xxxx = 5 bytes + 0bxxxx decoded u32's follow, this saves both space and
// effort when we encounter 
//

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>


#if(defined(_MSVC_LANG) && (_MSVC_LANG >= 201803L)) || __cplusplus >= 201803L
#    define IF_LIKELY(x) if(x) [[likely]]
#    define IF_UNLIKELY(x) if(x) [[unlikely]]
#elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
#    define IF_LIKELY(x) if(__builtin_expect(x, 1))
#    define IF_UNLIKELY(x) if(__builtin_expect(x, 0))
#else
#    define IF_LIKELY(x) if(x)
#    define IF_UNLIKELY(x) if(x)
#endif

#if defined(_MSC_VER)
#    define FORCEINLINE __forceinline
#    define UNREACHABLE                                                                                                \
        {                                                                                                              \
            __assume(0);                                                                                               \
        }
#elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
#    define FORCEINLINE __attribute__((always_inline)) inline
#    define UNREACHABLE                                                                                                \
        {                                                                                                              \
            __builtin_unreachable();                                                                                   \
        }
#else
#    define FORCEINLINE inline
#    define UNREACHABLE
#endif


#define CONSTEXPRINLINE constexpr FORCEINLINE


constexpr static uint32_t VLEMaxEncodeBytes = 5;


constexpr static inline uint8_t VLESizeTable[]{1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 3, 3,
                                               3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5};


CONSTEXPRINLINE uint8_t VLEEncSize(uint32_t value)
{
    /*
        Should be roughly equivilant to:
            or      edi, 1
            bsr     edi, edi
            movzx   eax, BYTE PTR VLESizeTable[rdi]
    */
    const uint32_t bitsNeeded = (uint32_t)std::bit_width(value | 1u);
    // const uint8_t numBytes = (bitsNeeded + 6u) / 7u;
    const uint8_t numBytes = VLESizeTable[bitsNeeded - 1u];
    return numBytes;
}


// 0xxxxxxx = 1 bytes
// 10xxxxxx = 2 bytes
// 110xxxxx = 3 bytes
// 1110xxxx = 4 bytes
// 1111xxxx = 5 bytes

// When encoding 1111xxxx, the last 4 bits can be used to stash extra information,
// for instance that the next [0 : 15] uint32s are not compressed and should just be
// memcpy'd and skip a decoding step altogether (assuming big numbers come in clusters).

CONSTEXPRINLINE uint32_t VLEEnc(uint32_t value, uint8_t* data, const uint8_t* end)
{
    uint8_t bytesNeeded = VLEEncSize(value);

    IF_UNLIKELY((data + bytesNeeded) > end)
    {
        return 0;
    }

    switch(bytesNeeded)
    {
    case 1: {
        data[0] = value;
        return bytesNeeded;
    }
    case 2: {
        uint8_t prefix = ~(0xff >> 1);
        data[1] = value & 0xff;
        value >>= 8;
        data[0] = prefix | value;
        return bytesNeeded;
    }
    case 3: {
        uint8_t prefix = ~(0xff >> 2);
        data[1] = value & 0xff;
        value >>= 8;
        data[2] = value & 0xff;
        value >>= 8;
        data[0] = prefix | value;
        return bytesNeeded;
    }
    case 4: {
        uint8_t prefix = ~(0xff >> 3);
        data[1] = value & 0xff;
        value >>= 8;
        data[2] = value & 0xff;
        value >>= 8;
        data[3] = value & 0xff;
        value >>= 8;
        data[0] = prefix | value;
        return bytesNeeded;
    }
    case 5: {
        uint8_t prefix = ~(0xff >> 4);
        data[1] = value & 0xff;
        value >>= 8;
        data[2] = value & 0xff;
        value >>= 8;
        data[3] = value & 0xff;
        value >>= 8;
        data[4] = value & 0xff;
        value >>= 8;
        data[0] = prefix;
        return bytesNeeded;
    }
    }

    UNREACHABLE
}


CONSTEXPRINLINE uint32_t VLEDec(const uint8_t*& data, const uint8_t* end)
{
    uint8_t numBytes = std::countl_one(uint32_t(data[0]) << 24) + 1;

    // give a 1 byte decode a dedicated fast path
    IF_LIKELY(numBytes == 1)
    {
        return *data++;
    }

    // > 5 is not valid, but atleast prevent reading OOB memory
    // in the case we have a malformed input / the redundant bits are
    // being used to store extra data.
    if(numBytes > 5)
    {
        numBytes = 5;
    }

    const uint8_t* data_ = data;
    data += numBytes;

    IF_UNLIKELY(data > end)
    {
        return 0;
    }

    switch(numBytes)
    {
    case 2: {
        uint8_t  prefix = (0xff >> 1);
        uint32_t result = (uint32_t(data_[0] & prefix) << 8) | (uint32_t(data_[1]) << 0);
        return result;
    }
    case 3: {
        uint8_t  prefix = (0xff >> 2);
        uint32_t result = (uint32_t(data_[0] & prefix) << 16) | (uint32_t(data_[2]) << 8) | (uint32_t(data_[1]) << 0);
        return result;
    }
    case 4: {
        uint8_t  prefix = (0xff >> 3);
        uint32_t result = (uint32_t(data_[0] & prefix) << 24) | (uint32_t(data_[3]) << 16) | (uint32_t(data_[2]) << 8)
                          | (uint32_t(data_[1]) << 0);
        return result;
    }
    case 5: {
        uint32_t result = (uint32_t(data_[4]) << 24) | (uint32_t(data_[3]) << 16) | (uint32_t(data_[2]) << 8)
                          | (uint32_t(data_[1]) << 0);
        return result;
    }
    }
    
    UNREACHABLE
}


// Variant to rotate the msb around to help pack negative numbers
CONSTEXPRINLINE uint32_t VLEEncZig(uint32_t value, uint8_t* data, const uint8_t* end)
{
    return VLEEnc(std::rotl(value, 1), data, end);
}


CONSTEXPRINLINE uint32_t VLEDecZig(const uint8_t*& data, const uint8_t* end)
{
    return std::rotr(VLEDec(data, end), 1);
}
