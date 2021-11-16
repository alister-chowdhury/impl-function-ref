#pragma once


// This contains CRTP for bitfield related operations
// the main purpose is to easily allow for masking structs of 1bit paramters.
//
// This extends the subclass with the normal set of bitwise ops (|, &, ^, |=, &=, ^=, ~).
// This is done by basically exploiting the fact that std::bitset implementations always use
// a matching byte size of roundDivide(sizeof(bits), 8);
//
//
// Example usage:
//
//   using u64 = uint64_t;
//
//   struct RhiPhysicalDeviceFeatures : BitfieldOps<RhiPhysicalDeviceFeatures>
//   {
//
//       // CRTP is not especially constexpr friendly, so an explicit definition is needed
//       // for a sublcass to be able to go a.contains(b), however T::contains(a, b) is already
//       // available.
//       // When compilers implement http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p0847r7.html
//       // this can probably be addressed.
//       constexpr bool contains(const RhiPhysicalDeviceFeatures& b) const { return bfopsContains(*this, b); }
//
//       u64 robustBufferAccess:1;
//       u64 fullDrawIndexUint32:1;
//       u64 imageCubeArray:1;
//       u64 independentBlend:1;
//       u64 geometryShader:1;
//       u64 tessellationShader:1;
//       u64 sampleRateShading:1;
//       u64 dualSrcBlend:1;
//       u64 logicOp:1;
//       u64 multiDrawIndirect:1;
//       u64 drawIndirectFirstInstance:1;
//       u64 depthClamp:1;
//       u64 depthBiasClamp:1;
//       u64 fillModeNonSolid:1;
//       u64 depthBounds:1;
//       u64 wideLines:1;
//       u64 largePoints:1;
//       u64 alphaToOne:1;
//       u64 multiViewport:1;
//       u64 samplerAnisotropy:1;
//       u64 textureCompressionETC2:1;
//       u64 textureCompressionASTC_LDR:1;
//       u64 textureCompressionBC:1;
//       u64 occlusionQueryPrecise:1;
//       u64 pipelineStatisticsQuery:1;
//       u64 vertexPipelineStoresAndAtomics:1;
//       u64 fragmentStoresAndAtomics:1;
//       u64 shaderTessellationAndGeometryPointSize:1;
//       u64 shaderImageGatherExtended:1;
//       u64 shaderStorageImageExtendedFormats:1;
//       u64 shaderStorageImageMultisample:1;
//       u64 shaderStorageImageReadWithoutFormat:1;
//       u64 shaderStorageImageWriteWithoutFormat:1;
//       u64 shaderUniformBufferArrayDynamicIndexing:1;
//       u64 shaderSampledImageArrayDynamicIndexing:1;
//       u64 shaderStorageBufferArrayDynamicIndexing:1;
//       u64 shaderStorageImageArrayDynamicIndexing:1;
//       u64 shaderClipDistance:1;
//       u64 shaderCullDistance:1;
//       u64 shaderFloat64:1;
//       u64 shaderInt64:1;
//       u64 shaderInt16:1;
//       u64 shaderResourceResidency:1;
//       u64 shaderResourceMinLod:1;
//       u64 sparseBinding:1;
//       u64 sparseResidencyBuffer:1;
//       u64 sparseResidencyImage2D:1;
//       u64 sparseResidencyImage3D:1;
//       u64 sparseResidency2Samples:1;
//       u64 sparseResidency4Samples:1;
//       u64 sparseResidency8Samples:1;
//       u64 sparseResidency16Samples:1;
//       u64 sparseResidencyAliased:1;
//       u64 variableMultisampleRate:1;
//       u64 inheritedQueries:1;
//       u64 padding:9;
//
//       static constexpr RhiPhysicalDeviceFeatures defaultMask()
//       {
//           RhiPhysicalDeviceFeatures features {};
//           features.robustBufferAccess = 1;
//           features.fullDrawIndexUint32 = 1;
//           features.imageCubeArray = 1;
//           features.independentBlend = 1;
//           features.geometryShader = 1;
//           features.tessellationShader = 1;
//           features.logicOp = 1;
//           features.multiDrawIndirect = 1;
//           features.drawIndirectFirstInstance = 1;
//           features.depthClamp = 1;
//           features.depthBiasClamp = 1;
//           features.fillModeNonSolid = 1;
//           features.depthBounds = 1;
//           features.alphaToOne = 1;
//           features.samplerAnisotropy = 1;
//           return features;
//       }
//
//       static constexpr RhiPhysicalDeviceFeatures sparseResidencyMask()
//       {
//           RhiPhysicalDeviceFeatures features {};
//           features.sparseResidencyBuffer = 1;
//           features.sparseResidencyImage2D = 1;
//           features.sparseResidencyImage3D = 1;
//           features.sparseResidency2Samples = 1;
//           features.sparseResidency4Samples = 1;
//           features.sparseResidency8Samples = 1;
//           features.sparseResidency16Samples = 1;
//           features.sparseResidencyAliased = 1;
//           return features;
//       }
//
//       constexpr void enableDefault()
//       {
//           constexpr RhiPhysicalDeviceFeatures features = defaultMask();
//           *this |= features;
//       }
//
//       constexpr void enableSparseResidency()
//       {
//           constexpr RhiPhysicalDeviceFeatures features = sparseResidencyMask();
//           *this |= features;
//       }
//   };
//
//
//   RhiPhysicalDeviceFeatures get()
//   {
//       RhiPhysicalDeviceFeatures features {};
//       features.enableDefault();
//       features.enableSparseResidency();
//       return features;
//   }


