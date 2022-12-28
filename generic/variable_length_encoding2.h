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
//
// Additionally, the bottom 4 bits of a 5 byte write encode how many subsequent
// u32s are already decoded and should just be copied, in an attempt to minimize
// the overhead of dealing with sequential big numbers.
//
// i.e:
//  [0xffffffff, 0xffffffff] => [0xf2, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]
//               rather than => [0xf1, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff]
//
//////
//
// Example usage:
//
//
//      // Encode a list of numbers into a data buffer:
//      u8* buffer = ...;
//      u32* numbers = ...;
//      u32 writtenBytes = VLEEncodeStream(numbers, numNumbers, buffer, bufferSize);
//
//
//      // Decoding an encoded stream back out
//      u32 numU32sRead = VLEDecodeStream(buffer, writtenBytes, numbers, numNumbers);
//
//////

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
#    define ASSUME(expr) __assume(expr);
#    define NOINLINE __declspec(noinline)
#    define FORCEINLINE __forceinline
#    define UNREACHABLE                                                                                                \
        {                                                                                                              \
            __assume(0);                                                                                               \
        }
#elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
#    define ASSUME(expr) if(!(expr)) { __builtin_unreachable(); }
#    define NOINLINE __attribute__((noinline))
#    define FORCEINLINE __attribute__((always_inline)) inline
#    define UNREACHABLE                                                                                                \
        {                                                                                                              \
            __builtin_unreachable();                                                                                   \
        }
#else
#    define ASSUME(expr)
#    define NOINLINE
#    define FORCEINLINE inline
#    define UNREACHABLE
#endif


#if defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#   define VLE_TARGET_X86   1
#else
#   define VLE_TARGET_X86   0
#endif


using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;


#if defined(_MSC_VER)
#   pragma intrinsic(_BitScanReverse)
    // using std::bit_width causes MSVC to emit unwanted runtime checks
    FORCEINLINE u32 bitWidth(unsigned long x)
    {
        unsigned long index;
        _BitScanReverse(&index, x);
        return index + 1;
    }
#else
#   include <bit>
    FORCEINLINE u32 bitWidth(u32 x)
    {
        return std::bit_width(x | 1u);
    }
#endif


constexpr static inline u8 VLESizeTable[]
{
    1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5
};


FORCEINLINE u8 VLEEncSize(u32 value)
{
    /*
        Should be roughly equivilant to:
            or      edi, 1
            bsr     edi, edi
            movzx   eax, BYTE PTR VLESizeTable[rdi]
    */
    const u32 bitsNeeded = bitWidth(value | 1u);
    // const u8 numBytes = (bitsNeeded + 6u) / 7u;
    const u8 numBytes = VLESizeTable[bitsNeeded - 1u];
    return numBytes;
}


struct VLEEncodeStateContext
{

    const u32*  decoded     {};
    const u32*  decodedEnd  {};
    u8*         out         {};
    const u8*   outEnd      {};
    u32         valid       {};

    FORCEINLINE u32 pack(u32 value) { return value; }
};


struct VLEDecodeStateContext
{
    const u8*   encoded     {};
    const u8*   encodedEnd  {};
    u32*        out         {};
    const u32*  outEnd      {};
    u32         valid       {};

    FORCEINLINE u32 unpack(u32 value) { return value; }

};


FORCEINLINE void VLEWriteGeneric(u32 value, u8* data, const u8 numBytes)
{
    for(u8 i=0; i<numBytes; ++i)
    {
        data[i] = value & 0xff; value >>= 8;
    }
}


FORCEINLINE void VLEWritex86(u32 value, u8* data, const u8 numBytes)
{
    switch(numBytes)
    {
        case 1: { *data = value & 0xff; return; }
        case 2: { *((u16*)data) = value & 0xffff; return; }
        case 3: { *((u16*)data) = value & 0xffff;  data[2] = (value >> 16) & 0xff; return;}
        case 4: { *((u32*)data) = value; return; }
    }
    UNREACHABLE
    return;
}


