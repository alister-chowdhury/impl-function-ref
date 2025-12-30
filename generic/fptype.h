#include <cstdint>
#include <cstring>
#include <iostream>
#include <type_traits>

template <bool has_signed_, uint32_t e_bits_, uint32_t m_bits_>
struct fptype {
    const static bool has_signed = has_signed_;
    const static uint32_t s_bits = has_signed ? 1u : 0u;
    const static uint32_t e_bits = e_bits_;
    const static uint32_t m_bits = m_bits_;
    const static uint32_t num_bits = s_bits + e_bits + m_bits;

    static_assert((e_bits + m_bits) > 0, "Empty float type");
    static_assert(num_bits <= 64,
                  "Needs more bits than is supported. (max is 64)");
    using data_type = std::conditional_t<
        num_bits <= 8, uint8_t,
        std::conditional_t<
            num_bits <= 16, uint16_t,
            std::conditional_t<num_bits <= 32, uint32_t, uint64_t> > >;

    const static uint32_t m_st = 0;
    const static uint32_t e_st = m_st + m_bits;
    const static uint32_t s_st = e_st + e_bits;

    const static data_type s_msk = ((data_type(1) << s_bits) - data_type(1))
                                   << s_st;
    const static data_type e_msk = ((data_type(1) << e_bits) - data_type(1))
                                   << e_st;
    const static data_type m_msk = ((data_type(1) << m_bits) - data_type(1))
                                   << m_st;

    static_assert(
        (e_bits <= 11) && (m_bits <= 52),
        "Float requires more precision that is natively possible (double).");
    const static bool use_double_ = (e_bits > 8) || (m_bits > 23);

    using working_type = std::conditional_t<use_double_, double, float>;
    using working_data_type =
        std::conditional_t<use_double_, uint64_t, uint32_t>;
    const static uint32_t ws_bits = 1u;
    const static uint32_t we_bits = use_double_ ? 11u : 8u;
    const static uint32_t wm_bits = use_double_ ? 52u : 23u;

    const static uint32_t wm_st = 0;
    const static uint32_t we_st = wm_st + wm_bits;
    const static uint32_t ws_st = we_st + we_bits;

    const static working_data_type ws_msk =
        ((working_data_type(1) << ws_bits) - working_data_type(1)) << ws_st;
    const static working_data_type we_msk =
        ((working_data_type(1) << we_bits) - working_data_type(1)) << we_st;
    const static working_data_type wm_msk =
        ((working_data_type(1) << wm_bits) - working_data_type(1)) << wm_st;

    // Scaling factors for converting between our format at the working format.
    // The main purpose of this is to allow native hardware to deal with
    // denormals. e.g (float16 -> float32)
    //  lg2_prefix = 127
    //  lg2_scale  = 112
    //  scale down (from working format) = (127 - 112) << 23 = 1.92593e-34
    //  scale up   (to working format)   = (127 + 112) << 23 = 5.192297e+33
    const static working_data_type lg2_prefix =
        ((working_data_type(1) << we_bits) / working_data_type(2)) - 1;
    const static working_data_type lg2_scale =
        ((working_data_type(1) << we_bits) -
         (working_data_type(1) << e_bits)) >>
        1;

    static working_type from_working_scale() {
        constexpr working_data_type scale = (lg2_prefix - lg2_scale) << wm_bits;
        working_type f;
        std::memcpy(&f, &scale, sizeof(f));
        return f;
    }

    static working_type to_working_scale() {
        constexpr working_data_type invscale = (lg2_prefix + lg2_scale)
                                               << wm_bits;
        working_type f;
        std::memcpy(&f, &invscale, sizeof(f));
        return f;
    }

    static fptype from_working(working_type f) {
        f *= from_working_scale();
        if constexpr (!has_signed) {
            if (f < 0) {
                f = 0;
            }
        }
        working_data_type wdata;
        std::memcpy(&wdata, &f, sizeof(f));

        data_type data{};
        working_data_type s = wdata & ws_msk;
        working_data_type e = wdata & we_msk;
        working_data_type m = wdata & wm_msk;

        data |= data_type(s >> (ws_st - s_st));

        // nan / inf (including overflows)
        if (e >= (working_data_type(e_msk) << (we_st - e_st))) {
            // Overflown, clear the mantissa so this doesn't become
            // a NaN.
            if (e != we_msk) {
                m = 0;
            }
            // Is a NaN, make sure the lower bits don't get clipped
            // and become an Inf.
            else if (m) {
                m = wm_msk;
            }
            data |= e_msk;
        } else {
            data |= data_type(e >> (we_st - e_st));
        }
        data |= data_type(m >> (wm_bits - m_bits));
        return frombits(data);
    }

