#include <vector>
#include <mutex>
#include <set>
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <functional>
#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <utility>
#include <unordered_set>
#include <memory>


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

inline unsigned int bsf(unsigned int m)
{
#if defined(__GNUC__) || defined(__CLANG__)
    return __builtin_ctz(m);
#elif defined(_MSC_VER)
    unsigned long v = 0;
    _BitScanForward(&v, (unsigned long)m);
    return v;
#else
    unsigned int value = 0;
    while (m >>= 1) ++value;
    return value;
#endif
}

inline unsigned long bsf(unsigned long long m)
{
#if defined(__GNUC__) || defined(__CLANG__)
    return (unsigned long)__builtin_ctzl(m);
#elif defined(_MSC_VER)
    unsigned long v = 0;
    _BitScanForward64(&v, m);
    return v;
#else
    unsigned long long value = 0;
    while (m >>= 1) ++value;
    return (unsigned long)value;
#endif
}



// Will align to 32bytes on Windows / Linux
// this is very important a children are stored in pairs (so 64bytes), and
// we can make use of GCCs tcache when allocating, which targets sizes <= 64
struct memchunk
{
    size_t      offset;
    memchunk*   parent;
    memchunk*   children;       // children are always stored as pairs
    uint8_t     inuse:1;
    uint8_t     index:7;        // size = (64 << index)
};


struct memchunk_offset_lessthan
{
    bool operator()(const memchunk* a, const memchunk* b) const noexcept
    {
        return a->offset < b->offset;
    }
};

// Always try to keep memchunks ordered by offset for the purpose of
// reducing fragmentation, as .begin() will be ordered.
using MemchunkOffsetSortedSet = std::set<memchunk*, memchunk_offset_lessthan>;


struct MemoryPool
{
    // 1GB    = 24
    // 64byte = 0
    std::array<MemchunkOffsetSortedSet, 25> chunksBySize;
    std::array<std::mutex, 25>              locksBySize;
    std::function<memchunk*(void)>          allocate1GB;
};


memchunk* splitchunk(memchunk* chnk)
{
    memchunk* children = (memchunk*)std::malloc(sizeof(memchunk)*2);
    memchunk local = *chnk;
    children[0].offset = local.offset;
    children[0].parent = chnk;
    children[0].children = nullptr;
    children[0].inuse = false;
    children[0].index = local.index - 1;
    children[1].offset = local.offset + (64 << (local.index - 1));
    children[1].parent = chnk;
    children[1].children = nullptr;
    children[1].inuse = false;
    children[1].index = local.index - 1;
    chnk->inuse = true;
    chnk->children = children;
    return children;
}


void freechunk(memchunk* chnk, MemoryPool& pool)
{
    // You'll notice, we deferr coalescing, to prevent the possibility
    // of tearing down and rebuilding great chunks of the tree for
    // alloc / free pairs
    const size_t index = chnk->index;
    chnk->inuse = false;
    auto& chunkSet = pool.chunksBySize[index];
    std::lock_guard<std::mutex> g(pool.locksBySize[index]);
    chunkSet.insert(chnk);
}


void freechunks_sameindex(memchunk** chnks, const size_t n, MemoryPool& pool)
{
    if(n)
    {
        uint32_t  index = chnks[0]->index;
        auto& chunkSet = pool.chunksBySize[index];
        std::lock_guard<std::mutex> g(pool.locksBySize[index]);
        for(size_t i=0; i<n; ++i)
        {
            chnks[i]->inuse = false;
        }
        chunkSet.insert(chnks, chnks+n);
    }
}


void freechunks_diffindex(memchunk** chnks, const size_t n, MemoryPool& pool)
{
    uint32_t    lockedChunks = 0;
    for(size_t i=0; i<n; ++i)
    {
        memchunk* chnk = chnks[i];
        uint32_t  index = chnk->index;
        uint32_t  lockMask = 1 << index;
        if(!(lockedChunks & lockMask))
        {
            lockedChunks |= lockMask;
            pool.locksBySize[index].lock();
        }
        chnk->inuse = false;
        pool.chunksBySize[index].insert(chnk);
    }
    while(lockedChunks)
    {
        uint32_t bit = lockedChunks & (~lockedChunks + 1);
        pool.locksBySize[bsf(bit)].unlock();
        lockedChunks ^= bit;
    }
}


void freechunks(memchunk** chnks, const size_t n, MemoryPool& pool)
{
    if(n)
    {
        uint8_t idx = chnks[0]->index;
        bool sameIndices = true;
        // Do a cursory check to see if the indices are the same
        for(size_t i=0; i<n; ++i)
        {
            if(idx ^ chnks[i]->index)
            {
                sameIndices = false;
                break;
            }
        }
        if(sameIndices)
        {
            freechunks_sameindex(chnks, n, pool);
        }
        else
        {
            freechunks_diffindex(chnks, n, pool);
        }
    }
}


// No saftey check etc
// TODO, if we do a slight amount of bookkeeping, this could be a loop
// and not recursive
memchunk*  fallocchunkidx(size_t index, MemoryPool& pool)
{
    // Big alloc
    IF_UNLIKELY(index == 24)
    {
        return pool.allocate1GB();
    }

    auto& targetPool = pool.chunksBySize[index];
    if(!targetPool.empty())
    {
        std::lock_guard<std::mutex> g(pool.locksBySize[index]);
        IF_LIKELY(!targetPool.empty())
        {
            auto it = targetPool.begin();
            memchunk* chnk = *it;
            targetPool.erase(it);
            chnk->inuse = true;
            return chnk;
        }
    }

    // Do a parent split, return the first, add the other to the global pool
    memchunk* parent = fallocchunkidx(index+1, pool);
    splitchunk(parent);
    memchunk* children = parent->children;
    {
        std::lock_guard<std::mutex> g(pool.locksBySize[index]);
        targetPool.insert(children + 1);
    }

    children->inuse = true;
    return children;
}


