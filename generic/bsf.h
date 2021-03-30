#pragma once



// Can double as a log2 for values with only one signed bit

inline
unsigned int bsf(unsigned int m)
{
#if defined(__GNUC__) || defined(__CLANG__)
    return __builtin_ctz(m);
#elif defined(_MSC_VER)
    unsigned long v = 0;
    _BitScanForward(&v, (unsigned long)m);
    return v;
#else
    unsigned int value = 0;
    while (m >>= 1) ++value;
    return value;
#endif
}


inline
unsigned long bsf(unsigned long long m)
{
#if defined(__GNUC__) || defined(__CLANG__)
    return (unsigned long)__builtin_ctzl(m);
#elif defined(_MSC_VER)
    unsigned long v = 0;
    _BitScanForward64(&v, m);
    return v;
#else
    unsigned long long value = 0;
    while (m >>= 1) ++value;
    return (unsigned long)value;
#endif
}
