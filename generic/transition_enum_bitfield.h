#pragma once

// This container a helper to migrate bitfield enums between different enum definitions.
// This exists because MSVC struggles a lot with folding bitfields which share the same value
// when in the format of `if(src & sreMask) dst |= dstMask`
//
// example usage:
//
// #include <cstdint>
//
// enum VkAccessFlagBits {
//     VK_ACCESS_INDIRECT_COMMAND_READ_BIT = 0x00000001,
//     VK_ACCESS_INDEX_READ_BIT = 0x00000002,
//     VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT = 0x00000004,
//     VK_ACCESS_UNIFORM_READ_BIT = 0x00000008,
//     VK_ACCESS_INPUT_ATTACHMENT_READ_BIT = 0x00000010,
//     VK_ACCESS_SHADER_READ_BIT = 0x00000020,
//     VK_ACCESS_SHADER_WRITE_BIT = 0x00000040,
//     VK_ACCESS_TRANSFER_READ_BIT = 0x00000800,
//     VK_ACCESS_TRANSFER_WRITE_BIT = 0x00001000,
// };
//
// enum ReadAccessFlagBits {
//     Indirect                = 1 << 0,
//     Index                   = 1 << 1,
//     VertexAttr              = 1 << 2,
//     Uniform                 = 1 << 3,
//     InputAttachment         = 1 << 4,
//     Shader                  = 1 << 5,
//     Transfer                = 1 << 6,
// };
//
// uint32_t toVulkanFlags(uint32_t bits)
// {
//     return TransitionBits<
//         // ReadAccessFlagBits   VkAccessFlagBits
//         uint32_t,               uint32_t,
//         Indirect,               VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
//         Index,                  VK_ACCESS_INDEX_READ_BIT,
//         VertexAttr,             VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
//         Uniform,                VK_ACCESS_UNIFORM_READ_BIT,
//         InputAttachment,        VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
//         Shader,                 VK_ACCESS_SHADER_READ_BIT,
//         Transfer,               VK_ACCESS_TRANSFER_READ_BIT
//     >::apply(bits);
// }


#if defined(_MSC_VER)
    #define FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
    #define FORCEINLINE __attribute__((always_inline)) inline
#else
    #define FORCEINLINE inline
#endif


template<typename SrcType, typename DstType, auto...>
struct TransitionBits
{
    FORCEINLINE static constexpr DstType apply(SrcType src, DstType current=0) { return current; }
};


template<typename SrcType, typename DstType, auto srcBits, auto dstBits, auto... nextPairs>
struct TransitionBits<SrcType, DstType, srcBits, dstBits, nextPairs...>
{

private:

    static constexpr int getShiftAmount(SrcType src_, DstType dst)
    {
        DstType src = (DstType)src_;
        if(src == dst) { return 0; }
        int counter = 0;
        if(src > dst)
        {
            while(src && (src != dst))
            {
                --counter;
                src >>= 1;
            }
        }
        else
        {
            while(src && (src != dst))
            {
                ++counter;
                src <<= 1;
            }
        }

        // Invalid mask, cannot simply bit shift
        if(!src) { return 0xffff; }
        return counter;
    }

public:

    FORCEINLINE static constexpr DstType apply(SrcType src, DstType current=0)
    {
        constexpr int shift = getShiftAmount(srcBits, dstBits);
        if constexpr(shift == 0) { current |= (src & srcBits); }
        else if constexpr(shift == 0xffff)
        {
            current |= (dstBits * bool((src & srcBits) == srcBits));
        }
        else if constexpr(shift > 0)
        {
            current |= ((src & srcBits) << shift);
        }
        else
        {
            current |= ((src & srcBits) >> (-shift));
        }

        return TransitionBits<SrcType, DstType, nextPairs...>::apply(src, current);
    }
};
