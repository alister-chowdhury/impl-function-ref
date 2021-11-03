#pragma once

// This contains a helper to dispatch ranges of values known at runtime to compile-time
// specific values.
//
// This is done by generating an indexable table, using each permutation as an ID,
// the result is an efficient switch-like lookup.
//
// Example usage:
//
//    enum class OptimizionMode
//    {
//        None,
//        Minimal,
//        Normal,
//        Full
//    };
//
//    enum class Format
//    {
//        BC1,
//        BC3,
//        BC7
//    };
//
//    template<OptimizionMode optMode, Format imageFormat>
//    struct ImageCompressor
//    {
//        static void run(const void* input, void* output)
//        {
//            // ... etc ...
//        }
//    };
//
//    void compressImage(OptimizionMode inOptMode,
//                        Format inImageFormat,
//                        const void* input,
//                        void* output)
//    {
//
//        using namespace variadic_dispatch;
//        dispatch<
//                Range<OptimizionMode, OptimizionMode::None, OptimizionMode::Full>,
//                Range<Format, Format::BC1, Format::BC7>
//        >(
//            [&](auto optMode, auto imageFormat)
//            {
//                using Compressor = ImageCompressor<optMode.value, imageFormat.value>;
//                Compressor::run(input, output);
//            },
//            inOptMode,
//            inImageFormat
//        );
//    }


#include <cstdint>
#include <type_traits>
#include <utility>


#if defined(_MSC_VER)
    #define FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
    #define FORCEINLINE __attribute__((always_inline)) inline
#else
    #define FORCEINLINE inline
#endif

#define CONSTEXPRINLINE constexpr FORCEINLINE


namespace variadic_dispatch
{

namespace detail
{


template<int32_t totalSize, typename... Dispatchables>
struct ChainedImpl
{
    template<typename F, int32_t index>
    static void dispatchFunc(F&& f)
    {
        f(typename Dispatchables::template integral<index>{}...);
    }


    template<typename F, int32_t... indicies>
    FORCEINLINE static void dispatchTableImpl(
            F&& f,
            int32_t index,
            std::integer_sequence<int32_t, indicies...>)
    {

        using fcall = void(*)(F&&);
        const static fcall table[sizeof...(indicies)]
        {
            dispatchFunc<F, indicies>...
        };
        table[index](std::forward<F>(f));

    }


    template<typename F>
    FORCEINLINE static void dispatchFromIndex(F&& f, int32_t index)
    {
        dispatchTableImpl<F>(
            std::forward<F>(f),
            index,
            std::make_integer_sequence<int32_t, totalSize>{}
        );
    }


    template<typename F, typename... Type>
    FORCEINLINE static bool dispatch(F&& f, Type... values)
    {
        const bool allInBounds = (Dispatchables::inBounds(values) && ...);
        if(!allInBounds)
        {
            return false;
        }

        const int32_t indexParts[sizeof...(Dispatchables)]
        {
            Dispatchables::index(values)...
        };

        int32_t index = 0;
        for(int32_t i=0; i<sizeof...(Dispatchables); ++i)
        {
            index += indexParts[i];
        }

        dispatchFromIndex(std::forward<F>(f), index);

        return true;
    }

};


template<typename... Ranges>
struct DispatcherImpl
{

    // Helper for generating strides which are constexpr and by extension usable
    // as template parameters
    struct StrideImpl
    {
        constexpr StrideImpl()
        {
            int32_t stride = 1;
            for(int32_t i=0; i<sizeof...(Ranges); ++i)
            {
                int32_t lastStride = stride;
                stride *= data[i];
                data[i] = lastStride;
            }
            totalSize = stride;
        }

        int32_t totalSize = 0;
        int32_t data[sizeof...(Ranges)] { Ranges::size... };
    };


    static constexpr StrideImpl strideData = {};


    template<int32_t... indicies>
    static constexpr auto makeStridesSequence(std::integer_sequence<int32_t, indicies...>)
    {
        return std::integer_sequence<int32_t, strideData.data[indicies]...>{};
    }

    template<int32_t... strides>
    static auto constexpr makeChainedImpl(std::integer_sequence<int32_t, strides...>)
    {
        return ChainedImpl<strideData.totalSize, typename Ranges::template Dispatchable<strides>...>{};
    }

    using Strides = decltype(makeStridesSequence(std::make_integer_sequence<int32_t, sizeof...(Ranges)>{}));
    using Chained = decltype(makeChainedImpl(Strides{}));

};


} // namespace detail


template<typename Type, Type start_, Type end_>
struct Range
{
    const static int32_t start = (int32_t)start_;
    const static int32_t end   = (int32_t)end_;
    const static int32_t size  = 1 + end - start;

    static_assert(end > start, "End must be greater than start!");

    // Class which encapsulates index <=> value transformations
    // in the context of a greater dispatch table
    template<int32_t stride>
    struct Dispatchable
    {
        template<int32_t index>
        const static Type value = (Type)((index / stride) % size + start);

        template<int32_t index>
        using integral = std::integral_constant<Type, value<index>>;

        static CONSTEXPRINLINE bool inBounds(const Type value_)
        {
            int32_t value = (int32_t)value_;
            return (value >= start) && (value <= end);
        }

        static CONSTEXPRINLINE int32_t index(const Type value_)
        {
            int32_t value = (int32_t)value_;
            return stride * (value - start);
        }
    };
};


template<typename... Ranges, typename Callback, typename... Values>
bool dispatch(Callback&& callback, Values... values)
{
    static_assert(sizeof...(Ranges) == sizeof...(values), "Range <=> Value size mismatch!");

    using Dispatcher = typename detail::DispatcherImpl<Ranges...>::Chained;

    return Dispatcher::dispatch(
        std::forward<Callback>(callback),
        values...
    );

}


} // namespace variadic_dispatch
