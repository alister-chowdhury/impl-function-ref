#pragma once

// TODO: This could be a bit of cleaning up

#include <cstddef>
#include <cstring>
#include <type_traits>


namespace detail {

    template<typename UintT>
    constexpr size_t get_uint_characters_needed() {

        const float characters_needed = 2.408239965311849 * sizeof(UintT);// 8*log10(2)
        size_t value = size_t(characters_needed);
        value += (value < characters_needed);
        return value;

    }

    template<typename UintT, size_t max_size, int j>
    struct stringify_uint_impl {
        
        __attribute__((always_inline)) static char* stringify(UintT x, char* out, char* b)
        {
            // Try to stop doing things ever power of 2 iterations
            // (Law of small numbers etc)
            const bool is_pow2 = (j & (j-1)) == 0;
            if(is_pow2 && (x<10))
            {
                b[j] = x;
                int i;
                for(i=j;i<(max_size-1) && b[i] == 0; ++i);
                for(;i<max_size; ++i){ *out++ = b[i] + '0'; }
                return out;
            }

            b[j] = (x % 10); x /= 10;
            return stringify_uint_impl<UintT, max_size, j-1>::stringify(
                x, out, b
            );
        }

    };

    template<typename UintT, size_t max_size>
    struct stringify_uint_impl<UintT, max_size, -1> {

        __attribute__((always_inline)) static char* stringify(UintT x, char* out, char* b)
        {
            int i;
            for(i=0;i<(max_size-1) && b[i] == 0; ++i);
            for(;i<max_size; ++i){ *out++ = b[i] + '0'; }
            return out;
        }

    };

}  // namespace detail

template<typename UintT>
struct stringify_uint {

    const static size_t max_size = detail::get_uint_characters_needed<UintT>();

    static char* stringify(UintT x, char* out)
    {
        if(x < 10)
        {
            *out++ = char(x) + '0';
            return out;
        }
        char b[max_size];

        return detail::stringify_uint_impl<UintT, max_size, max_size-1>::stringify(
            x, out, b
        );
    }

};

template<typename IntT>
struct stringify_int {

    using UintT = std::make_unsigned_t<IntT>;

    const static size_t max_size = stringify_uint<UintT>::max_size + 1;

        static char* stringify(IntT x, char* out)
    {
        if(x < 0)
        {
            *out++ = '-';
            x = -x;
        }

        return stringify_uint<UintT>::stringify(
            UintT(x), out
        );
    }

};


template<typename T>
__attribute__((always_inline)) inline
bool parse_int(const char* c, T& out)
{
    bool negate = false;

    if(std::is_signed<T>())
    {
        if(*c == '-')
        {
            negate = true;
            ++c;
        }
    }

    T rvalue = 0;
    while(unsigned char c0 = *c++)
    {
        rvalue *= 10;
        c0 -= '0';
        if(__builtin_expect(!!(c0 > 10), 0)) { return false; }
        rvalue += c0;
    }

    if(negate)
    {
        rvalue = -rvalue;
    }

    out = rvalue;
    return true;
}
