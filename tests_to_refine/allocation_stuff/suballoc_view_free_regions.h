
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <cstdlib>

#include <array>
#include <mutex>
#include <shared_mutex>
#include <set>
#include <vector>
#include <unordered_set>


struct MemoryRegion
{
    size_t  offset = 0;
    size_t  size = 0;
};


struct MemoryBlock
{
    size_t maxFreeSize = 0;
    std::vector<MemoryRegion>   freeRegions;
    std::mutex lock;
};


struct BoundMemoryRegion
{
    MemoryRegion    region;
    MemoryBlock*    blk;
};


struct MemoryPool
{
    // We store the block boundaries, which are used
    // to determine roughly which region to start attempting
    // allocating memory from
    size_t      minBlockSize = 0;
    size_t      midBlockSize = 0;
    size_t      maxBlockSize = 0;

    // Available regions are sorted by maxFreeSize
    std::vector<MemoryBlock*>  availableBlocks;
    std::vector<MemoryBlock*>  fullBlocks;

    std::shared_mutex           availableBlocksLock;
    std::mutex                  fullBlocksLock;

    // When true, full blocks need to evaluated and if nessacary
    // added back into available blocks
    bool                        dirty = false;

};


void updatePoolFullBlocks(MemoryPool* pool)
{
    std::lock_guard<std::mutex>         fullGuard(pool->fullBlocksLock);
    std::unique_lock<std::shared_mutex> availableGuard(pool->availableBlocksLock);

    if(!pool->dirty) { return; }

    auto fullIter = pool->fullBlocks.begin();
    auto fullEnd = pool->fullBlocks.end();

    for(; fullIter < fullEnd; ++fullIter)
    {
        MemoryBlock* blk = *fullIter;
        const size_t maxFreeSize = blk->maxFreeSize;
        if(maxFreeSize)
        {
            fullIter = pool->fullBlocks.erase(fullIter);
            fullEnd = pool->fullBlocks.end();

            // Find where to insert it (linear search, possily should use bsearch)
            auto availIter = pool->availableBlocks.begin();
            auto availEnd = pool->availableBlocks.end();
            for(; availIter<availEnd ; ++availIter)
            {
                if((*availIter)->maxFreeSize > maxFreeSize)
                {
                    break;
                }
            }
            pool->availableBlocks.insert(availIter, blk);
        }
    }

    // Figure out min / mid / max block sizes
    pool->minBlockSize = pool->fullBlocks[0]->maxFreeSize;
    pool->midBlockSize = pool->fullBlocks[pool->fullBlocks.size()/2]->maxFreeSize;
    pool->maxBlockSize = pool->fullBlocks.back()->maxFreeSize;
    pool->dirty = false;

}


MemoryRegion allocRegion(MemoryBlock* blk, size_t size)
{
    MemoryRegion region;
    if(size > blk->maxFreeSize) { return region; }

    std::lock_guard<std::mutex> guard(blk->lock);
    const size_t maxFreeSize = blk->maxFreeSize;
    if(size > maxFreeSize) { return region; }


    // At this point there is guranteed to be available memory
    auto iter = blk->freeRegions.begin();

    for(;1;++iter)
    {
        size_t regionSize = iter->size;
        if(size > regionSize) { continue; }

        // Found a match
        region.offset = iter->offset;
        region.size = size;

        // Erase the region if its a exact match
        if(regionSize == size)
        {
            blk->freeRegions.erase(iter);
        }
        // Otherwise update it
        else
        {
            iter->offset += size;
            iter->size -= size;
        }

        // Update the max free size if it matched the region
        if(regionSize == maxFreeSize)
        {
            size_t newMaxFreeSize = 0;
            for(const auto it : blk->freeRegions)
            {
                size_t newRegionSize = it.size;
                if(newRegionSize > newMaxFreeSize)
                {
                    newMaxFreeSize = newRegionSize;
                }
            }

            if(newMaxFreeSize != maxFreeSize)
            {
                blk->maxFreeSize = newMaxFreeSize;
            }
        }

        return region;
    }

}

