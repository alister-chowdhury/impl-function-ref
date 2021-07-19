// This is a system for encoding numbers (with the intention being they are drawn on a GPU)
// The way this works is by packing a u32 with 8 characters, or a u64 with 16 characters (4 bits per character).
// This is useful because it requires a fixed amount of storage and max triangle count.
// If the number doesn't fit into the budget (8 or 16 characters), engineering notation is used instead.
//
// Each characters value follows this mapping:
// 0  => '0'
// 1  => '1'
// 2  => '2'
// 3  => '3'
// 4  => '4'
// 5  => '5'
// 6  => '6'
// 7  => '7'
// 8  => '8'
// 9  => '9'
// 10 => 'e'
// 11 => '.'
// 12 => '+'
// 13 => '-'
// 14 => '#'
// 15 => ' '
//
// Example outputs:
//
// encodeNumber32(1.0)          => 1
// encodeNumber32(100000)       => 100000
// encodeNumber32(1234567890)   => 1.235e+9
// encodeNumber32(0.01)         => .01
// encodeNumber32(0.00052123)   => 5.212e-4
// encodeNumber32(-999999.999)  => -1000000
// encodeNumber32(-999999.9999) => -1.00e+7
//
//
// It should be straight forward enough to issue a draw(trianges=16) and given
// the vertex id, determine the character index and quads corner points.
// With the character value being [(encoded >> (4 * index)) & 0xf].
// This can be passed from vertex to fragment shader and sampled accordingly.
// Additionally, 0xf is empty, which allows for the possibility of generating
// degenerate triangles / limiting the draw count as a means to cull empty characters.


/////////////////////////////
// Things to put in a header

#include <cstdint>

// 0  => '0'
// 1  => '1'
// 2  => '2'
// 3  => '3'
// 4  => '4'
// 5  => '5'
// 6  => '6'
// 7  => '7'
// 8  => '8'
// 9  => '9'
// 10 => 'e'
// 11 => '.'
// 12 => '+'
// 13 => '-'
// 14 => '#'
// 15 => ' '
enum class SpecialCharacters : uint8_t
{
    E        = 10,
    DOT      = 11,
    PLUS     = 12,
    NEG      = 13,
    INVALID  = 14,
    EMPTY    = 15
};


// Encode numbers using a u32, with 8 characters
uint32_t    encodeNumber32(const float x);
uint32_t    encodeNumber32(const uint8_t x);
uint32_t    encodeNumber32(const int8_t x);
uint32_t    encodeNumber32(const uint16_t x);
uint32_t    encodeNumber32(const int16_t x);
uint32_t    encodeNumber32(const uint32_t x);
uint32_t    encodeNumber32(const int32_t x);
uint32_t    encodeNumber32(const uint64_t x);
uint32_t    encodeNumber32(const int64_t x);


// Encode numbers using a u64, with 16 characters
uint64_t    encodeNumber64(const float x);
uint64_t    encodeNumber64(const uint8_t x);
uint64_t    encodeNumber64(const int8_t x);
uint64_t    encodeNumber64(const uint16_t x);
uint64_t    encodeNumber64(const int16_t x);
uint64_t    encodeNumber64(const uint32_t x);
uint64_t    encodeNumber64(const int32_t x);
uint64_t    encodeNumber64(const uint64_t x);
uint64_t    encodeNumber64(const int64_t x);


// double isn't explicitly supported
inline uint32_t    encodeNumber32(const double x) { return encodeNumber32(float(x)); }
inline uint64_t    encodeNumber64(const double x) { return encodeNumber64(float(x)); }

// Helper to stringify and visualize the value
struct DecodedNumber32
{
    operator const char* () const { return data; }
    char data[9];
};

struct DecodedNumber64
{
    operator const char* () const { return data; }
    char data[17];
};

inline void stringifyEncodedNumber(const uint32_t data, char* output)
{
    const char* mapping = "0123456789e.+-#_";
    for(uint32_t i=0; i<8; ++i)
    {
        output[i] = (mapping[ (data >> (4*i)) & 0xf ]);
    }
    output[8] = 0;
}

inline void stringifyEncodedNumber(const uint64_t data, char* output)
{
    const char* mapping = "0123456789e.+-#_";
    for(uint32_t i=0; i<16; ++i)
    {
        output[i] = (mapping[ (data >> (4*i)) & 0xf ]);
    }
    output[16] = 0;
}