void fallocchunksidx(memchunk** out, size_t n, size_t index, MemoryPool& pool)
{
    if(!n) { return; }

    // Fetch what we can from the global pool
    auto& targetPool = pool.chunksBySize[index];
    if(!targetPool.empty())
    {
        std::unique_lock<std::mutex> g(pool.locksBySize[index]);
        size_t localCopyCount = std::min(n, targetPool.size());
        IF_LIKELY(localCopyCount)
        {
            auto begin = targetPool.begin();
            auto it = begin;
            for(size_t i=0; i<localCopyCount; ++i, ++it)
            {
                out[i] = *it;
                out[i]->inuse = true;
            }
            targetPool.erase(begin, it);
        }
        g.unlock();
        out += localCopyCount;
        n -= localCopyCount;
    }

    // For anything left, fetch from the parent and split
    if(n)
    {
        // Deal with big allocs
        IF_UNLIKELY(index == 24)
        {
            while(n--)
            {
                *out++ = pool.allocate1GB();
            }
            return;
        }

        // Write to the back of the user provided pointer
        const size_t parentCount = n / 2 + (n & 1);
        memchunk** parentIterStart = out + n - parentCount;
        fallocchunksidx(parentIterStart, parentCount, index + 1, pool);

        // Don't iterate the last entry if the output count isn't even
        // a seperate brach below handles that
        memchunk** parentIterEnd = parentIterStart + parentCount - (n & 1);
        for(; parentIterStart < parentIterEnd ; ++parentIterStart, out+=2)
        {
            memchunk* parent = *parentIterStart;
            splitchunk(parent);
            memchunk* children = parent->children;
            out[0] = children;
            out[1] = children + 1;
            children[0].inuse = true;
            children[1].inuse = true;
        }

        // Split the final parent, moving the front to the back and the
        // second half to the global pool
        if(n & 1)
        {
            memchunk* parent = *parentIterStart;
            splitchunk(parent);
            memchunk* children = parent->children;
            out[0] = children;
            children->inuse = true;
            std::lock_guard<std::mutex> g(pool.locksBySize[index]);
            targetPool.insert(children + 1);
        }
    }

}



#pragma once

#include <cstdint>

#if defined(_MSC_VER) && defined(_M_X64)
    #include <intrin.h>

#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    #include <x86intrin.h>

#else
    #error "Only really intended for x64 gcc/clang/msc"
#endif




// uint64_t cycles = measure_cycles([](){ some_function(); });
// NB: You'll need to adjust for any overheads involved (typically) calling
//     an empty function a few times, then subtracting the average etc.
template <typename CallbackT>
inline uint64_t measure_cycles(CallbackT callback) {

#ifdef _MSC_VER
    int64_t first = __rdtsc();
    callback();
    return __rdtsc() - first;
#else
    int64_t first = _rdtsc();
    callback();
    return _rdtsc() - first;
#endif

}



int main(void)
{
    MemoryPool pool;
    pool.allocate1GB = []{
        memchunk* chnk = (memchunk*)std::malloc(sizeof(memchunk));
        chnk->offset = 0;
        chnk->parent = nullptr;
        chnk->children = nullptr;
        chnk->inuse = true;
        chnk->index = 24;
        return chnk;
    };


    memchunk* a, *b, *c, *d;
    uint64_t  ta, tb, tc, td;

    for(int i=0;i<10;++i)
    {
        ta = measure_cycles([&]{ a = fallocchunkidx(0, pool); });
        tb = measure_cycles([&]{ b = fallocchunkidx(0, pool); });
        tc = measure_cycles([&]{ c = fallocchunkidx(0, pool); });
        td = measure_cycles([&]{ d = fallocchunkidx(0, pool); });
        std::printf("ALLOC = %zu %zu %zu %zu\n", ta, tb, tc, td);

        ta = measure_cycles([&]{ freechunk(a, pool); });
        tb = measure_cycles([&]{ freechunk(b, pool); });
        tc = measure_cycles([&]{ freechunk(c, pool); });
        td = measure_cycles([&]{ freechunk(d, pool); });
        std::printf("FREE = %zu %zu %zu %zu\n", ta, tb, tc, td);
        std::puts("");
    }

    std::puts("\nAlloc + Free together");
    for(int i=0;i<10;++i)
    {        
        const static size_t N = 1024;
        memchunk* chunks[N];
        ta = measure_cycles([&]{ fallocchunksidx(chunks, N, 0, pool); });
        // for(int i=0; i<N; ++i)
        // {
        //     std::printf("%zu ", chunks[i]->offset);
        // }
        // std::puts("");
        tb = measure_cycles([&]{ freechunks(chunks, N, pool); });
        std::printf("ALLOC = %zu\n", ta);
        std::printf("FREE = %zu\n", tb);
        std::puts("");
    }


    for(auto it : pool.chunksBySize)
    {
        std::printf("%zu\n", it.size());
    }



}
