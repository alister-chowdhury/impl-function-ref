#pragma once


// This contains logic for compile-time value arrays and compile-time type arrays.
//
// ctva::ctv_array<T, T...>
// Type that wraps around an array of values.
//  `size()` returns the arrays size.
//  `empty()` returns if the array is empty.
//  `get(index)` can be used to fetch a value.
//  `sorted()` returns true if the array is sorted.
//  `find(value)` finds the first index of a value linearly, -1 if not found.
//  `contains(value)` returns true if the array contains a value.
//  `lower_bound(value)` does a lower bound binary search, only valid if sorted.
//
//  `ctva::get_multiple_t<array, indices...>`
//      Fetches multiple values from provided indices into a new ctv_array
//
//  `ctva::get_range_t<Array, start, end>`
//      Fetches a range of values given a start and end.
//
//  `ctva::get_indirect_t<Array, IndexArray>`
//      Same as `ctva::get_multiple_t`, but uses an array for indices
//      rather than variadic arguments.
//
//  `ctva::get_masked_t<Array, BoolArray>`
//      Returns back an array using the indicies of the BoolArray that are
//      `true`.
//          using A = ctva::ctv_array<int, 1, 2, 3, 4>;
//          using B = ctva::ctv_array<bool, true, false, true, false>;
//          using C = ctva::get_masked_t<A, B>; // ctv_array<int, 1, 3>;
// 
//  `ctva::make_mask_t<Array, predicate>`
//      Creates a ctv_array<bool, ...> using a user provided predicate.
//          using A = ctva::ctv_array<int, 1, 2, 3, 4>;
//          using B = ctva::make_mask_t<A, [](int x){ return x < 3;}>;
//                   // = ctv_array<bool, true, true, false, false>;
//
//  `ctva::filter_t<Array, predicate>`
//      Filters for values using a user provided predicate.
//          using A = ctva::ctv_array<int, 1, 2, 3, 4>;
//          using B = ctva::filter_t<A, [](int x){ return x < 3;}>;
//                   // = ctv_array<int, 1, 2>;
//
//  `ctva::insert_back_t<Array, values...>`
//      Inserts multiple values to the end of an array.
//
//  `ctva::insert_front_t<Array, values...>`
//      Inserts multiple values to the start of an array.
//
//  `ctva::push_back_t<Array, value>`
//      Inserts a value to the end of an array.
//
//  `ctva::push_front_t<Array, value>`
//      Inserts a value to the start of an array.
//
//  `ctva::set_value_t<Array, index, value>`
//      Set the value at a given index of an array.
//
//  `ctva::convert_to_t<Array, type>`
//      Converts the arrays elements to a new type.
//
//  `ctva::transform_t<Array, type, transformer>`
//      Transform this array given a user provided function.
//
//          struct Data { int a; int b; };
//          using DataArray = ctva::ctv_array<Data,
//                                          Data{.a=0, .b=0},
//                                          Data{.a=1, .b=0},
//                                          Data{.a=0, .b=1},
//                                          Data{.a=1, .b=1}>;
//
//          // = ctv_array<int, 0, 1, 0, 1>;
//          using Avalues = ctva::transform_t<DataArray,
//                                           int,
//                                           [](Data x){ return x.a; }>;
//
//  `ctva::merge_back_t<ArrayA, ArrayB>`
//      Merges ArrayBs values to the end of ArrayA.
//
//  `ctva::merge_front_t<ArrayA, ArrayB>`
//      Merges ArrayBs values to the start of ArrayA.
//
//  `ctva::make_unique_t<Array>`
//      Removes any duplicate values.
//
//  `ctva::make_sorted_t<Array>`
//      Sorts the array.
//
//  `ctva::make_unique_sorted_t<Array>`
//      Sorts the array while also removing duplicates.
//
//
// ctta::ctt_array<T, T...>
// Type that wraps around an array of types.
//  `size()` returns the arrays size.
//  `empty()` returns if the array is empty.
//  `find<T>()` finds the first index of a type linearly, -1 if not found.
//  `contains<T>()` returns true if the array contains a type.
//
//  `ctta::get_t<array, index>`
//      Fetch a single type.
//
//  `ctta::get_multiple_t<array, indices...>`
//      Fetches multiple types from provided indices into a new ctt_array
//
//  `ctta::get_range_t<Array, start, end>`
//      Fetches a range of values given a start and end.
//
//  `ctta::get_indirect_t<Array, IndexArray>`
//      Same as `ctta::get_multiple_t`, but uses an ctv_array for indices
//      rather than variadic arguments.
//
//  `ctta::get_masked_t<Array, BoolArray>`
//      Returns back an array using the indicies of the BoolArray that are
//      `true`.
//          using A = ctta::cta_array<char, float, double, int>;
//          using B = ctva::ctv_array<bool, true, false, true, false>;
//          using C = ctta::get_masked_t<A, B>; // ctt_array<char, double>;
// 
//  `ctta::make_mask_t<Array, predicate>`
//      Creates a ctv_array<bool, ...> using a user provided predicate.
//          using A = ctta::ctt_array<int, float, char, int>;
//          // = ctv_array<bool, true, false, true, true>;
//          using B = ctta::make_mask_t<
//                  A,
//                  [](auto x){
//                      using T = typename decltype(x) :: type;
//                      return !std::is_same_v<T, float>;
//                  }
//          >;
//
//  `ctta::filter_t<Array, predicate>`
//      Filters for types using a user provided predicate.
//          using A = ctta::ctt_array<int, float, char, int>;
//          // = ctt_array<int, char, int>;
//          using B = ctta::filter_t<
//                  A,
//                  [](auto x){
//                      using T = typename decltype(x) :: type;
//                      return !std::is_same_v<T, float>;
//                  }
//          >;
//
//  `ctta::insert_back_t<Array, T...>`
//      Inserts multiple types to the end of an array.
//
//  `ctta::insert_front_t<Array, T...>`
//      Inserts multiple types to the start of an array.
//
//  `ctta::push_back_t<Array, T>`
//      Inserts a type to the end of an array.
//
//  `ctta::push_front_t<Array, T>`
//      Inserts a type to the start of an array.
//
//  `ctta::set_value_t<Array, index, T>`
//      Set the type at a given index of an array.
//
//  `ctta::convert_to_t<Array, type>`
//      Converts the arrays elements to a new type.
//
//  `ctta::merge_back_t<ArrayA, ArrayB>`
//      Merges ArrayBs types to the end of ArrayA.
//
//  `ctta::merge_front_t<ArrayA, ArrayB>`
//      Merges ArrayBs typrs to the start of ArrayA.
//
//  `ctta::make_unique_t<Array>`
//      Removes any duplicate types.