inline DecodedNumber32 stringifyEncodedNumber(const uint32_t data)
{
    DecodedNumber32 output;
    stringifyEncodedNumber(data, output.data);
    return output;
}

inline DecodedNumber64 stringifyEncodedNumber(const uint64_t data)
{
    DecodedNumber64 output;
    stringifyEncodedNumber(data, output.data);
    return output;
}


//////////////////////////////////////
// Things to put in a translation unit


#include <cmath>
#include <type_traits>
#include <cstring>


#if defined(_MSC_VER) && defined(_M_X64)
    #define ENC_FORCE_INLINE __forceinline
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    #define ENC_FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define ENC_FORCE_INLINE inline
#endif


namespace
{

// [-45, 38]
template<typename T>
const T POW_10_LUT[] = {
    1e-45, 1e-44, 1e-43, 1e-42,
    1e-41, 1e-40, 1e-39, 1e-38,
    1e-37, 1e-36, 1e-35, 1e-34,
    1e-33, 1e-32, 1e-31, 1e-30,
    1e-29, 1e-28, 1e-27, 1e-26,
    1e-25, 1e-24, 1e-23, 1e-22,
    1e-21, 1e-20, 1e-19, 1e-18,
    1e-17, 1e-16, 1e-15, 1e-14,
    1e-13, 1e-12, 1e-11, 1e-10,
    1e-09, 1e-08, 1e-07, 1e-06,
    1e-05, 1e-04, 1e-03, 1e-02,
    1e-01, 1e+00, 1e+01, 1e+02,
    1e+03, 1e+04, 1e+05, 1e+06,
    1e+07, 1e+08, 1e+09, 1e+10,
    1e+11, 1e+12, 1e+13, 1e+14,
    1e+15, 1e+16, 1e+17, 1e+18,
    1e+19, 1e+20, 1e+21, 1e+22,
    1e+23, 1e+24, 1e+25, 1e+26,
    1e+27, 1e+28, 1e+29, 1e+30,
    1e+31, 1e+32, 1e+33, 1e+34,
    1e+35, 1e+36, 1e+37, 1e+38
};


inline float fpow10(int32_t n)
{
    if(n < -45) { return 0; }
    if(n > 38 ) { n = 38; } // 1e+38 = inf with float
   return POW_10_LUT<float>[n + 45];
}


inline int32_t approxFloorLog10(float x)
{
    int32_t bits;
    std::memcpy(&bits, &x, sizeof(int32_t));

    double approxLog2 = double(((bits >> 23) & 0xff) - 127);
    int32_t approxLog10 = int32_t(
        approxLog2
        * 0.3010299956639811952137  // log10(2)
    );

    if(x / fpow10(approxLog10) < 1.0f)
    {
        --approxLog10;
    }

    return approxLog10;
}


inline float fractInputReturnFloor(float& input)
{
    float floored = std::floor(input);
    input -= floored;
    return floored;
}


template<typename T>
struct RepBuffer
{
    const static uint32_t   capacity = 2 * sizeof(T);

    template<typename U>
    RepBuffer& push(U value)
    {
        data |= (~T(value) & T(0b1111)) << (4 * index++);
        return *this;
    }

    template<typename... Us>
    RepBuffer& push(Us... values)
    {
        T        localData = 0;
        uint32_t localIndex = 0;

        auto pushLocal = [&](T value)
        {
            localData |= (~value & T(0b1111)) << (4 * localIndex++);
        };

        ( (void)pushLocal(T(values)), ... );

        localData <<= (4 * index);

        data |= localData;
        index += localIndex;

        return *this;
    }

    template<typename U>
    RepBuffer& pushFromBuffer(U* values, const uint32_t count)
    {
        T        localData = 0;
        uint32_t localIndex = 0;

        if constexpr(capacity == 8)
        {
            switch(count)
            {
                default:
                case 8:    localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 7:    localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 6:    localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 5:    localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 4:    localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 3:    localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 2:    localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 1:    localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 0:    break;
            }
        }
        else if constexpr(capacity == 16)
        {
            switch(count)
            {
                default:
                case 16:   localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 15:   localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 14:   localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 13:   localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 12:   localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 11:   localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 10:   localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 9:    localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 8:    localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 7:    localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 6:    localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 5:    localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 4:    localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 3:    localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 2:    localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 1:    localData |= (~values[localIndex] & T(0b1111)) << (4 * localIndex++);
                case 0:    break;
            }
        }
        else
        {
            for(size_t i=0; i<count; ++i)
            {
                localData |= (~values[i] & T(0b1111)) << (4 * localIndex++);
            }
        }

        localData <<= (4 * index);
        data |= localData;
        index += count;

        return *this;
    }