    working_type to_working() const {
        working_data_type wdata{};
        working_data_type s = data & s_msk;
        working_data_type e = data & e_msk;
        working_data_type m = data & m_msk;
        wdata |= s << (ws_st - s_st);
        wdata |= m << (wm_bits - m_bits);
        // inf / nan
        if (e == e_msk) {
            wdata |= we_msk;
        } else {
            wdata |= e << (we_st - e_st);
        }
        working_type f;
        std::memcpy(&f, &wdata, sizeof(f));
        return f * to_working_scale();
    }

    static fptype frombits(data_type d) {
        fptype r;
        r.data = d;
        return r;
    }

    constexpr fptype() = default;
    constexpr fptype(const fptype&) = default;
    fptype(working_type x) : data(from_working(x).data) {}

    fptype operator-() const { return (-((working_type) * this)); }

#define FPTYPE_ARITHM_OP(op)                                 \
    fptype operator op(const fptype x) const {               \
        return (((working_type) * this) op(working_type) x); \
    }                                                        \
    fptype operator op(const float x) const {                \
        return (((working_type) * this) op(working_type) x); \
    }                                                        \
    fptype operator op(const double x) const {               \
        return (((working_type) * this) op(working_type) x); \
    }
    FPTYPE_ARITHM_OP(+)
    FPTYPE_ARITHM_OP(-)
    FPTYPE_ARITHM_OP(*)
    FPTYPE_ARITHM_OP(/)
#undef FPTYPE_ARITHM_OP

#define FPTYPE_CMP_OP(op)                                    \
    bool operator op(const fptype x) const {                 \
        return (((working_type) * this) op(working_type) x); \
    }                                                        \
    bool operator op(const float x) const {                  \
        return (((working_type) * this) op(working_type) x); \
    }                                                        \
    bool operator op(const double x) const {                 \
        return (((working_type) * this) op(working_type) x); \
    }
    FPTYPE_CMP_OP(==)
    FPTYPE_CMP_OP(!=)
    FPTYPE_CMP_OP(<=)
    FPTYPE_CMP_OP(<)
    FPTYPE_CMP_OP(>=)
    FPTYPE_CMP_OP(>)
#undef FPTYPE_CMP_OP

    operator working_type() const { return to_working(); }

    data_type data{};
};

using float16 = fptype<true, 5, 10>;
using bfloat16 = fptype<true, 8, 7>;

// Tests
#include <cmath>
#include <limits>

#define EXPECT_EQ(a, b)                                                       \
    if ((a) != (b)) {                                                         \
        std::cout << "ERROR: Expected eq: " #a "==" #b " [" << a << ", " << b \
                  << "]\n";                                                   \
    }
#define EXPECT_NEQ(a, b)                                                       \
    if ((a) == (b)) {                                                          \
        std::cout << "ERROR: Expected neq: " #a "==" #b " [" << a << ", " << b \
                  << "]\n";                                                    \
    }
#define EXPECT_TRUE(a)                                 \
    if (!(a)) {                                        \
        std::cout << "ERROR: Expected true: " #a "\n"; \
    }
#define EXPECT_FALSE(a)                                 \
    if ((a)) {                                          \
        std::cout << "ERROR: Expected false: " #a "\n"; \
    }

