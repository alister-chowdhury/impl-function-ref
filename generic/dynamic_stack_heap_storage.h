// THIS WAS WIP
// IF I COMMITED THIS, THAT WAS AN ACCIDENT

#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <new>
#include <utility>


constexpr size_t min1(size_t value) { return value == 0 ? 1 : value; }

// Dynamic stack-heap storage
template<
    typename T,
    // By default max local storage such that
    // the footprint is < 64bytes, with atleast 1 entry
    // being stored locally.
    size_t max_local_storage=min1(size_t(64 - int(sizeof(void*)*3))/int(sizeof(T)))>
struct dsh_storage
{

    struct T_bytes
    {
        operator T*() { return (T*)&data[0]; }
        operator const T*() const { return (const T*)&data[0]; }

        template<typename... Args>
        T* init(Args&&... args)
        {
            new (data) T(std::forward<Args>(args)...);
            return (T*)this;
        }
        void destroy() { ((T*)this)-> ~T(); }

        alignas(alignof(T)) uint8_t data[sizeof(T)];
    };

    T* begin() { return raw_begin; }
    const T* begin() const { return raw_begin; }

    T* end() { return raw_end; }
    const T* end() const { return raw_end; }

    T* capacity() { return raw_capacity; }
    const T* capacity() const { return raw_capacity; }

    size_t size() const { return raw_end - raw_begin; }
    size_t capacity_size() const { return raw_capacity - raw_begin; }


    void clear()
    {
        for(T_bytes* iter=raw_begin; iter<raw_end; ++iter)
        {
            iter->destroy();
        }
        if(raw_begin != &local_storage[0])
        {
            std::free(raw_begin);
            raw_begin = &local_storage[0];
        }
        raw_end = &local_storage[0];
        raw_capacity = &local_storage[max_local_storage];
    }

    // All the insert_space* functions MUST have init calls done straight after
    // and before any other operation is done to the container, otherwise
    // strange problems might occur.
    __attribute__((always_inline)) T_bytes* insert_space_inline(const size_t i, const size_t n)
    {
        // Simply move stuff
        if(raw_end + n < raw_capacity)
        {

            if(std::is_trivial<T>::value)
            {
                // Copy tail
                std::memmove(raw_begin+i+n, raw_begin+i, sizeof(T_bytes)*(size()-i));
            }
            else
            {
                // Copy tail
                const size_t old_size = size();
                for(size_t j=old_size-1; j>=i; --j)
                {
                    raw_begin[j+n].init(std::move(*((T*)raw_begin[j])));
                    raw_begin[j].destroy();
                }
            }
            raw_end += n;
            return raw_begin + i;
        }

        // Need a realloc
        else
        {
             const size_t old_size = size();

            size_t new_capacity_size = std::max(
                old_size + n + 1,
                capacity_size() * 2
            );

            T_bytes* old_begin = raw_begin;            
            T_bytes* new_begin = (T_bytes*)std::malloc(sizeof(T_bytes) * new_capacity_size);

            if(std::is_trivial<T>::value)
            {
                // Copy head
                std::memcpy(new_begin, old_begin, sizeof(T_bytes) * i);
                // Copy tail
                std::memcpy(new_begin+i+n, old_begin+i, sizeof(T_bytes) * (old_size-i));
            }
            else
            {
                // Copy head
                for(size_t j=0; j<i; ++j)
                {
                    new_begin[j].init(std::move(*((T*)old_begin[j])));
                    old_begin[j].destroy();
                }
                // Copy tail
                for(size_t j=i; j<old_size; ++j)
                {
                    new_begin[j+n].init(std::move(*((T*)old_begin[j])));
                    old_begin[j].destroy();
                }
            }

            if(raw_begin != &local_storage[0])
            {
                std::free(old_begin);
            }

            raw_begin = new_begin;
            raw_end = new_begin + old_size + n;
            raw_capacity = new_begin + new_capacity_size;
            return new_begin + i;
        }
    }

    T_bytes* insert_space(const size_t i, const size_t n)
    {
        return insert_space_inline(i, n);
    }

    T_bytes* insert_space_back(const size_t n)
    {
        return insert_space_inline(size(), n);
    }

    T_bytes* insert_space_front(const size_t n)
    {
        return insert_space_inline(0, n);
    }

    template<typename... Args>
    T* emplace_back(Args&&... args)
    {
        return insert_space_back(1)->init(std::forward<Args>(args)...);
    }

    template<typename... Args>
    T* emplace_front(Args&&... args)
    {
        return insert_space_front(1)->init(std::forward<Args>(args)...);
    }

    T* push_back(T value)
    {
        return insert_space_back(1)->init(std::move(value));
    }

    void push_back(const T* b, const T* e)
    {
        const size_t new_entries = e - b;
        if(!new_entries) { return; }
        T_bytes * new_entries_begin = insert_space_back(new_entries);
        
        if(std::is_trivial<T>::value)
        {
            std::memcpy(new_entries_begin, b, new_entries * sizeof(T_bytes));
        }
        else
        {
            for(; b<e; ++b, ++new_entries_begin)
            {
                new_entries_begin->init(*b);
            }
        }
    }

    T* push_front(T value)
    {
        return insert_space_front(1)->init(std::move(value));
    }

    void push_front(const T* b, const T* e)
    {
        const size_t new_entries = e - b;
        if(!new_entries) { return; }
        T_bytes * new_entries_begin = insert_space_front(new_entries);

        if(std::is_trivial<T>::value)
        {
            std::memcpy(new_entries_begin, b, new_entries * sizeof(T_bytes));
        }
        else
        {
            for(; b<e; ++b, ++new_entries_begin)
            {
                new_entries_begin->init(*b);
            }
        }
    }

    T_bytes*    raw_begin = &local_storage[0];
    T_bytes*    raw_end = &local_storage[0];
    T_bytes*    raw_capacity = &local_storage[max_local_storage];
    T_bytes     local_storage[max_local_storage];

};


#include <string>

using TT = int;
using O = dsh_storage<TT>;


void x0(O& o, TT* b, TT* e)
{
    o.push_back(b, e);
}
