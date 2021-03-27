#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <cstdlib>

#include <atomic>
#include <array>
#include <mutex>
#include <set>
#include <vector>


struct MemoryHeader
{
    size_t  size;   // last bit is 0 if allocated
                    // since we always align to 64
};


struct MemoryBlock
{
    // Implicit free list
    size_t                      offset;
    std::atomic<size_t>         freeMemory;
    std::vector<MemoryHeader>   headers;
    std::mutex                  lock;
};


MemoryBlock* makeMemoryBlock(size_t offset, size_t size)
{
    MemoryBlock* blk = new MemoryBlock;
    blk->offset = offset;
    blk->freeMemory = size;
    blk->headers.push_back(MemoryHeader{size ^ 1});
    return blk;
}


void freeBlock(MemoryBlock* blk, size_t offset)
{
    std::lock_guard<std::mutex> guard(blk->lock);
    size_t currentOffset = 0;
    size_t prevEmptyElements = 0;
    size_t afterEmptyElements = 0;
    size_t totalEmptySize = 0;

    // Find the entry
    auto start = blk->headers.begin();
    auto end = blk->headers.end();
    auto iter = start;
    for(; iter<end; ++iter)
    {
        if(currentOffset == offset) break;
        size_t blkSize = iter->size;
        size_t inUse = (~blkSize & 1);
        if(inUse)
        {
            prevEmptyElements = 0;
            totalEmptySize = 0;
        }
        else
        {
            blkSize &= (~size_t(0) ^ 1);            // flip off inuse bit
            ++prevEmptyElements;
            totalEmptySize += blkSize;
        }
        currentOffset += blkSize;
    }

    // Mark where we found it
    auto found = iter;
    blk->freeMemory += found->size;
    totalEmptySize += found->size;

    // Keep scanning forward in an attempt to caolesce successive blocks
    ++iter;
    for(; iter<end; ++iter)
    {
        size_t blkSize = iter->size;
        size_t inUse = (~blkSize & 1);
        if(inUse){ break; }
        ++afterEmptyElements;
        totalEmptySize &= (~size_t(0) ^ 1);
    }

    // We were unable to caolesce backwards or forwards
    if(!prevEmptyElements && !afterEmptyElements)
    {
        found->size ^= 1;
        return;
    }

    // Caolesce blocks
    size_t foundBlock = std::distance(start, found);
    size_t startBlock = foundBlock - prevEmptyElements + 1;
    size_t endBlock = foundBlock + afterEmptyElements + 1;
    blk->headers.erase(start+startBlock, start+endBlock);
    blk->headers[startBlock-1].size = totalEmptySize ^ 1;

}


// Returns offset
size_t allocBlock(MemoryBlock* blk, size_t size)
{
    std::lock_guard<std::mutex> guard(blk->lock);
    size_t offset = 0;
    auto iter = blk->headers.begin();
    auto end = blk->headers.end();

    // First fit
    for(; iter<end; ++iter)
    {
        size_t currentSize = iter->size;
        size_t inuse = currentSize & 1;
        currentSize &= ~size_t(0) ^ 1;
        // Found storage
        if(inuse && size <= currentSize)
        {
            // Lazy clear inuse flag
            iter->size = size;
            blk->freeMemory -= currentSize;
            // Gotta do some segmentation
            if(size != currentSize)
            {
                size_t remaining = currentSize - size;
                remaining ^= 1;
                MemoryHeader newHeader;
                newHeader.size = remaining;
                blk->headers.insert(iter+1, newHeader);
            }
            return offset;
        }
        offset += iter->size;
    }

    // No memory found
    return 0;
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


int main()
{

    MemoryBlock* blk = makeMemoryBlock(0, 4096);



    for(int i=0; i<64; ++i)
    {
        size_t o1;
        size_t t = measure_cycles([&]{o1 = allocBlock(blk, 64);});
        std::printf("%zu %zu\n", t, o1);
    }

    std::printf("FREE\n");

    for(int i=0; i<32; ++i)
    {
        size_t t = measure_cycles([&]{freeBlock(blk, 64*i*2);});
        std::printf("%zu\n", t);
    }

    for(int i=0; i<32; ++i)
    {
        size_t o1;
        size_t t = measure_cycles([&]{o1 = allocBlock(blk, 64);});
        std::printf("%zu %zu\n", t, o1);
    }

    std::printf("FREE FWD\n");
    for(int i=0; i<64; ++i)
    {
        size_t t = measure_cycles([&]{freeBlock(blk, 64*i);});
        std::printf("%zu\n", t);
    }

    std::printf("FREE REV\n");

    for(int i=0; i<64; ++i)
    {
        allocBlock(blk, 64);
    }

    for(int i=0; i<64; ++i)
    {
        size_t t = measure_cycles([&]{freeBlock(blk, 64*(63-i));});
        std::printf("%zu\n", t);
    }

}

