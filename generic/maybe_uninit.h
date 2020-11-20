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
 *          out.get()->do_something_else();
 *      }
 *  }
 *
 */
template <typename T>
struct maybe_uninit
{
    maybe_uninit() = default;
    ~maybe_uninit() {
        if (m_initialized) {
            get()->~T();
            m_initialized = false;
        }
    }

    // For now, not supporting move / copy semantics
    maybe_uninit(const maybe_uninit& other) = delete;
    maybe_uninit(maybe_uninit&& other) = delete;
    maybe_uninit& operator=(const maybe_uninit& other) = delete;

    /**
     * @brief Initialize the underlying type.
     * @details Repeated calls replaces the underlying object.
     */
    template <typename... ArgTs>
    T* init(ArgTs&&... args) {
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
     * @brief Get a pointer to an instance of the underlying type.
     * @details If uninitialized a nullptr is returned.
     */
    T* get() {
        if (!m_initialized) {
            return nullptr;
        }
        return (T*)&m_buffer;
    }

    const T* get() const {
        if (!m_initialized) {
            return nullptr;
        }
        return (const T*)&m_buffer;
    }

  private:
    alignas(alignof(T)) char m_buffer[sizeof(T)];
    char m_initialized = false;
};
