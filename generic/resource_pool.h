#pragma once

// This contains the non-threadsafe and lockfree-threadsafe containers:
//
// BrickBasedMemoryPool:
//  A brick based pool of memory for a specific type which can requested and free'd when
//  finished with, so it's raw memory resources can be used again.
//
// e.g:
//
//  BrickBasedMemoryPool<std::vector<int>> pool;
//  std::vector<int>* tmp = pool.get(100); // Initialize with 100 ints
//  pool.release(tmp);                     // Destructor is called and raw memory resource is re-pooled
//
//
// ResourcePool:
//  A base class for pooling resources allocated by a subclass.
//
// e.g:
//
//  struct FencePool : ResourcePool<VkFence, FencePool>
//   {
//       FencePool(VkDevice device) : device(device) {}
//
//   protected:
//       friend class ResourcePool<VkFence, FencePool>;
//       VkFence allocate() { return vkCreateFence(device, ..., nullptr); }
//       void deallocate(VkFence fence) { vkDestroyFence(device, fence, nullptr); }
//       VkDevice    device;
//   };
//
//   ...
//
//  FencePool pool;
//  VkFence fence = pool.get(); // resource may be allocated if not already done
//  pool.release(fence);        // resource is not destroyed, but rather put back into the pool
//
//
// Another rather contrived example could be for allocating IDs
//   template<bool threadSafe=false>
//   struct IntIotaPool : ResourcePool<int, IntIotaPool<threadSafe>, threadSafe>
//   {
//       void deallocate(int) {}
//       int allocate() { return increment++; }
//       int increment = 0;
//   };
//
//
// Lockfree thread saftey can be activated with a template paramter.
//
//   BrickBasedMemoryPool<T, /* threadSafe = */ true>;
//   ResourcePool<T, Subclass, /* threadSafe = */ true>;
//

#include <utility>
#include <atomic>
#include <cstdint>
#include <type_traits>
#include <new>


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


namespace detail
{

template<typename Node, bool threadSafe=false>
struct LinkedListHead
{
    Node* pop()
    {
        Node* nd = head;
        if(nd) { head = nd->next; }
        return nd;
    }

    void push(Node* newNode)
    {
        newNode->next = head;
        head = newNode;
    }

    void pushPreLinked(Node* linkedHead, Node* linkedTail)
    {
        linkedTail->next = head;
        head = linkedHead;
    }

    Node* getRaw() { return head; }

    Node*   head = nullptr;
};


template<typename Node>
struct LinkedListHead<Node, true>
{
    NOINLINE Node* pop()
    {
        // Copy head locally, if our version of the head is not a nullptr
        // and matches what is still stored, set the stored head to nd->next
        // otherwise set nd = head.
        Node* nd = head.load(std::memory_order_relaxed);
        while(nd && !head.compare_exchange_weak(nd,
                                                nd->next,
                                                std::memory_order_release,
                                                std::memory_order_relaxed));
        return nd;
    }

    NOINLINE void push(Node* newNode)
    {
        // Set the new nodes next to the head, if they match, swap head with newNode
        // otherwise set newNode->next to head.
        newNode->next = head.load(std::memory_order_relaxed);
        while(!head.compare_exchange_weak(newNode->next,
                                            newNode,
                                            std::memory_order_release,
                                            std::memory_order_relaxed));
    }

    NOINLINE void pushPreLinked(Node* linkedHead, Node* linkedTail)
    {
        // Set the tails next to head, if they match, swap head with the linkedHead,
        // otherwise set linkedTail->next to head.
        linkedTail->next = head;
        while(!head.compare_exchange_weak(linkedTail->next,
                                            linkedHead,
                                            std::memory_order_release,
                                            std::memory_order_relaxed));
    }

    Node* getRaw() { return head.load(std::memory_order_relaxed); }

    std::atomic<Node*> head { nullptr };
};


} // namespace detail


template<typename T, bool threadSafe=false, uint32_t brickSize=64>
class BrickBasedMemoryPool
{
public:

    ~BrickBasedMemoryPool()
    {
        Brick* brick = bricks.getRaw();
        while(brick)
        {
            Brick* nextBrick = brick->next;
            delete brick;
            brick = nextBrick;
        }
    }

    template<typename... InitArgs>
    FORCEINLINE T* get(InitArgs&&... args)
    {
        return new (allocateNode()) T(std::forward<InitArgs>(args)...);
    }

    FORCEINLINE void release(T* item)
    {
        item->~T();
        Node* nd = (Node*)item;
        freeNodes.push(nd);
    }

private:
    union Node
    {
        alignas(std::alignment_of_v<T>) char data[sizeof(T)];
        Node*   next;
    };

    struct Brick
    {
        Node nodes[brickSize];
        Brick* next;
    };


    NOINLINE Node* allocateNode()
    {
        if(Node* nd = freeNodes.pop()) { return nd; }
        return allocateBrick();
    }

    NOINLINE Node* allocateBrick()
    {
        Brick* newBrick = new Brick;
        bricks.push(newBrick);

        for(uint32_t i=1; i<brickSize-1; ++i)
        {
            newBrick->nodes[i].next = &newBrick->nodes[i+1];
        }
        freeNodes.pushPreLinked( &newBrick->nodes[1], &newBrick->nodes[brickSize-1]);
        return &newBrick->nodes[0];
    }

    detail::LinkedListHead<Node, threadSafe>  freeNodes;
    detail::LinkedListHead<Brick, threadSafe> bricks;
};


template<
    typename T,
    class AllocatorSubClass,
    bool threadSafe=false,
    uint32_t internalMemoryBrickSize=64
>
class ResourcePool
{
public:

    ~ResourcePool()
    {
        Resource* resource = freeResources.getRaw();
        while(resource)
        {
            Resource* nextResource = resource->next;
            ((AllocatorSubClass*)this)->deallocate(resource->value);
            resource = nextResource;
        }
    }

    void release(T value)
    {
        Resource* resource = memoryPool.get();
        resource->value = value;
        freeResources.push(resource);
    }

    T get()
    {
        Resource* resource = freeResources.pop();
        if(resource)
        {
            T value = resource->value;
            memoryPool.release(resource);
            return value;
        }
        return ((AllocatorSubClass*)this)->allocate();
    }

private:
    struct Resource
    {
        T           value;
        Resource*   next;
    };

    detail::LinkedListHead<Resource, threadSafe>                        freeResources;
    BrickBasedMemoryPool<Resource, threadSafe, internalMemoryBrickSize> memoryPool;
};