#include <bitset>


#if (defined(_MSVC_LANG) && (_MSVC_LANG >= 201811L)) || __cplusplus >= 201811L
    #include <bit>
    #define BIT_CAST(T, arg)        std::bit_cast<T>(arg)
#else
    #define BIT_CAST(T, arg)        __builtin_bit_cast(T, arg)
#endif


#if defined(_MSC_VER)
    #define FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
    #define FORCEINLINE __attribute__((always_inline)) inline
#else
    #define FORCEINLINE inline
#endif


#define CONSTEXPRINLINE constexpr FORCEINLINE


template<typename T>
struct BitfieldOps
{

    CONSTEXPRINLINE friend T operator | (const T& a, const T& b)
    {
        using BitSetType = std::bitset<sizeof(T) * 8>;
        BitSetType bsA = BIT_CAST(BitSetType, a);
        BitSetType bsB = BIT_CAST(BitSetType, b);
        return BIT_CAST(T, bsA | bsB);
    }

    CONSTEXPRINLINE friend T operator & (const T& a, const T& b)
    {
        using BitSetType = std::bitset<sizeof(T) * 8>;
        BitSetType bsA = BIT_CAST(BitSetType, a);
        BitSetType bsB = BIT_CAST(BitSetType, b);
        return BIT_CAST(T, bsA & bsB);
    }

    CONSTEXPRINLINE friend T operator ^ (const T& a, const T& b)
    {
        using BitSetType = std::bitset<sizeof(T) * 8>;
        BitSetType bsA = BIT_CAST(BitSetType, a);
        BitSetType bsB = BIT_CAST(BitSetType, b);
        return BIT_CAST(T, bsA ^ bsB);
    }

    CONSTEXPRINLINE friend T operator ~(const T& a)
    {
        using BitSetType = std::bitset<sizeof(T) * 8>;
        BitSetType bsA = BIT_CAST(BitSetType, a);
        return BIT_CAST(T, ~bsA);
    }

    CONSTEXPRINLINE friend T& operator |= (T& a, const T& b)
    {
        a = a | b;
        return a;
    }

    CONSTEXPRINLINE friend T& operator &= (T& a, const T& b)
    {
        a = a & b;
        return a;
    }

    CONSTEXPRINLINE friend T& operator ^= (T& a, const T& b)
    {
        a = a ^ b;
        return a;
    }

    CONSTEXPRINLINE friend bool operator == (const T& a, const T& b)
    {
        using BitSetType = std::bitset<sizeof(T) * 8>;
        BitSetType bsA = BIT_CAST(BitSetType, a);
        BitSetType bsB = BIT_CAST(BitSetType, b);
        bsA ^= bsB;
        return bsA.none();
    }

    CONSTEXPRINLINE friend bool operator != (const T& a, const T& b)
    {
        using BitSetType = std::bitset<sizeof(T) * 8>;
        BitSetType bsA = BIT_CAST(BitSetType, a);
        BitSetType bsB = BIT_CAST(BitSetType, b);
        bsA ^= bsB;
        return bsA.any();
    }

    static CONSTEXPRINLINE bool bfopsContains(const T& a, const T& b)
    {
        using BitSetType = std::bitset<sizeof(T) * 8>;
        BitSetType bsA = BIT_CAST(BitSetType, a);
        BitSetType bsB = BIT_CAST(BitSetType, b);
        bsA &= bsB;
        bsA ^= bsB;
        return bsA.none();
    }

    static CONSTEXPRINLINE bool contains(const T& a, const T& b)
    {
        return bfopsContains(a, b);
    }

};
