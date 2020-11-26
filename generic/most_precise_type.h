// As an FYI, the useful-ness of this is debatable, but acts more as a reference as to how one might use fold expressions


#include <type_traits>
#include <cstdint>


template <typename T, typename... Ts>
constexpr bool contains_type() {
    return (
        false
        || ...
        || std::is_same_v<T, std::remove_cv_t<std::remove_reference_t<Ts>>>
    );
}


template <typename... BoolTs>
constexpr int count_bools(BoolTs... bools) {
    return (
        0
        + ...
        + int(bools)
    );
}

template <typename... Ts>
struct most_precise_type {

private:
    const static bool contains_long_double = contains_type<long double, Ts...>();
    const static bool contains_double = contains_type<double, Ts...>();
    const static bool contains_float = contains_type<float, Ts...>();

    const static bool contains_uint64 = contains_type<uint64_t, Ts...>();
    const static bool contains_int64 = contains_type<int64_t, Ts...>();

    const static bool contains_uint32 = contains_type<uint32_t, Ts...>();
    const static bool contains_int32 = contains_type<int32_t, Ts...>();

    const static bool contains_uint16 = contains_type<uint16_t, Ts...>();
    const static bool contains_int16 = contains_type<int16_t, Ts...>();

    const static bool contains_uint8 = contains_type<uint8_t, Ts...>();
    const static bool contains_int8 = contains_type<int8_t, Ts...>();


    const static bool use_long_double = (
        contains_long_double
        || (count_bools((contains_float || contains_double), contains_uint64, contains_int64) > 1)
    );

    const static bool use_uint64 = contains_uint64;
    const static bool use_int64 = (
        contains_int64
        || (contains_uint32 && contains_int32)
    );

    const static bool use_double = (
        contains_double
        || (count_bools(contains_float, contains_uint32, contains_int32) > 1)
    );

    const static bool use_float = contains_float;

    const static bool use_uint32 = contains_uint32;
    const static bool use_int32 = (
        contains_int32
        || (contains_uint16 && contains_int16)
    );

    const static bool use_uint16 = contains_uint16;
    const static bool use_int16 = (
        contains_int16
        || (contains_uint8 && contains_int8)
    );

    const static bool use_uint8 = contains_uint8;
    const static bool use_int8 = contains_int8;


public:

    using type = std::conditional_t<
        use_long_double,
        long double,
        std::conditional_t<
            use_uint64,
            uint64_t,
            std::conditional_t<
                use_int64,
                int64_t,
                std::conditional_t<
                    use_double,
                    double,
                    std::conditional_t<
                        use_float,
                        float,
                        std::conditional_t<
                            use_uint32,
                            uint32_t,
                            std::conditional_t<
                                use_int32,
                                int32_t,
                                std::conditional_t<
                                    use_uint16,
                                    uint16_t,
                                    std::conditional_t<
                                        use_int16,
                                        int16_t,
                                        std::conditional_t<
                                            use_uint8,
                                            uint8_t,
                                            std::conditional_t<
                                                use_int8,
                                                int8_t,
                                                void
                                            >
                                        >
                                    >
                                >           
                            >
                        >
                    >
                >
            >
        >
    >;
};

template <typename... Ts>
using most_precise_type_t = typename most_precise_type<Ts...>::type;


template <typename... Ts>
constexpr most_precise_type_t<Ts...> sum(const Ts... values) {
    return ( most_precise_type_t<Ts...>(0) + ... + most_precise_type_t<Ts...>(values));
}

template <typename... Ts>
constexpr most_precise_type_t<Ts...> product(const Ts... values) {
    return ( most_precise_type_t<Ts...>(1) * ... * most_precise_type_t<Ts...>(values));
}