void freeRegion(MemoryBlock* blk, const MemoryRegion region)
{
    std::lock_guard<std::mutex> guard(blk->lock);

    // Easy case
    if(blk->freeRegions.empty())
    {
        blk->freeRegions.push_back(region);
        blk->maxFreeSize = region.size;
        return;
    }

    size_t maxFreeSize = blk->maxFreeSize;

    auto start = blk->freeRegions.begin();

    // Check if this region is at the front
    if(start->offset > region.offset)
    {
        // Caolesce
        if((region.offset + region.size) == start->offset)
        {
            start->offset = region.offset;
            start->size += region.size;
            if(maxFreeSize < start->size)
            {
                blk->maxFreeSize = start->size;
            }
            return;
        }

        // Just insert
        blk->freeRegions.insert(start, region);
        if(maxFreeSize < region.size)
        {
            blk->maxFreeSize = region.size;
        }
        return;
    }

    // Find the region that should be before where this one would be placed
    auto end = blk->freeRegions.end();
    auto found = start;

    for(; found<end; ++found)
    {
        if(found->offset > region.offset)
        {
            --found;
            break;
        }
    }

    // Attempt to caolesce
    const static int CAOLESCE_LEFT_FLAG = 1;
    const static int CAOLESCE_RIGHT_FLAG = 2;
    int caolesceFlag = 0;

    // We're at the tail of the entries
    if(found == end)
    {
        --found;
        // Can only caolesce left
        caolesceFlag = (found->offset + found->size) == region.offset; // CAOLESCE_LEFT_FLAG
    }
    else
    {
        caolesceFlag = (found->offset + found->size) == region.offset;                // CAOLESCE_LEFT_FLAG
        caolesceFlag |= int((region.offset + region.size) == (found+1)->offset) << 1; // CAOLESCE_RIGHT_FLAG
    }

    // Generate a jump table
    switch(caolesceFlag)
    {
        // Just insert and run
        case 0: {
            blk->freeRegions.insert(found+1, region);
            if(maxFreeSize < region.size)
            {
                blk->maxFreeSize = region.size;
            }
            break;
        }

        // Merge left
        case CAOLESCE_LEFT_FLAG: {
            found->size += region.size;
            if(maxFreeSize < found->size)
            {
                blk->maxFreeSize = found->size;
            }
            break;
        }

        // Merge right
        case CAOLESCE_RIGHT_FLAG: {
            ++found;
            found->offset = region.offset;
            found->size += region.size;
            if(maxFreeSize < found->size)
            {
                blk->maxFreeSize = found->size;
            }
            break;
        }

        // Merge the region to add and the successive region into a new region
        case (CAOLESCE_LEFT_FLAG | CAOLESCE_RIGHT_FLAG): {
            found->size += (region.size + (found+1)->size);
            blk->freeRegions.erase(found+1);
            if(maxFreeSize < found->size)
            {
                blk->maxFreeSize = found->size;
            }
            break;
        }

        default: {
            #if defined(_MSC_VER)
                __assume(0);
            #elif defined(__GNUC__)
                __builtin_unreachable();
            #else
                break;
            #endif
        }
    }

}


