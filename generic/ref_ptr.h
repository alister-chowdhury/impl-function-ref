#pragma once


// This adds logic for a shared_ptr style system of refcounting (with less overhead).
//
// Classes subscribe to having a ref-counter by inheriting from RefPtrTracking
// which allows for a custom destructor class and thread safety (via atomics)
// can be opted into (ThreadSafeRefPtrTracking also exists for convience).
//
// This can then be used with RefPtr<T>.
//
//    Example usage:
//
//    struct A : RefPtrTracking<A>
//    {
//    };
//
//    struct B : A
//    {
//    };
//
// RefPtr<B> bPtr = makeRefPtr<B>();
// RefPtr<A> aPtr = bPtr; // implicit downcast is supported
//
// Using RefPtrTracking will make the class unmoveable however.
// Much in the same way a shared_ptr shouldn't be used on the stack object
// neither should this.
//
// Additionally, unlikely shared_ptr, the following is possible without
// a double destructor being called (although this is kinda sketchy).
//
//    RefPtr<A> ref = ...;
//    A* raw = ref.get();
//    RefPtr<A> newRef = raw;
//
//
// Classes may also opt to do the following:
//
//    struct C : RefPtrTracking<C>
//    {
//    protected:
//        friend class RefPtr<C>;
//        C() {}
//    };
//
// RefPtr<C> cPtr = makeRefPtr<C>();
//
// In order to enforce that class is always instantiated as RefPtr.
//

#include <cstdint>
#include <type_traits>
#include <atomic>
#include <utility>
#include <memory>


#if defined(_MSC_VER)
    #define FORCEINLINE __forceinline
    #define NOINLINE __declspec(noinline)

#elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
    #define FORCEINLINE __attribute__((always_inline)) inline
    #define NOINLINE __attribute__((noinline))

#else
    #define FORCEINLINE inline
    #define NOINLINE
#endif



// Convience class to use with RefPtrTracking to return items to pools etc.
class RefPtrDestroySelfDeleter
{
public:
    template<typename T>
    FORCEINLINE void operator()(T* item)
    {
        item->destroySelf();
    }
};


namespace detail
{

template<bool threadSafe=false>
struct RefPtrCounter
{

protected:
    using counter_type = std::conditional_t<threadSafe, std::atomic<uint64_t>, uint64_t>;
    FORCEINLINE void incReference() { ++counter; }
    FORCEINLINE bool decReference() { return --counter == 0; }
    counter_type counter { 0 };
};


} // namespace detail


template <typename T> class RefPtr;


template<typename T, typename Deleter=std::default_delete<T>, bool threadSafe=false>
class RefPtrTracking : public detail::RefPtrCounter<threadSafe>
{

public:
    // Moving is not allowed of reference objects
    RefPtrTracking() = default;
    RefPtrTracking(const RefPtrTracking&) = delete;
    void operator=(const RefPtrTracking&) = delete;


    template<typename U, typename V>
    static constexpr bool matchingPointerAddress()
    {
        constexpr bool v = &U::counter == &V::counter;
        return v;
    }


protected:
    template<typename U>
    friend class RefPtr;

    template<typename U=T>
    FORCEINLINE RefPtr<U> refPtr() { return RefPtr<T>((T*)this); }

    template<typename U>
    const static bool matchingTrackingPtrAddress = &U::counter == &T::counter;

    FORCEINLINE void incReference()
    {
        detail::RefPtrCounter<threadSafe>::incReference();
    }

    void decReference()
    {
        if(detail::RefPtrCounter<threadSafe>::decReference())
        {
            deleteSelf();
        }
    }

    uint64_t getCounter() const
    {
        return detail::RefPtrCounter<threadSafe>::counter;
    }

    NOINLINE void deleteSelf()
    {
        Deleter()((T*)this);
    }
};


template<typename T, typename Deleter=std::default_delete<T>>
using ThreadSafeRefPtrTracking = RefPtrTracking<T, Deleter, true>;