void half_tests() {
    EXPECT_EQ(float16::frombits(0x0000), 0.0f);
    EXPECT_EQ(float16::frombits(0x0001),
              0.000000059604645f);                          // todo, denormals
    EXPECT_EQ(float16::frombits(0x03ff), 0.000060975552f);  // todo, denormals
    EXPECT_EQ(float16::frombits(0x0400), 0.00006103515625f);
    EXPECT_EQ(float16::frombits(0x3555), 0.33325195f);
    EXPECT_EQ(float16::frombits(0x3bff), 0.99951172f);
    EXPECT_EQ(float16::frombits(0x3c00), 1.0f);
    EXPECT_EQ(float16::frombits(0x3c01), 1.00097656f);
    EXPECT_EQ(float16::frombits(0x4000), 2.0f);
    EXPECT_EQ(float16::frombits(0x4248), 3.140625f);
    EXPECT_EQ(float16::frombits(0x7bff), 65504.0f);
    EXPECT_EQ(float16::frombits(0x7c00),
              std::numeric_limits<float>::infinity());

    EXPECT_EQ(float16(0.0f).data, 0x0000);
    EXPECT_EQ(float16(0.000000059604645f).data, 0x0001);  // todo, denormals
    EXPECT_EQ(float16(0.000060975552f).data, 0x03ff);     // todo, denormals
    EXPECT_EQ(float16(0.00006103515625f).data, 0x0400);
    EXPECT_EQ(float16(0.33325195f).data, 0x3555);
    EXPECT_EQ(float16(0.99951172f).data, 0x3bff);
    EXPECT_EQ(float16(1.0f).data, 0x3c00);
    EXPECT_EQ(float16(1.00097656f).data, 0x3c01);
    EXPECT_EQ(float16(2.0f).data, 0x4000);
    EXPECT_EQ(float16(3.140625f).data, 0x4248);
    EXPECT_EQ(float16(65504.0f).data, 0x7bff);
    EXPECT_EQ(float16(std::numeric_limits<float>::infinity()).data, 0x7c00);

    EXPECT_EQ(float16::frombits(0x0000 | 0x8000), -0.0f);
    EXPECT_EQ(float16::frombits(0x0001 | 0x8000),
              -0.000000059604645f);  // todo, denormals
    EXPECT_EQ(float16::frombits(0x03ff | 0x8000),
              -0.000060975552f);  // todo, denormals
    EXPECT_EQ(float16::frombits(0x0400 | 0x8000), -0.00006103515625f);
    EXPECT_EQ(float16::frombits(0x3555 | 0x8000), -0.33325195f);
    EXPECT_EQ(float16::frombits(0x3bff | 0x8000), -0.99951172f);
    EXPECT_EQ(float16::frombits(0x3c00 | 0x8000), -1.0f);
    EXPECT_EQ(float16::frombits(0x3c01 | 0x8000), -1.00097656f);
    EXPECT_EQ(float16::frombits(0x4000 | 0x8000), -2.0f);
    EXPECT_EQ(float16::frombits(0x4248 | 0x8000), -3.140625f);
    EXPECT_EQ(float16::frombits(0x7bff | 0x8000), -65504.0f);
    EXPECT_EQ(float16::frombits(0x7c00 | 0x8000),
              -std::numeric_limits<float>::infinity());

    EXPECT_EQ(float16(-0.0f).data, (0x0000 | 0x8000));
    EXPECT_EQ(float16(-0.000000059604645f).data,
              (0x0001 | 0x8000));  // todo, denormals
    EXPECT_EQ(float16(-0.000060975552f).data,
              (0x03ff | 0x8000));  // todo, denormals
    EXPECT_EQ(float16(-0.00006103515625f).data, (0x0400 | 0x8000));
    EXPECT_EQ(float16(-0.33325195f).data, (0x3555 | 0x8000));
    EXPECT_EQ(float16(-0.99951172f).data, (0x3bff | 0x8000));
    EXPECT_EQ(float16(-1.0f).data, (0x3c00 | 0x8000));
    EXPECT_EQ(float16(-1.00097656f).data, (0x3c01 | 0x8000));
    EXPECT_EQ(float16(-2.0f).data, (0x4000 | 0x8000));
    EXPECT_EQ(float16(-3.140625f).data, (0x4248 | 0x8000));
    EXPECT_EQ(float16(-65504.0f).data, (0x7bff | 0x8000));
    EXPECT_EQ(float16(-std::numeric_limits<float>::infinity()).data,
              (0x7c00 | 0x8000));

    EXPECT_EQ(float16(3.973643e-08f), 0.0f)
    EXPECT_EQ(float16(-3.973643e-08f), -0.0f)
    EXPECT_TRUE(std::isinf(float16(65536.0)));
    EXPECT_TRUE(std::isinf(float16(-65536.0)));
}

int main(void) { half_tests(); }