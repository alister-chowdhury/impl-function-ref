#pragma once


// This contains logic for compile-time value arrays.
//
// ct_array<T, T...>
// Type that wraps around an array of values.
//  `size()` returns the arrays size.
//  `empty()` returns if the array is empty.
//  `get(index)` can be used to fetch a value.
//  `sorted()` returns true if the array is sorted.
//  `find(value)` finds the first index of a value linearly, -1 if not found.
//  `contains(value)` returns true if the array contains a value.
//  `lower_bound(value)` does a lower bound binary search, only valid if sorted.
//
//  `mpl::get_multiple_t<array, indices...>`
//      Fetches multiple values from provided indices into a new ct_array
//
//  `mpl::get_range_t<Array, start, end>`
//      Fetches a range of values given a start and end.
//
//  `mpl::get_indirect_t<Array, IndexArray>`
//      Same as `mpl::get_multiple_t`, but uses an array for indices
//      rather than variadic arguments.
//
//  `mpl::get_masked_t<Array, BoolArray>`
//      Returns back an array using the indicies of the BoolArray that are
//      `true`.
//          using A = mpl::ct_array<int, 1, 2, 3, 4>;
//          using B = mpl::ct_array<bool, true, false, true, false>;
//          using C = mpl::get_masked_t<A, B>; // ct_array<int, 1, 3>;
// 
//  `mpl::make_mask_t<Array, predicate>`
//      Creates a ct_array<bool, ...> using a user provided predicate.
//          using A = mpl::ct_array<int, 1, 2, 3, 4>;
//          using B = mpl::make_mask_t<A, [](int x){ return x < 3;}>;
//                   // = ct_array<bool, true, true, false, false>;
//
//  `mpl::filter_t<Array, predicate>`
//      Filters for values using a user provided predicate.
//          using A = mpl::ct_array<int, 1, 2, 3, 4>;
//          using B = mpl::filter_t<A, [](int x){ return x < 3;}>;
//                   // = ct_array<int, 1, 2>;
//
//  `mpl::insert_back_t<Array, values...>`
//      Inserts multiple values to the end of an array.
//
//  `mpl::insert_front_t<Array, values...>`
//      Inserts multiple values to the start of an array.
//
//  `mpl::push_back_t<Array, values...>`
//      Inserts a value to the end of an array.
//
//  `mpl::push_front_t<Array, values...>`
//      Inserts a value to the start of an array.
//
//  `mpl::set_value_t<Array, index, value>`
//      Set the value at a given index of an array.
//
//  `mpl::convert_to_t<Array, type>`
//      Converts the arrays elements to a new type.
//
//  `mpl::transform_t<Array, type, transformer>`
//      Transform this array given a user provided function.
//
//          struct Data { int a; int b; };
//          using DataArray = mpl::ct_array<Data,
//                                          Data{.a=0, .b=0},
//                                          Data{.a=1, .b=0},
//                                          Data{.a=0, .b=1},
//                                          Data{.a=1, .b=1}>;
//
//          // = ct_array<int, 0, 1, 0, 1>;
//          using Avalues = mpl::transform_t<DataArray,
//                                           int,
//                                           [](Data x){ return x.a; }>;
//
//  `mpl::merge_back_t<ArrayA, ArrayB>`
//      Merges ArrayBs values to the end of ArrayA.
//
//  `mpl::merge_front_t<ArrayA, ArrayB>`
//      Merges ArrayBs values to the start of ArrayA.
//
//  `mpl::make_unique_t<Array>`
//      Removes any duplicate values.
//
//  `mpl::make_sorted_t<Array>`
//      Sorts the array.
//
//  `mpl::make_unique_sorted_t<Array>`
//      Sorts the array while also removing duplicates.


#include <cstdint>
#include <cstddef>
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