template<typename T>
class RefPtr
{
    const static bool hasRefTrackingCounter = (
        std::is_base_of<detail::RefPtrCounter<false>, T>::value
        || std::is_base_of<detail::RefPtrCounter<true>, T>::value
    );
    static_assert(
        hasRefTrackingCounter,
        "Type does not inherit from RefPtrTracking, which is required!"
    );

private:
    template<typename U>
    struct can_cast_to
    {
        // Down casting is fine as long as the tracking address matches
        const static bool hasMatchingTrackingAddress = T::template matchingTrackingPtrAddress<U>;
        const static bool value = (
            hasMatchingTrackingAddress
            && std::is_base_of<T, U>::value
        );
    };

public:

    template<typename... Args>
    FORCEINLINE static RefPtr<T> makeRefPtr(Args&&... args)
    {
        return new T(std::forward<Args>(args)...);
    }

    ~RefPtr()
    {
        if(ptr)
        {
            ptr->decReference();
        }
    }

    RefPtr() {}

    RefPtr(const RefPtr& inPtr)
    : ptr(inPtr.ptr)
    {
        if(ptr)
        {
            ptr->incReference();
        }
    }

    RefPtr(RefPtr&& inPtr)
    : ptr(inPtr.ptr)
    {
        inPtr.ptr = nullptr;
    }

    template<typename U, std::enable_if_t<can_cast_to<U>::value, bool> = true>
    RefPtr(U* inPtr)
    : ptr(inPtr)
    {
        if(ptr)
        {
            ptr->incReference();
        }
    }

    template<typename U, std::enable_if_t<can_cast_to<U>::value, bool> = true>
    RefPtr(const RefPtr<U>& inPtr)
    : ptr(inPtr.ptr)
    {
        if(ptr)
        {
            ptr->incReference();
        }
    }

    template<typename U, std::enable_if_t<can_cast_to<U>::value, bool> = true>
    explicit RefPtr(RefPtr<U>&& inPtr)
    : ptr(inPtr.ptr)
    {
        inPtr.ptr = nullptr;
    }

    template<typename U, std::enable_if_t<can_cast_to<U>::value, bool> = true>
    RefPtr& operator= (U* newPtr)
    {
        T* oldPtr = ptr;
        if(newPtr)
        {
            newPtr->incReference();
        }
        ptr = newPtr;
        if(oldPtr)
        {
            oldPtr->decReference();
        }
        return *this;
    }


    RefPtr& operator= (const RefPtr& inPtr)
    {
        T* oldPtr = ptr;
        T* newPtr = inPtr.ptr;
        if(newPtr)
        {
            newPtr->incReference();
        }
        ptr = newPtr;
        if(oldPtr)
        {
            oldPtr->decReference();
        }
        return *this;
    }

    FORCEINLINE operator bool() const { return (bool)ptr; }

    const T* operator->() const { return ptr; }
    const T& operator*() const { return *ptr; }
    T& operator*() { return *ptr; }
    T* operator->() { return ptr; }

    FORCEINLINE const T* get() const { return ptr; }
    FORCEINLINE T* get() { return ptr; }

    FORCEINLINE uint64_t count() const
    {
        if(ptr)
        {
            return ptr->getCounter();
        }
        return 0;
    }

    void release()
    {
        if(ptr)
        {
            ptr->decReference();
            ptr = nullptr;
        }
    }

    FORCEINLINE bool operator == (const RefPtr<T>& other) const { return ptr == other.ptr; }
    FORCEINLINE bool operator != (const RefPtr<T>& other) const { return ptr != other.ptr; }
    FORCEINLINE bool operator <= (const RefPtr<T>& other) const { return ptr <= other.ptr; }
    FORCEINLINE bool operator >= (const RefPtr<T>& other) const { return ptr >= other.ptr; }
    FORCEINLINE bool operator < (const RefPtr<T>& other) const { return ptr < other.ptr; }
    FORCEINLINE bool operator > (const RefPtr<T>& other) const { return ptr > other.ptr; }
    friend FORCEINLINE auto hash(const RefPtr<T>& value) { return std::hash<T*>{}(value.ptr); }

private:
    template<typename U>
    friend class RefPtr;

    T*  ptr = nullptr;
};


template<typename T, typename... Args>
FORCEINLINE RefPtr<T> makeRefPtr(Args&&... args)
{
    return std::move(RefPtr<T>::makeRefPtr(std::forward<Args>(args)...));
}
