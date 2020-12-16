#pragma once

// This basically makes use of the fact that currently the top 16 bits of a pointer in 64bit arch isn't used.
// https://stackoverflow.com/questions/16198700/using-the-extra-16-bits-in-64-bit-pointers
// https://stackoverflow.com/questions/31522582/can-i-use-some-bits-of-pointer-x86-64-for-custom-data-and-how-if-possible
//
// This can be used more-or-less as a drop in replacement for T*
// However, iteration benefits from a cast to T*, due to this class
// hindering (atleast GCC) from being able to vectorize loops.

#include <cstdint>



template<typename T, uint8_t tagged_bits=16>
struct tagged_ptr {
    const static uintptr_t MASK = (~uintptr_t(0)) >> tagged_bits;
    const static uintptr_t INV_MASK = ~MASK;

    tagged_ptr() = default;
    tagged_ptr(const tagged_ptr&) = default;
    tagged_ptr(T* p) : m_ptr((uintptr_t)p){}


    tagged_ptr& operator++() {
        m_ptr += sizeof(T);
        return *this;
    }
    tagged_ptr& operator--() {
        m_ptr -= sizeof(T);
        return *this;
    }

    operator T*() const { return ptr(); }
    T& operator*() const { return *ptr(); }
    T* operator->() const { return ptr(); }

    tagged_ptr& operator+=(const int x) {
        m_ptr += sizeof(T) * x;
        return *this;
    }
    tagged_ptr& operator-=(const int x) {
        m_ptr -= sizeof(T) * x;
        return *this;
    }

    tagged_ptr operator+(const int x) const {
        return (tagged_ptr(*this) += x);
    }
    tagged_ptr operator-(const int x) const {
        return (tagged_ptr(*this) -= x);
    }

    T* ptr() const {
        return (T*)(m_ptr & MASK);
    }
    T* ptr(T* new_ptr) {
        T* old_ptr = ptr();
        m_ptr += (uintptr_t)new_ptr - (m_ptr & MASK);
        return old_ptr;
    }

    uintptr_t data() const {
        return (m_ptr >> (sizeof(uintptr_t)*8 - tagged_bits));
    }
    uintptr_t data(uintptr_t new_data) {
        uintptr_t old = data();
        m_ptr += (new_data << (sizeof(uintptr_t)*8 - tagged_bits)) - (m_ptr & INV_MASK);
        return old;
    }

private:
    uintptr_t   m_ptr = 0;

};