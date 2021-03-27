// #define MEM_DBG
#define BSF_IMPL

#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <cstdlib>

#include <array>
#include <mutex>
#include <set>
#include <unordered_set>

inline
unsigned long long bsf(unsigned long long m)
{

#if defined(__GNUC__) || defined(__CLANG__)
    return __builtin_ctzl(m);

#elif defined(_MSC_VER)
    unsigned long long v = 0;
    _BitScanForward64(&v, (unsigned long)m);
    return v;

#else
    unsigned long long value = 0;
    while (m >>= 1) ++value;
    return value;

#endif

}

struct MemoryBlock
{
    MemoryBlock* parent;
    MemoryBlock* children; // left / right
    size_t       size;
    size_t       offset;
    uint8_t      index:7;
    uint8_t      inuse:1;
};


struct AvailableMemoryBlock
{
    // 1GB   = 0
    // 64byte = 24
    std::array<std::set<MemoryBlock*>, 25> blocks;
    std::array<std::mutex, 25> locks;
};




// When freeing a block, remove it as a dependency 
void freeBlock(MemoryBlock* blk, AvailableMemoryBlock* mem)
{
    blk->inuse = false;
    // Clear children if it's been subdivided
    MemoryBlock* children = blk->children;
    if(children)
    {
        // Remove children from the global queue
        {
            std::lock_guard<std::mutex> guard(mem->locks[blk->index+1]);
            mem->blocks[blk->index+1].erase(&children[0]);
            mem->blocks[blk->index+1].erase(&children[1]);
        }
        // Child nodes are allocated at the same time
        std::free(children);
        blk->children = nullptr;
    }
    // Coalesce parent block if possible
    bool freedParent = false;
    MemoryBlock* parent = blk->parent;
    if(parent)
    {
        if(!parent->children[0].inuse && !parent->children[1].inuse)
        {
            freedParent = true;
            freeBlock(parent, mem);
        }
    }
    // If we didn't caolesce the parent, add this block back into the pool
    if(!freedParent)
    {
        std::lock_guard<std::mutex> guard(mem->locks[blk->index]);
        mem->blocks[blk->index].insert(blk);
    }

}

void subdivideBlockRaw(MemoryBlock* blk)
{
    MemoryBlock* children = (MemoryBlock*)std::calloc(2, sizeof(MemoryBlock));
    size_t childSize = blk->size / 2;
    size_t childOffset = blk->offset;
    uint8_t childIndex = blk->index + 1;
    children[0].parent = blk;
    children[0].size = childSize;
    children[0].offset = childOffset;
    children[0].index = childIndex;
    children[1].parent = blk;
    children[1].size = childSize;
    children[1].offset = childOffset + childSize;
    children[1].index = childIndex;
    blk->children = children;
    blk->inuse = true;
}

MemoryBlock* allocate1GB()
{
    MemoryBlock* blk = (MemoryBlock*)std::calloc(1, sizeof(MemoryBlock));
    blk->size = 1073741824;
    // Do vulkan alloc stuff here
    return blk;
}


MemoryBlock* allocateBlockByMemorySizeId(
    size_t memorySizeId,    
    AvailableMemoryBlock* mem
)
{
    // If memorySizeId is 0, it means we need to request 1GB
    if(memorySizeId == 0)
    {
        return allocate1GB();
    }

    std::lock_guard<std::mutex> guard(mem->locks[memorySizeId]);
    // If there are no free blocks, allocate and subdivde
    if(mem->blocks[memorySizeId].empty())
    {
        MemoryBlock* parent = allocateBlockByMemorySizeId(memorySizeId-1, mem);
        subdivideBlockRaw(parent);
        // Add right into the pool
        mem->blocks[memorySizeId].insert(&parent->children[1]);
        parent->children[0].inuse = true;
        #ifdef MEM_DBG
            std::printf("Allocating parent block of %p %lu\n", &parent->children[0], memorySizeId);
        #endif
        return &parent->children[0];
    }
    
    // TODO:
    // We should make use of std::set to be sorted to the tune of the offset.
    // So there should be a bias to returning left side objects first.
    auto entry = mem->blocks[memorySizeId].begin();
    MemoryBlock* blk = *entry;
    mem->blocks[memorySizeId].erase(entry);
    blk->inuse = true;
    #ifdef MEM_DBG
        std::printf("Found block for %p %lu\n", blk, memorySizeId);
    #endif
    return blk;
}


