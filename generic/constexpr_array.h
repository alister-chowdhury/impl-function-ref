#pragma once

#include <cstddef>
#include <initializer_list>


// C++20 template friendly array with a psuedo-dynamic length
// everything is sprinkled with consteval, because this should
// never be used at runtime.
// Example usage:
//
//    constexpr constexpr_array<int> thing = {1, 2, 3};
//
//    template<constexpr_array<int> t>
//    size_t getSize()
//    {
//        return t.size();    
//    }
//
//    size_t i = getSize<{1, 2, 3}>(); // will return 3


template<
    typename T,
    // Clang and GCC are totally fine with this value being 0xffffffff,
    // MSVC complains if the bytesize is > 0x7fffffff, and the compiler
    // itself will run out of heap memory trying to instantiate anything
    // too-too large, so maybe tone this down if you plan on having arrays
    // of arrays etc
    size_t maxSize=0xffff
>
struct constexpr_array
{

    consteval constexpr_array() = default;

    template<typename Container>
    consteval constexpr_array(const Container& blob)
    : size_(blob.size())
    {
        init_(blob.begin(), blob.end());
    }

    consteval constexpr_array(const std::initializer_list<T>& blob)
    : size_(blob.size())
    {
        init_(blob.begin(), blob.end());
    }

    consteval size_t    size()  const { return size_; }
    consteval const T*  data()  const { return &data_[0]; }
    consteval const T&  operator[](const size_t n) const { return data()[n]; }
    consteval const T*  begin()  const { return data(); }
    consteval const T*  end()   const { return data() + size(); }

    template<typename Container>
    consteval constexpr_array& operator= (const Container& blob)
    {
        size_ = blob.size();
        init_(blob.begin(), blob.end());
        return *this;
    }

    consteval constexpr_array& operator= (const std::initializer_list<T>& blob)
    {
        size_ = blob.size();
        init_(blob.begin(), blob.end());
        return *this;
    }

    consteval void init_(auto begin_, auto end_)
    {
        size_t i=0;
        for(auto iter=begin_; iter<end_; ++iter)
        {
            data_[i++] = *iter;
        }
    }

    size_t size_ = 0;
    T      data_ [maxSize] {};

};