namespace mpl
{


template<typename Array, auto... indicies>
using get_multiple_t = typename Array::template get_multiple_t<indicies...>;

template<typename Array, auto start, auto end>
using get_range_t = typename Array::template get_range_t<start, end>;

template<typename Array, typename IndirectArray>
using get_indirect_t = typename Array::template get_indirect_t<IndirectArray>;

template<typename Array, typename MaskArray>
using get_masked_t = typename Array::template get_masked_t<MaskArray>;

template<typename Array, auto predicate>
using make_mask_t = typename Array::template get_masked_t<predicate>;

template<typename Array, auto predicate>
using filter_t = typename Array::template filter_t<predicate>;

template<typename Array, auto... newValues>
using insert_back_t = typename Array::template insert_back_t<newValues...>;

template<typename Array, auto... newValues>
using insert_front_t = typename Array::template insert_front_t<newValues...>;

template<typename Array, auto newValue>
using push_back_t = typename Array::template push_back_t<newValue>;

template<typename Array, auto newValue>
using push_front_t = typename Array::template push_front_t<newValue>;

template<typename Array, auto index, auto newValue>
using set_value_t = typename Array::template set_value_t<index, newValue>;

template<typename Array, typename U>
using convert_to_t = typename Array::template convert_to_t<U>;

template<typename Array, typename U, auto transformer>
using transform_t = typename Array::template transform_t<U, transformer>;

// Force B class to conform to As' type and class
template<typename A, typename B>
using merge_front_t = typename convert_to_t<B, typename A::element_type>
                        ::template merge_back_t<A>;

template<typename A, typename B>
using merge_back_t = typename convert_to_t<B, typename A::element_type>
                        ::template merge_front_t<A>;

template<typename Array>
using make_unique_t = typename Array::template make_unique_t<>;

template<typename Array>
using make_sorted_t = typename Array::template make_sorted_t<>;

template<typename Array>
using make_unique_sorted_t = typename Array::template make_unique_sorted_t<>;

template<size_t, typename>
struct cta_qsort_helper;

template<typename T, T... values>
struct ct_array
{
    using element_type = T;

    static CONSTEXPRINLINE size_t size()
    {
        return sizeof...(values);
    }
    
    static CONSTEXPRINLINE bool empty()
    {
        return size() == 0;
    }


    constexpr static inline T data[empty() ? 1 : size()] { values... };

    static CONSTEXPRINLINE bool sorted()
    {
        if constexpr(size() > 1)
        {
            for(size_t i=0; i < (size() - 1); ++i)
            {
                if(data[i] > data[i+1])
                {
                    return false;
                }
            }
        }
        return true;
    }

    static CONSTEXPRINLINE int find(const T value)
    {
        for(int i=0; i<sizeof...(values); ++i)
        {
            if(data[i] == value) { return i; }
        }
        return -1;
    }

    static CONSTEXPRINLINE bool contains(const T value)
    {
        return find(value) != -1;
    }

    template<int first_, int count_>
    static CONSTEXPRINLINE int lower_bound_iter(const T value)
    {
        if constexpr (count_ > 0)
        {
            constexpr int step = count_ / 2;
            constexpr int it = first_ + step;

            if(data[it] < value)
            {
                return lower_bound_iter<it+1, count_ - (step + 1)>(value);
            }
            else
            {
                return lower_bound_iter<first_, step>(value);
            }
        }
        else
        {
            return first_;
        }
    }

    static CONSTEXPRINLINE int lower_bound(const T value)
    {
        static_assert(sorted(), "Cannot lower_bound unsorted array!");
        return lower_bound_iter<0, int(size())>(value);
    }

    static CONSTEXPRINLINE T sum()
    {
        if constexpr(size() == 0)
        {
            return {};
        }
        else
        {
            return (... + values);
        }
    }

    template<typename U, U... indicies>
    static CONSTEXPRINLINE ct_array<T, data[indicies]...> get_indirect_helper(ct_array<U, indicies...>);

    template<typename FlagArray, typename U, U... helper_indicies>
    static CONSTEXPRINLINE auto get_masked_helper_inner(std::integer_sequence<U, helper_indicies...>)
    {
        static_assert(size() == FlagArray::size(), "Size mismatch!");

        struct Tmp
        {
            size_t indicies[sizeof...(helper_indicies)];
        };

        constexpr auto fetch = []
        {
            Tmp tmp {};
            size_t j = 0;
            for(size_t i=0; i<size(); ++i)
            {
                if(FlagArray::get(i))
                {
                    tmp.indicies[j++] = i;
                }
            }
            return tmp;
        };

        constexpr Tmp tmp = fetch();
        using IndirectArray = ct_array<size_t, tmp.indicies[helper_indicies]...>;
        return decltype(get_indirect_helper(IndirectArray{})){};
    }

