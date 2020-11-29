#pragma once

#include <utility>
#include <tuple>
#include <type_traits>
#include <initializer_list>


// any_true<bool...>::value / any_true_v<bool...>
// all_true<bool...>::value / all_true_v<bool...>

// contains_type<needle, haystack...>::value / contains_type_v<needle, haystack>

// get_nth_type<index, types...>::type / get_nth_type_t<index, types>
// get_nth_value<index, value_t, values...>::value / get_nth_value_v<index, value_t, values...>;
// get_nth_integer_sequence_value<index, int_sequence_t>::value / get_nth_integer_sequence_value_v<index, int_sequence_t>
// get_last_value<value_t, values...>::value / get_last_value_v<value_t, values...>;
// get_last_integer_sequence_value<int_sequence_t>::value / get_last_integer_sequence_value_v<int_sequence_t>

// is_sorted<type, values...>::value / is_sorted_v<type, values...>
// is_sorted_integer_sequence<int_sequence_t>::value / is_sorted_integer_sequence<int_sequence_t>



// any_true / all_true
#if __cplusplus >= 201703L && 0 // Disabling fold expression impl for now
                                // GCC / Clang seem to scale much better
                                // with the template is_same variety.
template<bool... values>
struct any_true {
    const static bool value = (false || ... || values);
};

template<bool... values>
struct all_true {
    const static bool value = (true && ... && values);
};

#else

template<bool... values>
struct any_true {
    const static bool value = (sizeof...(values) > 0) && !std::is_same<
        std::integer_sequence<bool, values...>,             // possibly contains a true
        std::integer_sequence<bool, (values != values)...>  // all falses
    >::value;
};

template<bool... values>
struct all_true {
    const static bool value = std::is_same<
        std::integer_sequence<bool, values...>,             // possibly contains a false
        std::integer_sequence<bool, (values == values)...>  // all trues
    >::value;
};

#endif

template<bool... values>
constexpr bool any_true_v = any_true<values...>::value;

template<bool... values>
constexpr bool all_true_v = all_true<values...>::value;



// contains_type
template<typename NeedleT, typename... HayStackTs>
struct contains_type : any_true< std::is_same<NeedleT, HayStackTs>::value... >{};

template<typename NeedleT, typename... HayStackTs>
constexpr bool contains_type_v = contains_type<NeedleT, HayStackTs...>::value;



// get_nth_type
namespace detail {
    
    template<typename T>
    struct nth_type_helper { using type = T; };

    template<typename... Ts>
    using nth_type_tuple_proxy = std::tuple<nth_type_helper<Ts>...>;

};

template<std::size_t idx, typename... Ts>
struct get_nth_type {

    using type = typename std::tuple_element<
        idx,
        detail::nth_type_tuple_proxy<Ts...>
    >::type;

};

template<std::size_t idx, typename... Ts>
using get_nth_type_t = typename get_nth_type<idx, Ts...>::type;



// get_nth_value
namespace detail {
    
    template<typename T, T value_>
    struct nth_value_helper { const static T value = value_; };

    template<typename T, T... values>
    using nth_value_tuple_proxy = std::tuple<nth_value_helper<T, values>...>;

};

template<std::size_t idx, typename T, T... values>
struct get_nth_value {

private:
    using fetched_type = typename std::tuple_element<
        idx,
        detail::nth_value_tuple_proxy<T, values...>
    >::type;

public:
    const static T value = fetched_type::value;

};

template<std::size_t idx, typename T, T... values>
constexpr T get_nth_value_v = get_nth_value<idx, T, values...>::value;



// get_nth_integer_sequence_value
template<std::size_t index, typename IntegerSequenceT>
struct get_nth_integer_sequence_value {

private:
    template<typename T, T... values>
    static constexpr get_nth_value<index, T, values...> impl(std::integer_sequence<T, values...>);

public:
    const static auto value = decltype(impl(std::declval<IntegerSequenceT>()))::value;

};

template<std::size_t index, typename IntegerSequenceT>
constexpr auto get_nth_integer_sequence_value_v = get_nth_integer_sequence_value<index, IntegerSequenceT>::value;



// get_last_value

template<typename T, T... values>
struct get_last_value {

#if __cplusplus >= 201703L && 0 // Disabling fold expression impl for now
                                // Using the constexpr initializer list version seems
                                // to actually compile faster (GCC/Clang)

    const static T value = (values, ...);

#else

private:
    constexpr static T impl() {
        T value = 0;
        std::initializer_list<int>({
            (value=values, 0)...    // continue to writeback to value, the last value
        });                         // will ultimatley be the last thing written.
        return value;
    };

public:
    const static T value = impl();

#endif

};

template<typename T, T... values>
constexpr T get_last_value_v = get_last_value<T, values...>::value;


// get_last_integer_sequence_value
template<typename IntegerSequenceT>
struct get_last_integer_sequence_value {

private:
    template<typename T, T... values>
    static constexpr get_last_value<T, values...> impl(std::integer_sequence<T, values...>);

public:
    const static auto value = decltype(impl(std::declval<IntegerSequenceT>()))::value;

};

template<typename IntegerSequenceT>
constexpr auto get_last_integer_sequence_value_v = get_last_integer_sequence_value<IntegerSequenceT>::value;


// is_sorted
template<
    typename T,
    T... Ts
> struct is_sorted : std::true_type {};

template<
    typename T,
    T first, T... remaining
> struct is_sorted<T, first, remaining...> {

private:
    const static T last = get_last_value_v<T, first, remaining...>;
    using first_t = std::integer_sequence<T, first, remaining...>;
    using second_t = std::integer_sequence<T, remaining..., last>;  // simply duplicate the last value
                                                                    // rather than popping the last value
                                                                    // from remain (a <= b) remains valid.

    template<
        T... first_elements,
        T... second_elements
    >
    static constexpr 
    all_true<
        (first_elements <= second_elements)...
    >
    is_sorted_impl(
        std::integer_sequence<T, first_elements...>,
        std::integer_sequence<T, second_elements...>
    );


public:
    const static bool value = decltype(is_sorted_impl(
        std::declval<first_t>(),
        std::declval<second_t>()
    ))::value;
};

template<typename T, T... values>
constexpr bool is_sorted_v = is_sorted<T, values...>::value;



// is_sorted_integer_sequence
template<typename IntegerSequenceT>
struct is_sorted_integer_sequence {

private:
    template<typename T, T... values>
    static constexpr is_sorted<T, values...> impl(std::integer_sequence<T, values...>);

public:
    const static bool value = decltype(impl(std::declval<IntegerSequenceT>()))::value;

};

template<typename IntegerSequenceT>
constexpr bool is_sorted_integer_sequence_v = is_sorted_integer_sequence<IntegerSequenceT>::value;
