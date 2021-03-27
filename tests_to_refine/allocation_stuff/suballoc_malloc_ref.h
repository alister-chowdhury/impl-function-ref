
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <cstdlib>

#include <array>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <set>
#include <vector>
#include <unordered_set>



struct memchunk;
typedef memchunk*    memchunkptr;


struct memchunk
{
    size_t      offset;
    size_t      size;
    memchunkptr  prev;
    memchunkptr  next;
};


struct memchunk_pool
{
private:
    std::vector<memchunk*> freeChunks;
    std::vector<memchunk*> blockAllocations;

    memchunk* allocNewChunksAndGet()
    {
        // When theres no free chunks, allocate 64, move 63 into the pool and return the 64th
        // notice we allow the allocation to dangle, and don't attempt to free the resource at
        // any point.
        memchunk* newChunks = (memchunk*)std::malloc(sizeof(memchunk)*64);
        blockAllocations.push_back(newChunks);
        freeChunks.insert(
            freeChunks.end(),
            {
                #define EXPAND_CHUNKS_HELPER8(start)\
                        newChunks+start+0, newChunks+start+1, newChunks+start+2, newChunks+start+3, \
                        newChunks+start+4, newChunks+start+5, newChunks+start+6, newChunks+start+7
                    EXPAND_CHUNKS_HELPER8(0),
                    EXPAND_CHUNKS_HELPER8(8),
                    EXPAND_CHUNKS_HELPER8(16),
                    EXPAND_CHUNKS_HELPER8(24),
                    EXPAND_CHUNKS_HELPER8(32),
                    EXPAND_CHUNKS_HELPER8(48),
                    newChunks+56, newChunks+57, newChunks+58, newChunks+59,
                    newChunks+60, newChunks+61, newChunks+62
                #undef EXPAND_CHUNKS_HELPER8
            }
        );
        return newChunks+63;
    }


public:

    // All functions should be in their own TU
    ~memchunk_pool()
    {
        for(memchunk* chunk : blockAllocations)
        {
            std::free(chunk);
        }
    }

    memchunk* get()
    {
        if(!freeChunks.empty())
        {
            memchunk* chunk = freeChunks.back();
            freeChunks.pop_back();
            return chunk;
        }
        return allocNewChunksAndGet();
    }

    void store(memchunk* chunk)
    {
        freeChunks.push_back(chunk);
    }
};



struct memchunk_pool_queue
{
    std::mutex                                      queueLock;
    std::vector<std::unique_ptr<memchunk_pool>>     queue;

    std::unique_ptr<memchunk_pool>  get()
    {
        std::lock_guard<std::mutex> g(queueLock);
        if(queue.empty())
        {
            return std::make_unique<memchunk_pool>();
        }
        std::unique_ptr<memchunk_pool> entry = std::move(queue.back());
        queue.pop_back();
        return std::move(entry);
    }

    void release(std::unique_ptr<memchunk_pool>&& item)
    {
        queue.push_back(std::move(item));
    }

    struct handle
    {
        handle(std::unique_ptr<memchunk_pool>&& p)
        : pool(std::move(p))
        {}
        ~handle()
        {
            parent->release(std::move(pool));
        }

        memchunk_pool* operator->() const
        {
            return pool.get();
        }

        memchunk_pool& operator*() const
        {
            return *pool;
        }

        std::unique_ptr<memchunk_pool>  pool;
        memchunk_pool_queue*            parent;
    };

    handle get_handle() { return handle(get()); }

};

memchunk_pool_queue poolqueue;
thread_local memchunk_pool_queue::handle pool = poolqueue.get_handle();


memchunk* x()
{
    return pool->get();
}