    template<typename FlagArray>
    static CONSTEXPRINLINE auto get_masked_helper()
    {
        static_assert(size() == FlagArray::size(), "Size mismatch!");
    
        using AsSizeT = typename FlagArray::template convert_to_t<size_t>;
        constexpr size_t count = AsSizeT::sum();
        
        if constexpr(count == 0)
        {
            return ct_array<T>{};
        }
        else if constexpr(count == size())
        {
            return ct_array<T, values...>{};
        }
        else
        {
            return get_masked_helper_inner<FlagArray>(std::make_integer_sequence<size_t, count>{});
        }
    }

    static CONSTEXPRINLINE T get(auto index)
    {
        return data[index];
    }

    template<auto... indicies>
    using get_multiple_t = ct_array<T, get(indicies)...>;

    template<auto offset, typename U, U... indicies>
    constexpr static get_multiple_t<offset + indicies...> get_helper(std::integer_sequence<U, indicies...>);

    template<auto start, auto end>
    using get_range_t = decltype(get_helper<start>(std::make_integer_sequence<decltype(start), end-start>{}));

    template<typename Array>
    using get_indirect_t = decltype(get_indirect_helper(Array{}));

    template<typename Array>
    using get_masked_t = decltype(get_masked_helper<Array>());

    template<T... newValues>
    using insert_back_t = ct_array<T, values..., newValues...>;

    template<T... newValues>
    using insert_front_t = ct_array<T, newValues..., values...>;

    template<T newValue>
    using push_back_t = ct_array<T, values..., newValue>;

    template<T newValue>
    using push_front_t = ct_array<T, newValue, values...>;

    template<typename U>
    using convert_to_t = ct_array<U, U(values)...>;

    template<typename B>
    using merge_back_t = typename B::template insert_front_t<values...>;

    template<typename B>
    using merge_front_t = typename B::template insert_back_t<values...>;

    template<auto index, T newValue>
    using set_value_t = typename get_range_t<0, index>
                        ::template push_back_t<newValue>
                        ::template merge_back_t<get_range_t<index+1, size()>>
                        ;

    // Force evaluation to be deferred
    // Prevent calling find on instantiation if T doesnt have a == (clang)
    template<typename U, U... indicies, typename V=T, std::enable_if_t<std::is_same_v<V, T>, bool> = true>
    static CONSTEXPRINLINE get_masked_t<
        ct_array<bool, (ct_array<V, values...>::find(data[indicies]) == indicies)...>
    > get_unique_helper(std::integer_sequence<U, indicies...>);

    // Force evaluation to be deferred
    template<auto predicate, typename U=bool, std::enable_if_t<std::is_same_v<U, bool>, bool> = true>
    using make_mask_t = ct_array<U, predicate(values)...>;

    template<auto predicate, typename U=bool, std::enable_if_t<std::is_same_v<U, bool>, bool> = true>
    using filter_t = get_masked_t<make_mask_t<predicate, U>>;

    template<typename U, auto transformer>
    using transform_t = ct_array<U, transformer(values)...>;

    // Force evaluation to be deferred
    template<typename U=int, std::enable_if_t<std::is_same_v<U, int>, bool> = true>
    using make_unique_t = decltype(get_unique_helper(std::make_integer_sequence<U, size()>{}));

    template<typename U=T, std::enable_if_t<std::is_same_v<U, T>, bool> = true>
    using make_sorted_t = typename cta_qsort_helper<0, ct_array<U, values...>>::type;

    template<typename U=int, std::enable_if_t<std::is_same_v<U, int>, bool> = true>
    using make_unique_sorted_t = typename make_unique_t<U>::template make_sorted_t<>;

};


template<size_t depth, typename Array>
struct cta_qsort_helper
{
    static CONSTEXPRINLINE auto helper()
    {
        if constexpr(Array::sorted())
        {
            return Array{};
        }
        else
        {
            using lt = filter_t<Array, [](auto x){ return (x < Array::get(0)); }>;
            using eq = filter_t<Array, [](auto x){ return (x == Array::get(0)); }>;
            using gt = filter_t<Array, [](auto x){ return (x > Array::get(0)); }>;
            using merged = merge_back_t<
                typename cta_qsort_helper<depth+1, lt>::type,
                merge_back_t<
                    eq,
                    typename cta_qsort_helper<depth+1, gt>::type
                >
            >;
            return merged{};
        }
    }
   
    using type = decltype(helper());
};


}  // namespace mpl
