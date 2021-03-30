#include <type_traits>
#include <utility>


namespace detail {


#if __cplusplus >= 201703L

using std::void_t;

#else

template<typename... Ts> struct make_void { typedef void type;};
template<typename... Ts> using void_t = typename make_void<Ts...>::type;

#endif


#if defined(__GNUC__)
    #define CONSTEXPR_ALWAYS_INLINE     __attribute__((always_inline)) constexpr inline
#elif defined(_MSC_VER)
    #define CONSTEXPR_ALWAYS_INLINE     constexpr __forceinline
#else
    #define CONSTEXPR_ALWAYS_INLINE     constexpr inline
#endif


template <typename T>
struct is_simple_type {
    constexpr static bool value = (
        std::is_pointer<T>::value
        || std::is_floating_point<T>::value
        || std::is_integral<T>::value
        || std::is_scalar<T>::value
    );
};


template <typename T>
struct cmp_eq_one_impl {

private:
    // For simple types compare them in the 'light' block
    template<typename U=T, std::enable_if_t<is_simple_type<U>::value, bool> = true>
    CONSTEXPR_ALWAYS_INLINE static bool cmp_eq_light_impl(const T& a, const T& b) {
        return a == b;
    }
    template<typename U=T, std::enable_if_t<!is_simple_type<U>::value, bool> = true>
    CONSTEXPR_ALWAYS_INLINE static bool cmp_eq_light_impl(const T& a, const T& b) {
        return true;
    }

    // For everything else compare them in the 'normal' block
    template<typename U=T, std::enable_if_t<is_simple_type<U>::value, bool> = true>
    CONSTEXPR_ALWAYS_INLINE static bool cmp_eq_normal_impl(const T& a, const T& b) {
        return true;
    }
    template<typename U=T, std::enable_if_t<!is_simple_type<U>::value, bool> = true>
    CONSTEXPR_ALWAYS_INLINE static bool cmp_eq_normal_impl(const T& a, const T& b) {
        return a == b;
    }

    // Helper for a.size() == b.size() compares
    template <typename U=T, typename=void_t<>>
    struct cmp_eq_size_impl {
        CONSTEXPR_ALWAYS_INLINE static bool call(const U&, const U&) {
            return true;
        }
    };

    template <typename U>
    struct cmp_eq_size_impl<U, void_t<decltype( std::declval<const U&>().size() )> > {
        CONSTEXPR_ALWAYS_INLINE static bool call(const U& a, const U& b) {
            return a.size() == b.size();
        }
    };

    CONSTEXPR_ALWAYS_INLINE static bool cmp_eq_size(const T& a, const T& b) {
        return cmp_eq_size_impl<T>::call(a, b);
    }


public:
    CONSTEXPR_ALWAYS_INLINE static bool cmp_eq_light(const T& a, const T& b) {
        return (
            cmp_eq_light_impl<>(a, b)
            && cmp_eq_size(a, b)
        );
    }
    CONSTEXPR_ALWAYS_INLINE static bool cmp_eq_normal(const T& a, const T& b) {
        return (
            cmp_eq_normal_impl<>(a, b)
        );
    }

};


template <typename... Ts>
struct cmp_eq_impl {

private:
    // all_equal helper

    template <bool light_block, typename...>
    struct all_equal_impl {
        CONSTEXPR_ALWAYS_INLINE static bool call() { return true; }
    };

    // Light all_equal compare
    template <typename T, typename... Nexts>
    struct all_equal_impl<true, T, Nexts...> {
        CONSTEXPR_ALWAYS_INLINE static bool call(
                const T& a, const Nexts&... nexts_a,
                const T& b, const Nexts&... nexts_b
        ) {
            return (
                cmp_eq_one_impl<T>::cmp_eq_light(a, b)
                && all_equal_impl<true, Nexts...>::call(nexts_a..., nexts_b...)
            );
        }
    };

    // Normal all_equal compare
    template <typename T, typename... Nexts>
    struct all_equal_impl<false, T, Nexts...> {
        CONSTEXPR_ALWAYS_INLINE static bool call(
                const T& a, const Nexts&... nexts_a,
                const T& b, const Nexts&... nexts_b
        ) {
            return (
                cmp_eq_one_impl<T>::cmp_eq_normal(a, b)
                && all_equal_impl<false, Nexts...>::call(nexts_a..., nexts_b...)
            );
        }
    };

    template<bool light_block>
    CONSTEXPR_ALWAYS_INLINE static bool all_equal(const Ts&... a_values, const Ts&... b_values) {
        return all_equal_impl<light_block, Ts...>::call(a_values..., b_values...);
    }

public:
    CONSTEXPR_ALWAYS_INLINE static bool call(const Ts&... a_values, const Ts&... b_values) {
        return (
            all_equal<true>(a_values..., b_values...)
            && all_equal<false>(a_values..., b_values...)
        );
    }
};


} // namespace detail


/**
 * Helper to compare members of two classes are the same.
 * This function will first try to do a 'light' block of compares, (calling .size(), testing simple types first etc)
 * If those tests pass it will move on to doing a series of == compares.
 *
 * e.g:
 *
 *  struct TestObject {
 *      float A;
 *      int B;
 *      std::string C;
 *      std::string D;
 *      std::unordered_map<std::string, std::string> E;
 *      std::unordered_set<std::string> F;
 *      std::string G;
 *      
 *      bool operator== (const TestObject& other) const {
 *          return test_members_are_equal(
 *              *this,
 *              other,
 *              &TestObject::A,
 *              &TestObject::B,
 *              &TestObject::C,
 *              &TestObject::D,
 *              &TestObject::E,
 *              &TestObject::F,
 *              &TestObject::G
 *          );
 *       }
 *  };
 * 
 */
template <typename ClassT, typename... ValueTs>
CONSTEXPR_ALWAYS_INLINE bool test_members_are_equal(
    const ClassT& first,
    const ClassT& second,
    ValueTs ClassT::*... members
) {
    return(
        (&first == &second) // be obvious and test the pointers first
        || detail::cmp_eq_impl<ValueTs...>::call(
            first.* members...,
            second.* members...
        )
    );
}