#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <tuple>
#include <utility>


#if defined(_MSC_VER)
    #define FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
    #define FORCEINLINE __attribute__((always_inline)) inline
#else
    #define FORCEINLINE inline
#endif
#define CONSTEXPRINLINE constexpr FORCEINLINE


namespace ctva
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
struct ctv_array
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
    static CONSTEXPRINLINE ctv_array<T, data[indicies]...> get_indirect_helper(ctv_array<U, indicies...>);

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
        using IndirectArray = ctv_array<size_t, tmp.indicies[helper_indicies]...>;
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
            return ctv_array<T>{};
        }
        else if constexpr(count == size())
        {
            return ctv_array<T, values...>{};
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
    using get_multiple_t = ctv_array<T, get(indicies)...>;

    template<auto offset, typename U, U... indicies>
    constexpr static get_multiple_t<offset + indicies...> get_helper(std::integer_sequence<U, indicies...>);

    template<auto start, auto end>
    using get_range_t = decltype(get_helper<start>(std::make_integer_sequence<decltype(start), end-start>{}));

    template<typename Array>
    using get_indirect_t = decltype(get_indirect_helper(Array{}));

    template<typename Array>
    using get_masked_t = decltype(get_masked_helper<Array>());

    template<T... newValues>
    using insert_back_t = ctv_array<T, values..., newValues...>;

    template<T... newValues>
    using insert_front_t = ctv_array<T, newValues..., values...>;

    template<T newValue>
    using push_back_t = ctv_array<T, values..., newValue>;

    template<T newValue>
    using push_front_t = ctv_array<T, newValue, values...>;

    template<typename U>
    using convert_to_t = ctv_array<U, U(values)...>;

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
        ctv_array<bool, (ctv_array<V, values...>::find(data[indicies]) == indicies)...>
    > get_unique_helper(std::integer_sequence<U, indicies...>);

    // Force evaluation to be deferred
    template<auto predicate, typename U=bool, std::enable_if_t<std::is_same_v<U, bool>, bool> = true>
    using make_mask_t = ctv_array<U, predicate(values)...>;

    template<auto predicate, typename U=bool, std::enable_if_t<std::is_same_v<U, bool>, bool> = true>
    using filter_t = get_masked_t<make_mask_t<predicate, U>>;

    template<typename U, auto transformer>
    using transform_t = ctv_array<U, transformer(values)...>;

    // Force evaluation to be deferred
    template<typename U=int, std::enable_if_t<std::is_same_v<U, int>, bool> = true>
    using make_unique_t = decltype(get_unique_helper(std::make_integer_sequence<U, size()>{}));

    template<typename U=T, std::enable_if_t<std::is_same_v<U, T>, bool> = true>
    using make_sorted_t = typename cta_qsort_helper<0, ctv_array<U, values...>>::type;

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


}  // namespace ctva



