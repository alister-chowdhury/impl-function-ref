#pragma once

// ProxyIterator
// -------------
// Intercept the iterators fetching logic with a custom function.
// use `createProxyIterator` for convience.
//  e.g:
//
//    struct F0
//    {
//        int a;
//        int b;
//    };
//
//    void insertSummed(F0* begin, F0* end, std::unordered_map<int, int>& output)
//    {
//        // NB: Make:
//        // [](It) -> RetType&
//        // If you want to iterate something that needs to be written back
//        auto func = [](F0* it)
//        {
//            return std::make_pair(it->a, it->a + it->b);
//        };
//
//        output.insert(
//            createProxyIterator(&func, begin),
//            createProxyIterator(&func, end)
//        );
//    }
//
//
// AttributeIterator
// -----------------
// Use pluck an attribute from the underlying class.
// use `createAttrIterator` for convience.
//  e.g:
//
//    struct F0
//    {
//        int a;
//        int b;
//    };
//
//    void insertB(F0* begin, F0* end, std::unordered_set<int>& output)
//    {
//        output.insert(
//            createAttrIterator<&F0::b>(begin),
//            createAttrIterator<&F0::b>(end)
//        );
//    }
//

#include <cstdint>
#include <iterator>
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


template<typename Function, typename BaseIterator>
struct ProxyIterator
{
    CONSTEXPRINLINE ProxyIterator(const Function* func, BaseIterator iter) : func(func), iter(iter) {}
    
    using RetType = decltype(std::declval<const Function>()(std::declval<const BaseIterator&>()));

    CONSTEXPRINLINE static RetType fetch(const Function* func, const BaseIterator& iter) { return (*func)(iter); }
    CONSTEXPRINLINE RetType fetch() const { return fetch(func, iter); }

    using iterator_category = typename std::iterator_traits<BaseIterator>::iterator_category;
    using difference_type   = std::ptrdiff_t;
    using reference         = decltype(fetch(std::declval<const Function*>(), std::declval<BaseIterator>()));
    using value_type        = std::remove_reference_t<reference>;
    using pointer           = value_type*;

    CONSTEXPRINLINE reference operator*() const { return fetch(); }
    CONSTEXPRINLINE pointer operator->() { return &fetch(); }
    CONSTEXPRINLINE ProxyIterator& operator++() { iter++; return *this; }  
    CONSTEXPRINLINE ProxyIterator operator++(int) { ProxyIterator tmp = *this; ++(*this); return tmp; }
    CONSTEXPRINLINE ProxyIterator& operator--() { iter--; return *this; }  
    CONSTEXPRINLINE ProxyIterator operator--(int) { ProxyIterator tmp = *this; -(*this); return tmp; }

    auto operator[](auto idx) const { return iter[idx]; }

    CONSTEXPRINLINE ProxyIterator& operator+=(difference_type amt) { iter+=amt; return *this; }
    CONSTEXPRINLINE ProxyIterator& operator-=(difference_type amt) { iter-=amt; return *this; }
    CONSTEXPRINLINE ProxyIterator operator+(difference_type amt) const { ProxyIterator tmp = *this; tmp += amt; }
    CONSTEXPRINLINE ProxyIterator operator-(difference_type amt) const { ProxyIterator tmp = *this; tmp -= amt; }

    friend CONSTEXPRINLINE bool operator== (const ProxyIterator& a, const ProxyIterator& b) { return a.iter == b.iter; };
    friend CONSTEXPRINLINE bool operator!= (const ProxyIterator& a, const ProxyIterator& b) { return a.iter != b.iter; };
    friend CONSTEXPRINLINE auto operator<=> (const ProxyIterator& a, const ProxyIterator& b) { return a.iter <=> b.iter; };
    friend CONSTEXPRINLINE difference_type operator- (const ProxyIterator& a, const ProxyIterator& b) { return a.iter - b.iter; };

    BaseIterator    iter;
    const Function* func;
};


template<auto AttributeAddress, typename BaseIterator>
struct AttributeIterator
{
    CONSTEXPRINLINE AttributeIterator(BaseIterator iter) : iter(iter) {}
    
    CONSTEXPRINLINE static auto& fetch(const BaseIterator& iter) { return iter->*AttributeAddress; }
    CONSTEXPRINLINE auto& fetch() const { return fetch(iter); }

    using iterator_category = typename std::iterator_traits<BaseIterator>::iterator_category;
    using difference_type   = std::ptrdiff_t;
    using reference         = decltype(fetch(std::declval<BaseIterator>()));
    using value_type        = std::remove_reference_t<reference>;
    using pointer           = value_type*;

    CONSTEXPRINLINE reference operator*() const { return fetch(); }
    CONSTEXPRINLINE pointer operator->() { return &fetch(); }
    CONSTEXPRINLINE AttributeIterator& operator++() { iter++; return *this; }  
    CONSTEXPRINLINE AttributeIterator operator++(int) { AttributeIterator tmp = *this; ++(*this); return tmp; }
    CONSTEXPRINLINE AttributeIterator& operator--() { iter--; return *this; }  
    CONSTEXPRINLINE AttributeIterator operator--(int) { AttributeIterator tmp = *this; -(*this); return tmp; }

    auto operator[](auto idx) const { return iter[idx]; }

    CONSTEXPRINLINE AttributeIterator& operator+=(difference_type amt) { iter+=amt; return *this; }
    CONSTEXPRINLINE AttributeIterator& operator-=(difference_type amt) { iter-=amt; return *this; }
    CONSTEXPRINLINE AttributeIterator operator+(difference_type amt) const { return AttributeIterator(iter) += amt; }
    CONSTEXPRINLINE AttributeIterator operator-(difference_type amt) const { return AttributeIterator(iter) -= amt; }

    friend CONSTEXPRINLINE bool operator== (const AttributeIterator& a, const AttributeIterator& b) { return a.iter == b.iter; };
    friend CONSTEXPRINLINE bool operator!= (const AttributeIterator& a, const AttributeIterator& b) { return a.iter != b.iter; };
    friend CONSTEXPRINLINE auto operator<=> (const AttributeIterator& a, const AttributeIterator& b) { return a.iter <=> b.iter; };
    friend CONSTEXPRINLINE difference_type operator- (const AttributeIterator& a, const AttributeIterator& b) { return a.iter - b.iter; };

    BaseIterator    iter;
};


template<typename Function, typename BaseIterator>
CONSTEXPRINLINE ProxyIterator<Function, BaseIterator> createProxyIterator(const Function* func, BaseIterator iter)
{
    return {func, iter};
}


template<auto AttributeAddress, typename BaseIterator>
CONSTEXPRINLINE AttributeIterator<AttributeAddress, BaseIterator> createAttrIterator(BaseIterator iter)
{
    return {iter};
}
