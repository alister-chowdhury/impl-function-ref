#pragma once


// NB: This doesn't generate a nice switch block!!!!!!!


// This contains helper functions to perform switch-like operations using templated variadic integers.
//
// * variadic_int_switch        - Template takes an explicit list of 'case' values.
// * variadic_int_range_switch  - Template takes a 'from' and 'until' argument and generates cases automatically,
//                                think pythons 'range' function.
//
//  Both values return bool signifying if a case was hit.
//
//  --
//
// `variadic_int_switch` Example usage:
//
//     variadic_int_switch<1, 2, 3, 4>(// predefined case labels
//         runtime_value,              // switch(runtime_value)
//         [](auto result) {           // 'case' function.
//             const static int value = decltype(result)::value;
//             my_other_thing<value>::do_things();
//         },
//         [](){                       // 'default' function (optional)
//             handle_not_found();
//         }
//     );
//
// `variadic_int_switch` may also use a specific int type via:
//
//     variadic_int_switch<specific_int_t, ...>
//
// --
//
// `variadic_int_range_switch` Example usage:
//
//     variadic_int_range_switch<
//          10,                        // 'from' (optional, default: 0)
//          20,                        // 'until'
//      >(
//         runtime_value,              // switch(runtime_value)
//         [](auto result) {           // 'case' function.
//             const static int value = decltype(result)::value;
//             my_other_thing<value>::do_things();
//         },
//         [](){                       // (Optional 'default' function)
//             handle_not_found();
//         }
//     );
//
// `variadic_int_range_switch` may also use a specific int type via:
//
//     variadic_int_range_switch<specific_int_t, ...>


#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <utility>


namespace detail {

// Container for the result
template<typename IntTypeT, IntTypeT value_>
struct variadic_int_value {
    const static IntTypeT value = value_;
};


// Implementation for user provided values
template <typename IntTypeT, IntTypeT... indexs, typename CallbackT, typename DefaultCaseT>
inline bool variadic_int_switch_impl(
        const IntTypeT index,
        CallbackT&& callback,
        DefaultCaseT&& default_case
) {

    bool matched = false;

    // Basically we're creating a list of integers as proxy.
    // Variadically we're comparing the provided index, and when it matches
    // executing the users callback, and nulling the result via the comma operator.
    // (For those that might not remember: 'int a= 1,2,3;' results in 'int a=3;')
    // Using a lambda that returns an int is visually simpler, sadly older versions of GCC
    // won't parse it correctly it would seem.

    #if 1
        std::initializer_list<int>(
            {
                (
                    index == indexs ?
                        (
                            (void)callback(variadic_int_value<IntTypeT, indexs>()), // call
                            (matched=true),                                         // writeback
                            0                                                       // The actual "evaluated value"
                        )
                        : 0
                )...
            }
        );


    // Lambda version, should it be something you'd rather use.
    // Effectively a top level dummy lambda is used for the sole purpose of taking arguments
    // from variadically called lambdas, which contain the comparison logic.
    #else
    
        [&](...){}(
            [&]()->int{
                if(index == indexs){
                    callback(variadic_int_value<IntTypeT, indexs>());
                    matched=true;
                }
                return 0;
            }
            ()...
        );


    #endif

    if(!matched) {
        default_case();
    }

    return matched;
}

// Implementation for switches given a range
template <typename IntTypeT, IntTypeT from_value, IntTypeT... indexs, typename CallbackT, typename DefaultCaseT>
inline bool variadic_int_range_switch_impl(
        const IntTypeT index,
        std::integer_sequence<IntTypeT, indexs...>,
        CallbackT&& callback,
        DefaultCaseT&& default_case
) {
    return variadic_int_switch_impl<IntTypeT, from_value+indexs...>(
        index,
        std::forward<CallbackT>(callback),
        std::forward<DefaultCaseT>(default_case)
    );
}

} // namespace detail


//// Explicit values
// Explicitly typed variety
template <
    typename IntTypeT,
    IntTypeT... values,
    typename CallbackT,
    typename DefaultCaseT=std::function<void(void)>
>
inline bool variadic_int_switch(
        const IntTypeT index,
        CallbackT&& callback,
        DefaultCaseT&& default_case=std::function<void(void)>([](){})
) {
    return detail::variadic_int_switch_impl<IntTypeT, values...>(
        index,
        std::forward<CallbackT>(callback),
        std::forward<DefaultCaseT>(default_case)
    );
};

// Auto Int
template <
    int... values,
    typename CallbackT,
    typename DefaultCaseT=std::function<void(void)>
>
inline bool variadic_int_switch(
        const int index,
        CallbackT&& callback,
        DefaultCaseT&& default_case=std::function<void(void)>([](){})
) {
    return variadic_int_switch<int, values...>(
        index,
        std::forward<CallbackT>(callback),
        std::forward<DefaultCaseT>(default_case)
    );
};

//// Ranges

// Explicitly typed variety
// From->Until
template <
    typename IntTypeT,
    IntTypeT from_value,
    IntTypeT until_value,
    typename CallbackT,
    typename DefaultCaseT=std::function<void(void)>
>
inline bool variadic_int_range_switch(
        const IntTypeT index,
        CallbackT&& callback,
        DefaultCaseT&& default_case=std::function<void(void)>([](){})
) {

    if(index < from_value || index >= until_value) {
        default_case();
        return false;
    }

    return detail::variadic_int_range_switch_impl<IntTypeT, from_value>(
        index,
        std::make_integer_sequence<IntTypeT, until_value-from_value>(),
        std::forward<CallbackT>(callback),
        std::forward<DefaultCaseT>(default_case)
    );
};

// 0->Until
template <
    typename IntTypeT,
    IntTypeT until_value,
    typename CallbackT,
    typename DefaultCaseT=std::function<void(void)>
>
inline bool variadic_int_range_switch(
        const IntTypeT index,
        CallbackT&& callback,
        DefaultCaseT&& default_case=std::function<void(void)>([](){})
) {
    return variadic_int_range_switch<IntTypeT, 0, until_value>(
        index,
        std::forward<CallbackT>(callback),
        std::forward<DefaultCaseT>(default_case)
    );
};


// Auto Int
// From->Until Range
template <
    int from_value,
    int until_value,
    typename CallbackT,
    typename DefaultCaseT=std::function<void(void)>
>
inline bool variadic_int_range_switch(
        const int index,
        CallbackT&& callback,
        DefaultCaseT&& default_case=std::function<void(void)>([](){})
) {
    return variadic_int_range_switch<int, from_value, until_value>(
        index,
        std::forward<CallbackT>(callback),
        std::forward<DefaultCaseT>(default_case)
    );
};

// 0->Until
template <
    int until_value,
    typename CallbackT,
    typename DefaultCaseT=std::function<void(void)>
>
inline bool variadic_int_range_switch(
        const int index,
        CallbackT&& callback,
        DefaultCaseT&& default_case=std::function<void(void)>([](){})
) {
    return variadic_int_range_switch<0, until_value>(
        index,
        std::forward<CallbackT>(callback),
        std::forward<DefaultCaseT>(default_case)
    );
};