inline void freeRegion(BoundMemoryRegion boundMemory)
{
    freeRegion(boundMemory.blk, boundMemory.region);
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

    MemoryBlock blk;
    blk.maxFreeSize = 4096;
    blk.freeRegions.push_back({0, blk.maxFreeSize});


    std::puts("Just allocating");
    for(int i=0; i<32; ++i)
    {
        MemoryRegion o1;
        size_t t = measure_cycles([&]{o1 = allocRegion(&blk, 64);});
        std::printf("%zu -> %zu %zu\n", t, o1.offset, o1.size);
    }

    blk.maxFreeSize = 4096;
    blk.freeRegions.clear();
    blk.freeRegions.push_back({0, blk.maxFreeSize});

    std::puts("\nAlloc + free in order");
    for(int i=0; i<5; ++i)
    {
        MemoryRegion a, b, c, d;
        size_t Aa = measure_cycles([&]{a = allocRegion(&blk, 64);});
        size_t Ab = measure_cycles([&]{b = allocRegion(&blk, 64);});
        size_t Ac = measure_cycles([&]{c = allocRegion(&blk, 64);});
        size_t Ad = measure_cycles([&]{d = allocRegion(&blk, 64);});
        size_t Fa = measure_cycles([&]{ freeRegion(&blk, a);});
        size_t Fb = measure_cycles([&]{ freeRegion(&blk, b);});
        size_t Fc = measure_cycles([&]{ freeRegion(&blk, c);});
        size_t Fd = measure_cycles([&]{ freeRegion(&blk, d);});
        std::printf("ALLOC = %zu %zu %zu %zu\n", Aa, Ab, Ac, Ad);
        std::printf("FREE = %zu %zu %zu %zu\n", Fa, Fb, Fc, Fd);
    }

    std::puts("\nAlloc + free rev");
    for(int i=0; i<5; ++i)
    {
        MemoryRegion a, b, c, d;
        size_t Aa = measure_cycles([&]{a = allocRegion(&blk, 64);});
        size_t Ab = measure_cycles([&]{b = allocRegion(&blk, 64);});
        size_t Ac = measure_cycles([&]{c = allocRegion(&blk, 64);});
        size_t Ad = measure_cycles([&]{d = allocRegion(&blk, 64);});
        size_t Fd = measure_cycles([&]{ freeRegion(&blk, d);});
        size_t Fc = measure_cycles([&]{ freeRegion(&blk, c);});
        size_t Fb = measure_cycles([&]{ freeRegion(&blk, b);});
        size_t Fa = measure_cycles([&]{ freeRegion(&blk, a);});
        std::printf("ALLOC = %zu %zu %zu %zu\n", Aa, Ab, Ac, Ad);
        std::printf("FREE = %zu %zu %zu %zu\n", Fa, Fb, Fc, Fd);
    }


    std::puts("\nAlloc + free out of order");
    for(int i=0; i<5; ++i)
    {
        MemoryRegion a, b, c, d;
        size_t Aa = measure_cycles([&]{a = allocRegion(&blk, 64);});
        size_t Ab = measure_cycles([&]{b = allocRegion(&blk, 64);});
        size_t Ac = measure_cycles([&]{c = allocRegion(&blk, 64);});
        size_t Ad = measure_cycles([&]{d = allocRegion(&blk, 64);});
        size_t Fb = measure_cycles([&]{ freeRegion(&blk, b);});
        size_t Fd = measure_cycles([&]{ freeRegion(&blk, d);});
        size_t Fc = measure_cycles([&]{ freeRegion(&blk, c);});
        size_t Fa = measure_cycles([&]{ freeRegion(&blk, a);});
        std::printf("ALLOC = %zu %zu %zu %zu\n", Aa, Ab, Ac, Ad);
        std::printf("FREE = %zu %zu %zu %zu\n", Fa, Fb, Fc, Fd);
    }


    std::puts("\nAlloc + free different sizes out of order");
    for(int i=0; i<5; ++i)
    {
        MemoryRegion a, b, c, d, e, f, g, h;
        size_t Aa = measure_cycles([&]{a = allocRegion(&blk, 64);});
        size_t Ab = measure_cycles([&]{b = allocRegion(&blk, 256);});
        size_t Ac = measure_cycles([&]{c = allocRegion(&blk, 32);});
        size_t Ad = measure_cycles([&]{d = allocRegion(&blk, 1024);});
        size_t Ae = measure_cycles([&]{e = allocRegion(&blk, 32);});
        size_t Af = measure_cycles([&]{f = allocRegion(&blk, 128);});
        size_t Ag = measure_cycles([&]{g = allocRegion(&blk, 24);});
        size_t Ah = measure_cycles([&]{h = allocRegion(&blk, 16);});
        size_t Fe = measure_cycles([&]{ freeRegion(&blk, e);});
        size_t Fa = measure_cycles([&]{ freeRegion(&blk, a);});
        size_t Fd = measure_cycles([&]{ freeRegion(&blk, d);});
        size_t Fg = measure_cycles([&]{ freeRegion(&blk, g);});
        size_t Fc = measure_cycles([&]{ freeRegion(&blk, c);});
        size_t Fb = measure_cycles([&]{ freeRegion(&blk, b);});
        size_t Ff = measure_cycles([&]{ freeRegion(&blk, f);});
        size_t Fh = measure_cycles([&]{ freeRegion(&blk, h);});
        std::printf("ALLOC = %zu %zu %zu %zu %zu %zu %zu %zu\n", Aa, Ab, Ac, Ad, Ae, Af, Ag, Ah);
        std::printf("FREE = %zu %zu %zu %zu %zu %zu %zu %zu\n", Fa, Fb, Fc, Fd, Fe, Ff, Fg, Fh);
    }

    std::puts("\nAlloc interleaved with free different sizes");
    for(int i=0; i<5; ++i)
    {
        MemoryRegion a, b, c, d, e, f, g, h;
        size_t Aa = measure_cycles([&]{a = allocRegion(&blk, 64);});
        size_t Ab = measure_cycles([&]{b = allocRegion(&blk, 64);});
        size_t Ac = measure_cycles([&]{c = allocRegion(&blk, 1024);});
        size_t Fa = measure_cycles([&]{ freeRegion(&blk, a);});
        size_t Ad = measure_cycles([&]{d = allocRegion(&blk, 128);});
        size_t Fd = measure_cycles([&]{ freeRegion(&blk, d);});
        size_t Ae = measure_cycles([&]{e = allocRegion(&blk, 32);});
        size_t Fc = measure_cycles([&]{ freeRegion(&blk, c);});
        size_t Fe = measure_cycles([&]{ freeRegion(&blk, e);});
        size_t Ag = measure_cycles([&]{g = allocRegion(&blk, 24);});
        size_t Fg = measure_cycles([&]{ freeRegion(&blk, g);});
        size_t Af = measure_cycles([&]{f = allocRegion(&blk, 128);});
        size_t Ff = measure_cycles([&]{ freeRegion(&blk, f);});
        size_t Fb = measure_cycles([&]{ freeRegion(&blk, b);});
        size_t Ah = measure_cycles([&]{h = allocRegion(&blk, 16);});
        size_t Fh = measure_cycles([&]{ freeRegion(&blk, h);});
        std::printf("ALLOC = %zu %zu %zu %zu %zu %zu %zu %zu\n", Aa, Ab, Ac, Ad, Ae, Af, Ag, Ah);
        std::printf("FREE = %zu %zu %zu %zu %zu %zu %zu %zu\n", Fa, Fb, Fc, Fd, Fe, Ff, Fg, Fh);
    }



}