MemoryBlock* allocateBlock(
    AvailableMemoryBlock* mem,
    size_t size
){
    if(size > 1073741824)
    {
        // Anything above 1GB isn't supported
        std::abort();
    }

    // Start at 64b, the max granularity
    size_t memorySizeId = 24;

    // Round size to the next power of two
    size--;
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    size |= size >> 32;
    size++;

    // Guard against null allocs
    if(size)
    {
    #ifdef BSF_IMPL
        memorySizeId -= bsf(size/64);
    #else
        // std::printf("K %llu - ", memorySizeId - bsf(size/64);
        size /= 64;
        for(;size>1;--memorySizeId,size>>=1);
        // printf("%llu\n", memorySizeId);
    #endif
    }

    return allocateBlockByMemorySizeId(
        memorySizeId, mem 
    );

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
    AvailableMemoryBlock m;
    MemoryBlock* blk;
    MemoryBlock* blk2;
    MemoryBlock* blk3;
    MemoryBlock* blk4;

    uint64_t cyc1 = measure_cycles([&]{ blk = allocateBlock(&m, 64); });
    uint64_t cyc2 = measure_cycles([&]{ blk2 = allocateBlock(&m, 64); });
    uint64_t cyc3 = measure_cycles([&]{ blk3 = allocateBlock(&m, 256); });
    uint64_t cyc4 = measure_cycles([&]{ blk4 = allocateBlock(&m, 64); });

    std::printf("\nB:\n--\n%lu\n%lu\n%lu\n%lu\n", cyc1, cyc2, cyc3, cyc4);

    freeBlock(blk, &m);
    freeBlock(blk2, &m);
    freeBlock(blk3, &m);
    freeBlock(blk4, &m);

    cyc1 = measure_cycles([&]{ blk = allocateBlock(&m, 64); });
    cyc2 = measure_cycles([&]{ blk2 = allocateBlock(&m, 64); });
    cyc3 = measure_cycles([&]{ blk3 = allocateBlock(&m, 256); });
    cyc4 = measure_cycles([&]{ blk4 = allocateBlock(&m, 64); });

    std::printf("\nB:\n--\n%lu\n%lu\n%lu\n%lu\n", cyc1, cyc2, cyc3, cyc4);


    cyc1 = measure_cycles([&]{ blk = allocateBlock(&m, 64); });
    cyc2 = measure_cycles([&]{ blk2 = allocateBlock(&m, 64); });
    cyc3 = measure_cycles([&]{ blk3 = allocateBlock(&m, 256); });
    cyc4 = measure_cycles([&]{ blk4 = allocateBlock(&m, 64); });

    std::printf("\nB:\n--\n%lu\n%lu\n%lu\n%lu\n", cyc1, cyc2, cyc3, cyc4);


    cyc1 = measure_cycles([&]{ freeBlock(blk, &m); });
    cyc2 = measure_cycles([&]{ freeBlock(blk2, &m); });
    cyc3 = measure_cycles([&]{ freeBlock(blk3, &m); });
    cyc4 = measure_cycles([&]{ freeBlock(blk4, &m); });
    std::printf("\nF:\n--\n%lu\n%lu\n%lu\n%lu\n", cyc1, cyc2, cyc3, cyc4);

    std::printf("LOOPING\n");

    for(int i=0;i<1024;++i)
    {
        std::printf("ALLOC %lu %lu\n", measure_cycles([&]{ blk = allocateBlock(&m, 64); }), measure_cycles([&]{ blk2 = allocateBlock(&m, 256); }));
        std::printf("      %lu %lu\n", measure_cycles([&]{ blk = allocateBlock(&m, 64); }), measure_cycles([&]{ blk2 = allocateBlock(&m, 256); }));
        std::printf("FREE %lu %lu\n", measure_cycles([&]{ freeBlock(blk, &m); }), measure_cycles([&]{ freeBlock(blk2, &m); }));
    
    }

}

