#pragma once


#include <memory>
#include <new>
#include <utility>



/**
 * @brief A wrapper type to construct uninitialized instances of an object.
 *        Primarily aimed at deferring the construction of objects with many strings
 *        or maps, when it's unclear if the object is actually going to be used.
 *
 * @description
 * Example usage:
 *
 *  void copy_and_fix_if_invalid(const MyObject& original, maybe_uninit<MyObject>& out)
 *  {
 *      if(original.is_invalid())
 *      {
 *          out.init(original)->fix();
 *          out->do_something_else();
 *      }
 *  }
 *
 */
template <typename T>
struct maybe_uninit
{
    constexpr maybe_uninit() = default;

    constexpr maybe_uninit(const maybe_uninit& other) {
        if(other.m_initialized) {
            init(*other);
        }
    }

    constexpr maybe_uninit(maybe_uninit&& other) {
        if(other.m_initialized) {
            init(std::move(*other));
        }
    }

    constexpr maybe_uninit& operator=(const maybe_uninit& other) {
        if(other.m_initialized) {
            if(m_initialized) {
                *get() = *other;
            }
            else {
                init(*other);
            }
        }
        else {
            uninit();
        }
        return *this;
    }

    ~maybe_uninit() {
        if (m_initialized) {
            get()->~T();
            m_initialized = false;
        }
    }

    /**
     * @brief Initialize the underlying type.
     * @details Repeated calls replaces the underlying object.
     */
    template <typename... ArgTs>
    constexpr T* init(ArgTs&&... args) {
        if (!m_initialized) {
            m_initialized = true;
        }
        else {
            get()->~T();
        }

        new (m_buffer) T(std::forward<ArgTs>(args)...);

        return get();
    }

    /**
     * @brief Uninitialize the underlying type.
     */
    constexpr void uninit() {
        if(m_initialized) {
            get()->~T();
            m_initialized = false;
        }
    }

    /**
     * @brief Test if the underlying object has been initialized.
     */
    constexpr bool initialized() const {
        return m_initialized;
    }

    /**
     * @brief Get a pointer to an instance of the underlying type.
     * @details If uninitialized a nullptr is returned.
     */
    constexpr T* get() {
        if (!m_initialized) {
            return nullptr;
        }
        return (T*)&m_buffer;
    }

    constexpr const T* get() const {
        if (!m_initialized) {
            return nullptr;
        }
        return (const T*)&m_buffer;
    }

    constexpr T& operator*() {
        return *get();
    }

    constexpr const T& operator*() const {
        return *get();
    }

    constexpr T* operator->() {
        return get();
    }

    constexpr T* operator->() const {
        return get();
    }

  private:
    alignas(alignof(T)) char m_buffer[sizeof(T)];
    char m_initialized = false;
};