FORCEINLINE void VLEWrite(u32 value, u8* data, const u8 numBytes)
{
#if VLE_TARGET_X86
    VLEWritex86(value, data, numBytes);
#else // VLE_TARGET_X86
    VLEWriteGeneric(value, data, numBytes);
#endif // VLE_TARGET_X86
}



FORCEINLINE u32 VLEReadGeneric(const u8* data, const u8 numBytes)
{
    switch(numBytes)
    {
        case 1:
        {
            return (u32(data[0]) << 0);
        }
        case 2:
        {
            return (u32(data[1]) << 8)
                    | (u32(data[0]) << 0)
                    ;
        }
        case 3:
        {
            return (u32(data[2]) << 16)
                    | (u32(data[1]) << 8)
                    | (u32(data[0]) << 0)
                    ;
        }
        case 4:
        {
            return (u32(data[3]) << 24)
                    | (u32(data[2]) << 16)
                    | (u32(data[1]) << 8)
                    | (u32(data[0]) << 0)
                    ;
        }
    }
    UNREACHABLE
    return 0;
}


FORCEINLINE u32 VLEReadx86(const u8* data, const u8 numBytes)
{
    switch(numBytes)
    {
        case 1: { u32 x = *data; return x; }
        case 2: { u32 x = *((const u16*)data); return x; }
        case 3: { u32 x = data[2]; x <<= 16; x |= *((const u16*)data); return x;}
        case 4: { u32 x = *((const u32*)data); return x;}
    }
    UNREACHABLE
    return 0;
}


FORCEINLINE u32 VLERead(const u8* data, const u8 numBytes)
{
#if VLE_TARGET_X86
    return VLEReadx86(data, numBytes);
#else // VLE_TARGET_X86
    return VLEReadGeneric(data, numBytes);
#endif // VLE_TARGET_X86
}


template<typename EncodeStateContext>
FORCEINLINE void VLEEncodeStream(EncodeStateContext& ctx)
{
    u8* backState {};

    while(ctx.decoded < ctx.decodedEnd)
    {
        u32 value = ctx.pack(*ctx.decoded++);        
        u8 bytesNeeded = VLEEncSize(value);
        
        // OOB
        // Technically, we may have an extra byte if we are using a backstate, but
        // in practice, the liklihood of whether or not you decide to fail compression
        // due to a byte seems unlikely.
        IF_UNLIKELY((ctx.out + bytesNeeded) >= ctx.outEnd)
        {
            return;
        }

        IF_LIKELY(bytesNeeded < 4)
        {
            backState = nullptr;
            switch(bytesNeeded)
            {
                case 1:
                {
                    *ctx.out++ = value;
                    continue;
                }
                case 2:
                {
                    u8 prefix = ~(0xff >> 1);
                    VLEWrite(value >> 6, ctx.out + 1, 1);
                    ctx.out[0] = prefix | (value & 0b00111111);
                    ctx.out += 2;
                    continue;
                }
                case 3:
                {
                    u8 prefix = ~(0xff >> 2);
                    VLEWrite(value >> 5, ctx.out + 1, 2);
                    ctx.out[0] = prefix | (value & 0b00011111);
                    ctx.out += 3;
                    continue;
                }
                case 4:
                {
                    u8 prefix = ~(0xff >> 3);
                    VLEWrite(value >> 4, ctx.out + 1, 3);
                    ctx.out[0] = prefix | (value & 0b00001111);
                    ctx.out += 4;
                    continue;
                }
            }
            UNREACHABLE;
        }
        else if(backState)
        {
            if(++backState[0] == 0xffu)
            {
                backState = nullptr;
            }
            VLEWrite(value, ctx.out, 4);
            ctx.out += 4;
            continue;
        }
        else
        {
            backState = ctx.out;
            u8 prefix = ~(0xff >> 4);
            VLEWrite(value, ctx.out + 1, 4);
            ctx.out[0] = prefix;
            ctx.out += 5;
            continue;
        }
    }
    ctx.valid = true;
}


