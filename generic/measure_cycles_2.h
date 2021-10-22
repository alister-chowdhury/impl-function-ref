#pragma once

#if defined(_MSC_VER) && defined(_M_X64)
    #include <intrin.h>
    #define NOINLINE __declspec(noinline)
    [[noreturn]] __forceinline void UNREACHABLE() {__assume(false);}
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    #include <x86intrin.h>
    #define NOINLINE __attribute__((noinline))
    [[noreturn]] inline __attribute__((always_inline)) void UNREACHABLE() {__builtin_unreachable();}
#else
    #error "Only really intended for x64 gcc/clang/msc"
#endif


#include <functional>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <thread>


NOINLINE int64_t measure_cycles2_raw(const std::function<void(void)>& callback)
{
#ifdef _MSC_VER
    int64_t first = __rdtsc();
    // MSVC refuses to not bool test std::function, but rolling our own std::function is a bit overkill
    if(!callback) { UNREACHABLE(); }
    else { callback(); }
    return __rdtsc() - first;
#else
    int64_t first = _rdtsc();
    if(!callback) { UNREACHABLE(); }
    else { callback(); }
    return _rdtsc() - first;
#endif
}


template<typename Callback>
NOINLINE int64_t measure_cycles2(Callback callback)
{
    // NB: Each dummy lambda resolves to a different function, this is on purpose.
    // Warmup spin
    measure_cycles2_raw([]{});

    // Calculate the overhead cost before and after the main callback, and subtract the average
    int64_t before =  measure_cycles2_raw([]{});
    int64_t target = measure_cycles2_raw(callback);
    int64_t after = measure_cycles2_raw([]{});
    return target - ((before + after) >> 1);
}


// Returns back [median, mad]
template<bool autoCorrection=true, typename Callback=void>
NOINLINE std::pair<int64_t, int64_t> measure_cycles2(Callback callback, uint32_t count, uint32_t warmup=10)
{
    if(count < 2) [[unlikely]] { return std::make_pair(-1, -1); }

    std::vector<int64_t> samples ( std::max(count, warmup) );
    int64_t* iter = &samples[0];
    for(uint32_t i=0; i<warmup; ++i)
    {
        int64_t value = measure_cycles2(callback);
        if(autoCorrection && value <= 0)
        {
            std::this_thread::yield();
            for(int i=0; i<3; ++i) { measure_cycles2(callback); }
            i -= 1;
        }
        else
        {
            iter[i] = value;
        }
    }
    for(uint32_t i=0; i<count; ++i)
    {
        int64_t value = measure_cycles2(callback);
        if(autoCorrection && value <= 0)
        {
            std::this_thread::yield();
            for(int i=0; i<3; ++i) { measure_cycles2(callback); }
            i -= 1;
        }
        else
        {
            iter[i] = value;
        }
    }

    // Calculate median
    std::sort(samples.begin(), samples.begin() + count);

    int64_t median = samples[count/2];
    if(count & 1)
    {
        median += samples[count/2 + 1];
        median >>= 1;
    }

    // Calculate MAD
    for(uint32_t i=0; i<count; ++i)
    {
        samples[i] = std::abs(samples[i] - median);
    }
    std::sort(samples.begin(), samples.begin() + count);
    int64_t mad = samples[count/2];
    if(count & 1)
    {
        mad += samples[count/2 + 1];
        mad >>= 1;
    }

    return std::make_pair(median, mad);
}



template<bool autoCorrection=true, typename Callback=void, typename ClearCallback=void>
NOINLINE std::pair<int64_t, int64_t> measure_cycles2(
    float tolerance,
    Callback callback,
    ClearCallback clearCallback,
    uint32_t count,
    uint32_t warmup=10)
{
    for(;;)
    {
        clearCallback();
        auto result = measure_cycles2<autoCorrection>(callback, count, warmup);
        if(float(result.second) / float(result.first) > tolerance)
        {
            continue;
        }
        return result;
    }
}