    RepBuffer& pop(uint32_t count)
    {
        T mask = ~T(0);
        mask >>= ((count - index) * 4);

        data &= mask;
        if(index > count)
        {
            index -= count;
        }
        else
        {
            index = 0;
        }

        return *this;
    }

    template<typename... Us>
    static T encode(Us... values)
    {
        return RepBuffer{}.push(values...).get();
    }

    static T getZero()
    {
        return encode(0, SpecialCharacters::DOT, 0);
    }

    static T getNan()
    {
        return encode(SpecialCharacters::INVALID,
                      SpecialCharacters::DOT,
                      SpecialCharacters::INVALID);
    }

    static T getPosInf()
    {
        RepBuffer buf;
        buf.push(SpecialCharacters::PLUS,
                 9,
                 SpecialCharacters::E,
                 SpecialCharacters::PLUS);
        for(uint32_t i=4; i<capacity; ++i)
        {
            buf.push(9);
        }
        return buf.get();
    }

    static T getNegInf()
    {
        RepBuffer buf;
        buf.push(SpecialCharacters::NEG,
                 9,
                 SpecialCharacters::E,
                 SpecialCharacters::PLUS);
        for(uint32_t i=4; i<capacity; ++i)
        {
            buf.push(9);
        }
        return buf.get();
    }

    T get() const { return ~data; }


    uint32_t remainingSpace() const
    {
        return capacity - index;
    }

    T           data = 0;
    uint32_t    index = 0;
};


template<int32_t n, typename T>
constexpr T ctpow10()
{
    if(n == 0) { return T(1); }
    if(n == 1) { return T(10); }
    constexpr int32_t nPos = (n < 0 ? -n : n);
    T half = ctpow10<nPos/2, T>();
    T joined = half * half * (nPos & 1 ? T(10) : T(1));
    return (n < 0 ? T(1)/joined : joined); 
}


template<uint32_t budget, typename T>
constexpr bool requiresEngineerNotation(T value)
{
    // If the type always requires less than the budget, just return false;
    if constexpr(std::is_integral<T>::value)
    {
        constexpr double intLog2 = sizeof(value) * 8;
        constexpr double intMaxDigits = (
            intLog2
            * 0.30102999566 // log10(2)
        );

        if constexpr( intMaxDigits < double(budget - std::is_signed<T>::value) )
        {
            return false;
        }
    }

    // Assume 0 can always be represented without eng notation
    if(value == T(0) || value == T(-0)) return false;

    T bigSidePos = ctpow10<int32_t(budget), T>();
    T bigSideNeg = ctpow10<int32_t(budget-1), T>();    // requires -

    // These are for floats
    T smallSidePos = ctpow10<-2, T>(); // requres 0.
    T smallSideNeg = ctpow10<-3, T>(); // requires -0.

    if(value < 0)
    {
        value = -value;
        return !(value < bigSideNeg && value >= smallSideNeg);
    }
    return !(value < bigSidePos && value >= smallSidePos);
}


// Based off https://github.com/alister-chowdhury/impl-function-ref/blob/master/generic/stringify_int.h
namespace detail {


    template<typename UintT, typename RepType, size_t max_size, int j>
    struct encodeUintImpl
    {
        
        ENC_FORCE_INLINE static void encode(UintT x, RepType& out, uint8_t* b)
        {
            // Try to stop doing things ever power of 2 iterations
            // (Law of small numbers etc)
            const bool is_pow2 = (j & (j-1)) == 0;
            if(is_pow2 && (x<10))
            {
                b[j] = x;
                int i;
                // Determine how many null chars were written
                // and backtrack.
                for(i=j;i<(max_size-1) && b[i] == 0; ++i);
                // Copy to the final output
                out.pushFromBuffer(&b[i], max_size-i);
                return;
            }

            b[j] = (x % 10); x /= 10;
            encodeUintImpl<UintT, RepType, max_size, j-1>::encode(
                x, out, b
            );
        }

    };