namespace ctta
{

template<typename Array, auto index>
using get_t = typename Array::template get_t<index>;

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

template<typename Array, typename... newValues>
using insert_back_t = typename Array::template insert_back_t<newValues...>;

template<typename Array, typename... newValues>
using insert_front_t = typename Array::template insert_front_t<newValues...>;

template<typename Array, typename newValue>
using push_back_t = typename Array::template push_back_t<newValue>;

template<typename Array, typename newValue>
using push_front_t = typename Array::template push_front_t<newValue>;

template<typename Array, auto index, typename T>
using set_value_t = typename Array::template set_value_t<index, T>;

template<typename A, typename B>
using merge_front_t = typename A::template merge_front_t<B>;

template<typename A, typename B>
using merge_back_t = typename A::template merge_back_t<B>;

template<typename Array>
using make_unique_t = typename Array::template make_unique_t<>;


template<typename T>
struct type_wrapper { using type = T; };

template<typename... Ts>
struct ctt_array
{
    using tuple_t = std::tuple<Ts...>;

    static CONSTEXPRINLINE size_t size()
    {
        return sizeof...(Ts);
    }

    static CONSTEXPRINLINE bool empty()
    {
        return size() == 0;
    }

    template<typename T>
    static CONSTEXPRINLINE int find()
    {
        if constexpr(!empty())
        {
            constexpr bool same[] = { std::is_same_v<T, Ts>... };
            for(int i=0; i<int(size()); ++i)
            {
                if(same[i])
                {
                    return i;
                }
            }
        }

        return -1;
    }

    template<typename T>
    static CONSTEXPRINLINE bool contains()
    {
        return find<T>() != -1;
    }

    template<auto index>
    using get_t = typename std::tuple_element<index, tuple_t>::type;

    template<auto... indicies>
    using get_multiple_t = ctt_array<get_t<indicies>...>;

    template<auto offset, typename U, U... indicies>
    constexpr static get_multiple_t<offset + indicies...> get_helper(std::integer_sequence<U, indicies...>);

    template<auto start, auto end>
    using get_range_t = decltype(get_helper<start>(std::make_integer_sequence<decltype(start), end-start>{}));

    template<typename T>
    using push_back_t = ctt_array<Ts..., T>;

    template<typename T>
    using push_front_t = ctt_array<T, Ts...>;

    template<typename... Us> static constexpr ctt_array<Ts..., Us...> merge_back_helper(ctt_array<Us...>);
    template<typename... Us> static constexpr ctt_array<Us..., Ts...> merge_front_helper(ctt_array<Us...>);

    template<typename... Us>
    using insert_back_t = ctt_array<Ts..., Us...>;

    template<typename... Us>
    using insert_front_t = ctt_array<Us..., Ts...>;

    template<typename Array>
    using merge_back_t = decltype(merge_back_helper(Array{}));

    template<typename Array>
    using merge_front_t = decltype(merge_front_helper(Array{}));

    template<auto index, typename T>
    using set_value_t = typename get_range_t<0, index>
                        ::template push_back_t<T>
                        ::template merge_back_t<get_range_t<index+1, size()>>
                        ;

    template<typename U, U... indicies>
    static CONSTEXPRINLINE ctt_array<get_t<indicies>...> get_indirect_helper(ctva::ctv_array<U, indicies...>);

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
        using IndirectArray = ctva::ctv_array<size_t, tmp.indicies[helper_indicies]...>;
        return decltype(get_indirect_helper(IndirectArray{})){};
    }

    template<typename FlagArray, typename Empty>
    static RAPI_CONSTEXPRINLINE auto get_masked_helper()
    {
        static_assert(size() == FlagArray::size(), "Size mismatch!");
    
        using AsSizeT = typename FlagArray::template convert_to_t<size_t>;
        constexpr size_t count = AsSizeT::sum();
        
        if constexpr(count == 0)
        {
            return Empty{};
        }
        else if constexpr(count == size())
        {
            return ctt_array<Ts...>{};
        }
        else
        {
            return get_masked_helper_inner<FlagArray>(std::make_integer_sequence<size_t, count>{});
        }
    }

    template<typename Array>
    using get_indirect_t = decltype(get_indirect_helper(Array{}));

    // Deferred (clang complains about returning ctt_array<> explicitly)
    template<typename Array, typename Empty=ctt_array<>>
    using get_masked_t = decltype(get_masked_helper<Array, Empty>());

    template<auto predicate>
    using make_mask_t = ctva::ctv_array<bool, predicate(type_wrapper<Ts>{})...>;

    template<auto predicate, typename Empty=ctt_array<>>
    using filter_t = get_masked_t<make_mask_t<predicate>, Empty>;

    template<typename U, U... indicies, typename Empty=ctt_array<>>
    static CONSTEXPRINLINE get_masked_t<ctva::ctv_array<bool, (find<get_t<indicies>>() == indicies)...>, Empty>
                                get_unique_helper(std::integer_sequence<U, indicies...>);

    // Force evaluation to be deferred
    template<typename U=int, std::enable_if_t<std::is_same_v<U, int>, bool> = true>
    using make_unique_t = decltype(get_unique_helper(std::make_integer_sequence<U, size()>{}));
};


} // namespace ctta