struct VLEDecShiftAndMask
{
    u32 shift;
    u32 mask;
};


static constexpr VLEDecShiftAndMask VLEDecSM[]
{
    {6, 0b00111111},
    {5, 0b00011111},
    {4, 0b00001111},
    {0, 0b00000000}
};


static constexpr u8 VLEDecSizes[]
{
    1, // 0b1000 => 1 extra byte(s)
    1, // 0b1001 => 1 extra byte(s)
    1, // 0b1010 => 1 extra byte(s)
    1, // 0b1011 => 1 extra byte(s)
    2, // 0b1100 => 2 extra byte(s)
    2, // 0b1101 => 2 extra byte(s)
    3, // 0b1110 => 3 extra byte(s)
    4  // 0b1111 => 4 extra byte(s)
};


template<typename DecodeStateContext>
FORCEINLINE void VLEDecodeStream(DecodeStateContext& ctx)
{

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warray-bounds"
#endif // defined(__clang__)

    // Workaround to load the effective address in composite ops.
    const u8* decSizesPtr = &VLEDecSizes[-0b1000];
    const VLEDecShiftAndMask* combineInfoPtr = &VLEDecSM[-1];

#if defined(__clang__)
#pragma clang diagnostic pop
#endif // defined(__clang__)

    while(ctx.encoded < ctx.encodedEnd)
    {
        // OOB
        IF_UNLIKELY(ctx.out >= ctx.outEnd)
        {
            return;
        }

        const u8 c = *ctx.encoded++;
        IF_LIKELY(c <= 0x7f)
        {
            *ctx.out++ = ctx.unpack(c);
            continue;
        }

        const u8 extraBytes = decSizesPtr[c >> 4];

        // OOBs read!
        IF_UNLIKELY((ctx.encoded + extraBytes) > ctx.encodedEnd)
        {
            return;
        }

        const VLEDecShiftAndMask& combineInfo = combineInfoPtr[extraBytes];
        *ctx.out++ = ctx.unpack((VLERead(ctx.encoded, extraBytes)  << combineInfo.shift)
                                | u32(c & combineInfo.mask))
                    ;
        ctx.encoded += extraBytes;

        // Handle extra metabits which are treated as predecoded and just need a memcpy
        IF_UNLIKELY(c > 0b11110000)
        {
            u32 metaBitsU32 = u32(c & 0b1111);
            const u8* encodedAfter = ctx.encoded + sizeof(u32) * metaBitsU32;
            u32* outItAfter = ctx.out + metaBitsU32;

            // OOBs read!
            IF_UNLIKELY((encodedAfter > ctx.encodedEnd) || (outItAfter > ctx.outEnd))
            {
                return;
            }
            
            for(; ctx.out < outItAfter; ++ctx.out, ctx.encoded += sizeof(u32))
            {
                u32 v;
                memcpy(&v, ctx.encoded, sizeof(u32));
                *ctx.out = ctx.unpack(v);
            }
        }
    }

    ctx.valid = true;
}


// Generate purpose encoder / decoder

// Returns bytes written or 0 if we ran out of space
NOINLINE u32 VLEEncodeStream(const u32* decoded, const u32 decodedSize, u8* out, const u32 outSize)
{
    VLEEncodeStateContext ctx
    {
        decoded,
        decoded + decodedSize,
        out,
        out + outSize
    };
    VLEEncodeStream(ctx);
    return ctx.valid ? (ctx.out - out) : 0;
}


// Returns u32s written or 0 if we ran out of space
NOINLINE u32 VLEDecodeStream(const u8* encoded, const u32 encodedSize, u32* out, const u32 outSize)
{
    VLEDecodeStateContext ctx
    {
        encoded,
        encoded + encodedSize,
        out,
        out + outSize
    };
    VLEDecodeStream(ctx);
    return ctx.valid ? (ctx.out - out) : 0;
}
