#include <vector>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <utility>


#if defined(__has_cpp_attribute)
    #if __has_cpp_attribute(likely) && __has_cpp_attribute(unlikely)
        #define IF_LIKELY(expr)   if(expr) [[likely]]
        #define IF_UNLIKELY(expr)   if(expr) [[unlikely]]
    #endif
#endif

#ifndef IF_LIKELY
    #if defined(__GNUC__)
        #define IF_LIKELY(expr)   if((__builtin_expect(!!(expr), 1)))
        #define IF_UNLIKELY(expr)   if((__builtin_expect(!!(expr), 0)))
    #else
        #define IF_LIKELY(expr)   if(expr)
        #define IF_UNLIKELY(expr)   if(expr)
    #endif
#endif



struct perthread_pool_header
{
    uint32_t      prefsize;   // prefered size held by this pool.
    uint32_t      maxsize;    // maximum capacity held by (when idx = maxsize, entries will be released)
    uint32_t      idx;        // current pop index (0 = empty)
};

template<typename T>
const static size_t perthread_pool_data_off = ((sizeof(perthread_pool_header) - 1) | (alignof(T) - 1)) + 1;

template<typename T>
perthread_pool_header* allocPerThreadPool(size_t prefsize, size_t maxsize) noexcept
{
    assert((prefsize != 0) && (maxsize > prefsize));
    const size_t numbytes = perthread_pool_data_off<T> + maxsize * sizeof(T);
    perthread_pool_header* result = (perthread_pool_header*)std::malloc(numbytes);
    result->prefsize = prefsize;
    result->maxsize = maxsize;
    result->idx = 0;
    return result;
}

inline void deallocPerThreadPool(perthread_pool_header* header) noexcept { std::free(header); }


// Only works with C style - POD
template<typename T, typename GlobalPool>
class perthread_pool
{

public:
    // No copying allowed
    perthread_pool(const perthread_pool&) = delete;
    perthread_pool& operator=(const perthread_pool&) = delete;

    perthread_pool() noexcept = default;
    perthread_pool(GlobalPool* globalPool, size_t prefsize, size_t maxsize) noexcept
    : m_pool(allocPerThreadPool<T>(prefsize, maxsize)), m_globalPool(globalPool)
    {}

    ~perthread_pool() noexcept { destroy(); }
    perthread_pool(perthread_pool&& other) noexcept
    : m_pool(other.m_pool), m_globalPool(other.m_globalPool)
    {
        other.m_pool = nullptr;
    }
    perthread_pool& operator=(perthread_pool&& other) noexcept
    {
        destroy();
        std::swap(m_pool, other.m_pool);
        m_globalPool = other.m_globalPool;
    }

    T get()
    {
        T* ptr = (T*)((uint8_t*)m_pool + perthread_pool_data_off<T>);
        // We need to fetch more data
        IF_UNLIKELY(m_pool->idx == 0)
        {
            // alloc from global
            m_globalPool->get(ptr, m_pool->prefsize);
            m_pool->idx = m_pool->prefsize;
        }
        return ptr[--m_pool->idx];
    }

    void release(T value)
    {
        T* ptr = (T*)((uint8_t*)m_pool + perthread_pool_data_off<T>);
        // We need to release some data
        IF_UNLIKELY(m_pool->idx == m_pool->maxsize)
        {
            m_globalPool->release(ptr + m_pool->prefsize, m_pool->maxsize-m_pool->prefsize);
            m_pool->idx = m_pool->prefsize;
        }
        ptr[m_pool->idx++] = value;
    }


private:
    void destroy() noexcept
    {
        if(m_pool)
        {
            // Is there value in a release back to the global pool?
            deallocPerThreadPool(m_pool);
            m_pool = nullptr;
        }
    }

    // Pointer to the pool itself
    perthread_pool_header*  m_pool = nullptr;
    GlobalPool*             m_globalPool = nullptr;
};


struct global_pool_u64
{
    void get(uint64_t* out, size_t size);
    void release(uint64_t* out, size_t size);
};





using perthread_poolT = perthread_pool<uint64_t, global_pool_u64>;


uint64_t g(perthread_poolT* p)
{
    return p->get();
}

void r(perthread_poolT* p, uint64_t v)
{
    p->release(v);
}

perthread_poolT al(global_pool_u64* glb)
{
    return perthread_poolT(glb, 32, 64);
}
