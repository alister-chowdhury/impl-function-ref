#include <vector>
#include <mutex>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <utility>
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


// Pool of memchunks that are stored as pairs
struct memchunk_pair_pool
{

    memchunk* get();
    void release(memchunk* chnk);

    ~memchunk_pair_pool()
    {
        for(memchunk* chnk : allocations)
        {
            std::free(chnk);
        }
    }


    const static size_t   min_pairs_per_swap = 512;
    const static size_t   number_of_pairs_per_alloc = 1024;

    // We store two buffers here, a buffer for where free chunks go
    // and a buffer for currently available chunks.

    // A atomic pointer and size to the contents of currentchunks is used
    // to prevent needing to pop / lock per request.

    // When there are no more available chunks, the old buffer is cleared
    // the buffers are swapped and if there are small number of entries
    // in the now current buffer, an allocation takes place.

    std::atomic<memchunk**>     ptr { nullptr };
    std::atomic<ssize_t>        count { 0 };

    std::vector<memchunk*>      currentchunks;
    std::vector<memchunk*>      nextchunks;

    std::mutex                  swaplock;
    std::mutex                  nextlock;

    std::atomic<size_t>         readcounter;


    // Allocations are done in bigish blocks;
    std::vector<memchunk*>  allocations;
};



memchunk* memchunk_pair_pool::get()
{
    ++readcounter;
    ssize_t index = count--;

    // Normal use case, just fetch from the pool
    if(index > 0)
    {
        memchunk* chnk = ptr[index];
        --readcounter;
        return chnk;
    }
    
    // Sad use case, we need to swap and possibly allocate
    --readcounter;
    std::lock_guard<std::mutex> swapg(swaplock);
    index = count--;

    // In the time we acquired the lock, something updated everything
    if(index > 0)
    {
        return ptr[index];
    }

    std::lock_guard<std::mutex> nextg(nextlock);
    while(readcounter);   // wait for anything that may still be reading from the old ptr to stop
                          // before beginning a swap.

    if(!nextchunks.empty())
    {
        std::swap(currentchunks, nextchunks);
    }

    // Alright, do an allocation
    if(currentchunks.size() < min_pairs_per_swap)
    {
        memchunk*   newpairs = (memchunk*)std::malloc(sizeof(memchunk)*2*number_of_pairs_per_alloc);
        allocations.push_back(newpairs);
        for(size_t i=0; i<min_pairs_per_swap; ++i)
        {
            currentchunks.push_back(newpairs + i *2);
        }
    }

    memchunk* ret = currentchunks.back();
    ptr = &*currentchunks.begin();
    count = currentchunks.size() - 1;
    return ret;
}

void memchunk_pair_pool::release(memchunk* chnk)
{
    // A bit crapper
    std::lock_guard<std::mutex> nextg(nextlock);
    nextchunks.push_back(chnk);
}





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



memchunk_pair_pool memchunk_pairs;


memchunk* a;
memchunk* b;
memchunk* c;
memchunk* d;
uint64_t cyc1;
uint64_t cyc2;
uint64_t cyc3;
uint64_t cyc4;


int main(void)
{


    for(int i=0; i<100; ++i)
    {
        cyc1 = measure_cycles([&]{ a = memchunk_pairs.get(); });
        cyc2 = measure_cycles([&]{ b = memchunk_pairs.get(); });
        cyc3 = measure_cycles([&]{ c = memchunk_pairs.get(); });
        cyc4 = measure_cycles([&]{ d = memchunk_pairs.get(); });

        std::printf("\nP:\n--\n%lu %lu %lu %lu\n", cyc1, cyc2, cyc3, cyc4);

        cyc1 = measure_cycles([&]{ a = (memchunk*)std::malloc(2*sizeof(memchunk)); });
        cyc2 = measure_cycles([&]{ b = (memchunk*)std::malloc(2*sizeof(memchunk)); });
        cyc3 = measure_cycles([&]{ c = (memchunk*)std::malloc(2*sizeof(memchunk)); });
        cyc4 = measure_cycles([&]{ d = (memchunk*)std::malloc(2*sizeof(memchunk)); });

        std::printf("%lu %lu %lu %lu\n", cyc1, cyc2, cyc3, cyc4);

        cyc1 = measure_cycles([&]{  });
        cyc2 = measure_cycles([&]{  });
        cyc3 = measure_cycles([&]{  });
        cyc4 = measure_cycles([&]{  });

        std::printf("%lu %lu %lu %lu\n", cyc1, cyc2, cyc3, cyc4);
    }

}