    // Tail call, all digits are used
    template<typename UintT, typename RepType, size_t max_size>
    struct encodeUintImpl<UintT, RepType, max_size, -1> {

        ENC_FORCE_INLINE static void encode(UintT x, RepType& out, uint8_t* b)
        {
            int i;
            // Determine how many null chars were written
            // and backtrack.
            for(i=0;i<(max_size-1) && b[i] == 0; ++i);
            out.pushFromBuffer(&b[i], max_size-i);
            return;
        }

    };


    template<typename UintT, typename RepType>
    struct encodeUint
    {
        const static size_t max_size = RepType::capacity;

        ENC_FORCE_INLINE static void encode(UintT x, RepType& output)
        {
            if(x < 10)
            {
                output.push(x);
                return;
            }

            uint8_t b[max_size];
            encodeUintImpl<UintT, RepType, max_size, max_size-1>::encode(
                x, output, b
            );
        }
    };

    template<typename IntT, typename RepType>
    struct encodeInt
    {
        using UintT = std::make_unsigned_t<IntT>;

        ENC_FORCE_INLINE static void encode(IntT x, RepType& output)
        {
            if(x < 0)
            {
                output.push(SpecialCharacters::NEG);
                x = -x;
            }
            encodeUint<UintT, RepType>::encode(
                UintT(x), output
            );
        }
    };

}  // namespace detail


// ints and uints
template<typename RepType, typename NumberT>
ENC_FORCE_INLINE RepType encodeWholeNumber(NumberT x)
{
    RepType output;

    if constexpr(std::is_signed<NumberT>::value)
    {
        detail::encodeInt<NumberT, RepType>::encode(x, output);
    }
    else
    {
        detail::encodeUint<NumberT, RepType>::encode(x, output);
    }

    return output;
}


template<typename RepType>
ENC_FORCE_INLINE RepType encodeWholeNumber(float x)
{
    RepType output;

    if(x < 0)
    {
        output.push(SpecialCharacters::NEG);
        x = -x;
    }

    const bool isWhole = std::floor(x) == x;
    int32_t e10 = approxFloorLog10(x);
    float d10 = fpow10(-e10);

    // Scale down
    x *= d10;

    // Apply rounding logic
    x += 0.5f * fpow10(-int32_t(output.remainingSpace()) + 2);

    // Deal with really odd case
    // where we round up enough to
    // change our current number
    if(x >= 10.0f)
    {
        x *= 0.1f;
        ++e10;
    }

    // Numbers >= 1, will also omit 0 for decimal numbers
    if(e10 >= 0)
    {
        for(int32_t i=0; i<=e10; ++i)
        {
            int decimal = int(fractInputReturnFloor(x));
            x *= 10.0f;
            output.push(decimal);
        }

        // stop on whole numbers or if we'd just write a single decimal place
        if(isWhole || (output.remainingSpace() <= 1))
        {
            return output;
        }        
    }
    // Decimals
    {
        // Include decimal place as zero we wish to strip
        uint32_t writtenZeroes = 1;
        output.push(SpecialCharacters::DOT);

        // Fill in 0's
        for(int32_t i=0; i<(-e10-1); ++i)
        {
            output.push(0);
            ++writtenZeroes;
        }

        // Use the remaining space for anything left
        uint32_t budget = output.remainingSpace();
        for(uint32_t i=0; i<budget; ++i)
        {
            int decimal = int(fractInputReturnFloor(x));
            x *= 10.0f;
            if(decimal == 0)
            {
                ++writtenZeroes;
            }
            else
            {
                writtenZeroes = 0;
            }
            output.push(decimal);
        }

        // Clear trailing 0's and possibly the decimal place
        output.pop(writtenZeroes);
    }

    return output;
}


template<typename RepType>
ENC_FORCE_INLINE RepType encodeEngNotation(float x)
{
    RepType output;

    if(x < 0)
    {
        output.push(SpecialCharacters::NEG);
        x = -x;
    }

    int32_t e10 = approxFloorLog10(x);
    float d10 = fpow10(-e10);

    // Scale down
    x *= d10;

    uint32_t budget = output.remainingSpace();

    // X.e+X
    budget -= 5;
    if(std::abs(e10) >= 10)
    {
        budget -= 1;
    }

    // Apply rounding logic
    x += 0.5f * fpow10(-int32_t(budget));

    // Deal with really odd case
    // where we round up enough to
    // change our current number
    if(x >= 10.0f)
    {
        x *= 0.1f;
        // Even odder case where our budget decreases
        if(++e10 == 10)
        {
            budget -= 1;
        }
    }

    // First number and a dot
    {
        int decimal = int(fractInputReturnFloor(x));
        x *= 10.0f;
        output.push(decimal, SpecialCharacters::DOT);
    }

    while(budget != 0)
    {
        int decimal = int(fractInputReturnFloor(x));
        x *= 10.0f;
        output.push(decimal);
        --budget;
    }

    output.push(
        SpecialCharacters::E,
        (e10 < 0) ? SpecialCharacters::NEG : SpecialCharacters::PLUS
    );

    if(e10 < 0)
    {
        e10 = -e10;
    }
    // NB: We only handle two digit exponents (which is fine for floats and doubles)
    if(e10 >= 10)
    {
        output.push(e10 / 10, e10 % 10);
    }
    else
    {
        output.push(e10 % 10);
    }

    return output;
}


template<typename StorageT, typename NumberT>
StorageT encodeNumber(const NumberT x)
{
    using RepT = RepBuffer<StorageT>;

    if(x == 0)
    {
        return RepT::getZero();
    }

    if(requiresEngineerNotation<RepT::capacity>(x))
    {
        return encodeEngNotation<RepT>(x).get();
    }

    return encodeWholeNumber<RepT>(x).get();
}


template<typename StorageT>
StorageT encodeNumber(const float x)
{
    using RepT = RepBuffer<StorageT>;

    if(x == 0)
    {
        return RepT::getZero();
    }

    if(std::isnan(x)) [[unlikely]]
    {
        return RepT::getNan();
    }

    if(std::isinf(x)) [[unlikely]]
    {
        if(std::signbit(x))
        {
            return RepT::getNegInf();
        }
        return RepT::getPosInf();
    }

    if(requiresEngineerNotation<RepT::capacity>(x))
    {
        return encodeEngNotation<RepT>(x).get();
    }

    return encodeWholeNumber<RepT>(x).get();
}

} // unnamed namespace


// Public functions
uint32_t    encodeNumber32(const float x) { return encodeNumber<uint32_t>(x); }
uint32_t    encodeNumber32(const uint8_t x) { return encodeNumber<uint32_t>(x); }
uint32_t    encodeNumber32(const int8_t x) { return encodeNumber<uint32_t>(x); }
uint32_t    encodeNumber32(const uint16_t x) { return encodeNumber<uint32_t>(x); }
uint32_t    encodeNumber32(const int16_t x) { return encodeNumber<uint32_t>(x); }
uint32_t    encodeNumber32(const uint32_t x) { return encodeNumber<uint32_t>(x); }
uint32_t    encodeNumber32(const int32_t x) { return encodeNumber<uint32_t>(x); }
uint32_t    encodeNumber32(const uint64_t x) { return encodeNumber<uint32_t>(x); }
uint32_t    encodeNumber32(const int64_t x) { return encodeNumber<uint32_t>(x); }


// Encode numbers using a u64, with 16 characters
uint64_t    encodeNumber64(const float x) { return encodeNumber<uint64_t>(x); }
uint64_t    encodeNumber64(const uint8_t x) { return encodeNumber<uint64_t>(x); }
uint64_t    encodeNumber64(const int8_t x) { return encodeNumber<uint64_t>(x); }
uint64_t    encodeNumber64(const uint16_t x) { return encodeNumber<uint64_t>(x); }
uint64_t    encodeNumber64(const int16_t x) { return encodeNumber<uint64_t>(x); }
uint64_t    encodeNumber64(const uint32_t x) { return encodeNumber<uint64_t>(x); }
uint64_t    encodeNumber64(const int32_t x) { return encodeNumber<uint64_t>(x); }
uint64_t    encodeNumber64(const uint64_t x) { return encodeNumber<uint64_t>(x); }
uint64_t    encodeNumber64(const int64_t x) { return encodeNumber<uint64_t>(x); }
