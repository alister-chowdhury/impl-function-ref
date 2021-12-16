// This is a first fit suballocator which makes uses of a customized rbtree.
// The node stores additional back and forward pointers to make it easier to
// test for coalescing events when deallocating and stores and extra maxSize
// to help cull paths when iterating the tree to find free space.
//
// There is currently no form of validation with respect to deallocation, so
// if you deallocate something twice, expect UB.
// When / if I need to add some form of debugging around this I will.
//
// The rbtree was based upon the one from GOSTL:
// https://github.com/liyue201/gostl/blob/master/ds/rbtree/rbtree.go
//
// This was intended to be used within the context of suballocations with
// Vulkan, using a size ratio of 1:256 (hence the use of a u32 for size and offset).
//
// Example usage:
//
// FFRbSuballocator allocator;
// allocator.deallocate(0, 1000); // Set an initial size of 1000
//
// FFRbSuballocator::AllocationToken t0 = allocator.allocate(100);
// if(!t0.isValid) { freakOut(); }
//
// ...
//
// allocator.deallocate(t0);
// or
// allocator.deallocate(t0.offset, t0.size);
//
// // Print a graph of the current tree in graphviz format
// // Paste into: http://magjac.com/graphviz-visual-editor/
// allocator.printGraphviz();
//
// Additionally there is FFRbLoggedSuballocator which really just
// wraps around FFRbSuballocator and writes allocation / deallocation
// events to a FILE*, so it can be replayed for debugging purposes.
// (using ffrbll_replay.py to generate a timeline gif)


/////////////////////////////////////////////////////
// .h
////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;


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


template<typename T, u32 brickCount=64>
class BrickBasedPool
{

    static_assert(brickCount > 2, "Brick count can't be less than 3!");

private:
    union Node
    {
        alignas(std::alignment_of_v<T>) char data[sizeof(T)];
        Node*   next;
    };

    struct Brick
    {
        Node nodes[brickCount];
        Brick* next;
    };

public:

    ~BrickBasedPool()
    {
        Brick* brick = bricks;
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
        nd->next = freeNodes;
        freeNodes = nd;
    }

private:

    NOINLINE Node* allocateNode()
    {
        if(freeNodes)
        {
            Node* nd = freeNodes;
            freeNodes = nd->next;
            return nd;
        }
        return allocateBrick();
    }


    NOINLINE Node* allocateBrick()
    {
        Brick* newBrick = new Brick;
        newBrick->next = bricks;
        bricks = newBrick;

        for(u32 i=1; i<brickCount-1; ++i)
        {
            newBrick->nodes[i].next = &newBrick->nodes[i+1];
        }
        newBrick->nodes[brickCount-1].next = freeNodes;
        return &newBrick->nodes[0];
    }


    Node*  freeNodes = nullptr;
    Brick* bricks = nullptr;
};


class FFRbSuballocator
{

public:

    struct AllocatorToken
    {
        u32 offset;
        u32 size;
        u8  isValid;
    };

    FFRbSuballocator();
    ~FFRbSuballocator();

    AllocatorToken allocate(u32 size, u32 alignment=1);
    FORCEINLINE void deallocate(AllocatorToken token) { if(token.isValid) { deallocate(token.offset, token.size); }}
    void deallocate(u32 offset, u32 size);

    void writeGraphviz(std::FILE* fp) const;
    FORCEINLINE void writeGraphviz(const char* filepath) const
    {
        std::FILE* fp = std::fopen(filepath, "w");
        writeGraphviz(fp);
        std::fclose(fp);
    }
    FORCEINLINE void printGraphviz() const { writeGraphviz(stdout); }

private:
    struct FFRbNode
    {
        u32 offset;
        u32 size;
        // Max of all descending nodes
        u32 maxSize;
        // 0 = black, 1 = red
        u8 colour;
        u8 unused[3];

        // Parent node
        FFRbNode* parent;
        union
        {
            FFRbNode* children[2];
            struct { FFRbNode* left, *right; };
        };

        // Iterator based double-linked-list for faster merging (to be measured later)
        union
        {
            FFRbNode* iterator[2];
            struct { FFRbNode* it_back, *it_fwd; };
        };

        FFRbNode() { reset(); }

        FORCEINLINE void flipColour()                       { colour ^= 1; }
        FORCEINLINE void setBlack()                         { colour = 0; }
        FORCEINLINE void setRed()                           { colour = 1; }
        FORCEINLINE void copyColour(const FFRbNode* other)
        {
            if(other) { colour = other->colour; }
            else { setBlack(); }
        }

        FORCEINLINE void reset()
        {
            // Zero all pointers, size etc
            std::memset(this, 0, sizeof(FFRbNode));
            setRed();
        }

        FORCEINLINE FFRbNode* next() { return it_fwd; }
        FORCEINLINE FFRbNode* back() { return it_back; }

    };

    FFRbNode* allocateFind(FFRbNode* nd, u32 size, u32 alignment);

    static FORCEINLINE bool updateMaxSize(FFRbNode* nd);
    static void updateMaxSizeRecursive(FFRbNode* nd);

    static FORCEINLINE bool isRed(FFRbNode* nd) { return (nd && nd->colour); }

    void rotateLeft(FFRbNode* nd);
    void rotateRight(FFRbNode* nd);
    
    FFRbNode* insert(u32 offset, u32 size);
    FFRbNode* insertAfter(FFRbNode* parent, u32 offset, u32 size);
    FFRbNode* insertAt(FFRbNode* parent, u8 side, u32 offset, u32 size);
    void insertFixup(FFRbNode* nd);

    void removeNode(FFRbNode* nd);
    void deleteFixup(FFRbNode* nd, FFRbNode* parent);
    FFRbNode* deleteFixupLeft(FFRbNode* nd, FFRbNode* parent);
    FFRbNode* deleteFixupRight(FFRbNode* nd, FFRbNode* parent);

    enum CoalasceFlag : u8
    {
        NONE  = 0b00,
        LEFT  = 0b01,
        RIGHT = 0b10,
        BOTH  = 0b11
    };

    FORCEINLINE CoalasceFlag canMerge(u32 offset, u32 size, const FFRbNode* left, const FFRbNode* right) const;
    static void writeGraphvizIter(std::FILE* fp, const FFRbNode* nd, const FFRbNode* expectedParent);

    FFRbNode*                   root = nullptr;
    BrickBasedPool<FFRbNode>    pool;

};


struct FFRbLoggedSuballocator
{

    FFRbLoggedSuballocator(std::FILE* fp, bool closeOnExit)
    : fp(fp), closeOnExit(closeOnExit)
    {}

    FFRbLoggedSuballocator() : FFRbLoggedSuballocator(stdout, false)
    {}

    FFRbLoggedSuballocator(const char* filepath)
    : FFRbLoggedSuballocator(std::fopen(filepath, "w"), true)
    {}

    ~FFRbLoggedSuballocator()
    {
        if(closeOnExit)
        {
            std::fclose(fp);
        }
    }

    FFRbSuballocator::AllocatorToken allocate(u32 size, u32 alignment=1)
    {
        FFRbSuballocator::AllocatorToken token = allocator.allocate(size, alignment);
        if(token.isValid)
        {
            std::fprintf(fp, "- %u %u\n", token.offset, token.size);
        }
        return token;
    }

    FORCEINLINE void deallocate(FFRbSuballocator::AllocatorToken token) { if(token.isValid) { deallocate(token.offset, token.size); }}
    void deallocate(u32 offset, u32 size)
    {
        if(size > 0)
        {
            std::fprintf(fp, "+ %u %u\n", offset, size);
        }
        allocator.deallocate(offset, size);
    }

    FORCEINLINE void writeGraphviz(std::FILE* fp) const { allocator.writeGraphviz(fp); }
    FORCEINLINE void writeGraphviz(const char* filepath) const { allocator.writeGraphviz(filepath); }
    FORCEINLINE void printGraphviz() const { allocator.printGraphviz(); }


private:
    bool closeOnExit = false;
    std::FILE* fp = nullptr;

    FFRbSuballocator allocator;
};


/////////////////////////////////////////////////////
// .cpp
////////////////////////////////////////////////////


using FFRbSa = FFRbSuballocator;

FFRbSa::FFRbSuballocator() {}
FFRbSa::~FFRbSuballocator() {}


FFRbSa::AllocatorToken FFRbSa::allocate(u32 size, u32 alignment)
{
    AllocatorToken result {0};

    if(!size) { return result; }

    FFRbNode* found = allocateFind(root, size, alignment);

    if(found)
    {
        result.isValid = true;
        result.size = size;

        // Total consumption (implies no alignment padding)
        if(size == found->size)
        {
            result.offset = found->offset;
            removeNode(found);
        }

        // Partial consumption
        else
        {
            // Always assuming alignment is a power of 2
            u32 alignedOffset = ((found->offset - 1) | (alignment - 1)) + 1;
            u32 alignmentPadding = alignedOffset - found->offset;

            // Simple
            if(alignmentPadding == 0)
            {
                result.offset = found->offset;
                found->offset += size;
                found->size -= size;
                updateMaxSizeRecursive(found);
            }

            // Can we just chop of the tail post alignment?
            else if(found->size == (size + alignmentPadding))
            {
                result.offset = found->offset + alignmentPadding;
                found->size -= size;
                updateMaxSizeRecursive(found);
            }

            // We need to split the node out
            else
            {
                result.offset = found->offset + alignmentPadding;

                u32 newOffset = found->offset + alignmentPadding + size;
                u32 newSize = found->size - alignmentPadding - size;

                // Original node now only contains the alignment padding amount
                found->size = alignmentPadding;
                updateMaxSizeRecursive(found);
                // New node starts after the original offset + padding + allocsize
                // and contains only what remains
                insertAfter(found, newOffset, newSize);
            }
        }
    }

    return result;
}


void FFRbSa::deallocate(u32 offset, u32 size)
{

    if(!size) { return; }

    if(!root)
    {
        insertAt(nullptr, 0, offset, size);
        return;
    }

    FFRbNode* nd = root;

    for(;;)
    {
        u8 side = offset > nd->offset;
        if(!nd->children[side])
        {

            // Found where we're either going to insert a node
            // or coalesce the node.
            FFRbNode* back = nullptr;
            FFRbNode* fwd = nullptr;

            if(side == 0)
            {
                fwd = nd;
                back = nd->it_back;
            }
            else
            {
                back = nd;
                fwd = nd->it_fwd;
            }

            switch(canMerge(offset, size, back, fwd))
            {

                // Merge into the back node
                case LEFT:
                {
                    back->size += size;
                    updateMaxSizeRecursive(back);
                    break;
                }

                // Merge into the fwd node
                case RIGHT:
                {
                    fwd->offset = offset;
                    fwd->size += size;
                    updateMaxSizeRecursive(fwd);
                    break;
                }

                // Merge into to both
                case BOTH:
                {
                    // update back and remove fwd
                    back->size += size;
                    back->size += fwd->size;
                    updateMaxSizeRecursive(back);
                    removeNode(fwd);
                    break;
                }

                // Can't merge, just insert
                default:
                case NONE:
                {
                    insertAt(nd, side, offset, size);
                    break;                    
                }

            }

            return;

        }
        nd = nd->children[side];
    }

}


void FFRbSa::writeGraphviz(std::FILE* fp) const
{
    std::fprintf(fp, "digraph {\ngraph [ordering=\"out\"];");
    if(root)
    {
        writeGraphvizIter(fp, root, nullptr);
    }
    std::fprintf(fp, "}");
}


FFRbSa::FFRbNode* FFRbSa::allocateFind(FFRbSa::FFRbNode* nd, u32 size, u32 alignment)
{

    if(!nd || (nd->maxSize < size)) { return nullptr; }

    if(FFRbNode* foundLeft = allocateFind(nd->left, size, alignment))
    {
        return foundLeft;
    }

    if(nd->size >= size)
    {
        // Always assuming alignment is a power of 2
        u32 alignedOffset = ((nd->offset - 1) | (alignment - 1)) + 1;
        u32 alignmentPadding = alignedOffset - nd->offset;
        if(nd->size >= (size + alignmentPadding))
        {
            return nd;
        }
    }

    return allocateFind(nd->right, size, alignment);
}


bool FFRbSa::updateMaxSize(FFRbSa::FFRbNode* nd)
{
    u32 currentMaxSize = nd->maxSize;
    u32 newMaxSize = nd->size;

    for(u32 i=0; i<2; ++i)
    {
        if(nd->children[i])
        {
            newMaxSize = std::max(newMaxSize, nd->children[i]->maxSize);
        }
    }

    if(newMaxSize != currentMaxSize)
    {
        nd->maxSize = newMaxSize;
        return true;
    }
    return false;
}


void FFRbSa::updateMaxSizeRecursive(FFRbSa::FFRbNode* nd)
{
    while(nd && updateMaxSize(nd)) { nd = nd->parent; }
}


void FFRbSa::rotateLeft(FFRbSa::FFRbNode* nd)
{
    FFRbNode* child = nd->right;
    nd->right = child->left;

    if(nd->right) { nd->right->parent = nd; }

    child->parent = nd->parent;
    if(!child->parent)              { root = child; }
    else if(nd == nd->parent->left) { nd->parent->left = child; }
    else                            { nd->parent->right = child; }

    child->left = nd;
    nd->parent = child;

    updateMaxSize(nd);
    updateMaxSize(child);
}


void FFRbSa::rotateRight(FFRbSa::FFRbNode* nd)
{
    FFRbNode* child = nd->left;
    nd->left = child->right;

    if(nd->left) { nd->left->parent = nd; }

    child->parent = nd->parent;
    if(!child->parent)               { root = child; }
    else if(nd == nd->parent->right) { nd->parent->right = child; }
    else                             { nd->parent->left = child; }

    child->right = nd;
    nd->parent = child;

    updateMaxSize(nd);
    updateMaxSize(child);

}


FFRbSa::FFRbNode* FFRbSa::insert(u32 offset, u32 size)
{
    if(!root)
    {
        return insertAt(nullptr, 0, offset, size);
    }

    return insertAfter(root, offset, size);
}


FFRbSa::FFRbNode* FFRbSa::insertAfter(FFRbSa::FFRbNode* nd, u32 offset, u32 size)
{
    for(;;)
    {
        u8 side = offset > nd->offset;
        if(!nd->children[side])
        {
            return insertAt(nd, side, offset, size);
        }
        nd = nd->children[side];
    }
}


FFRbSa::FFRbNode* FFRbSa::insertAt(FFRbSa::FFRbNode* parent, u8 side, u32 offset, u32 size)
{
    FFRbNode* nd = pool.get();
    nd->offset = offset;
    nd->size = size;
    nd->maxSize = size;
    nd->parent = parent;

    if(!parent)
    {
        root = nd;
        root->setBlack();
    }
    else
    {
        parent->children[side] = nd;

        // Fixup iterators
        nd->iterator[1-side] = parent;
        nd->iterator[side]   = parent->iterator[side];
        for(u8 i=0; i<2; ++i)
        {
            if(nd->iterator[i]) { nd->iterator[i]->iterator[1-i] = nd; }
        }

        updateMaxSizeRecursive(nd->parent);
        insertFixup(nd);
    }

    return nd;
}


void FFRbSa::insertFixup(FFRbSa::FFRbNode* nd)
{
    while(isRed(nd->parent))
    {
        FFRbNode* grandparent = nd->parent->parent;
        if(nd->parent == grandparent->left)
        {
            FFRbNode* uncle = grandparent->right;
            if(isRed(uncle))
            {
                nd->parent->setBlack();
                uncle->setBlack();
                grandparent->setRed();
                nd = grandparent;
            }
            else
            {
                if(nd == nd->parent->right)
                {
                    nd = nd->parent;
                    rotateLeft(nd);
                    grandparent = nd->parent->parent;
                }
                nd->parent->setBlack();
                grandparent->setRed();
                rotateRight(grandparent);
            }
        }

        else
        {
            FFRbNode* uncle = grandparent->left;
            if(isRed(uncle))
            {
                nd->parent->setBlack();
                uncle->setBlack();
                grandparent->setRed();
                nd = grandparent;
            }
            else
            {
                if(nd == nd->parent->left)
                {
                    nd = nd->parent;
                    rotateRight(nd);
                    grandparent = nd->parent->parent;
                }
                nd->parent->setBlack();
                grandparent->setRed();
                rotateLeft(grandparent);
            }
        }
    }

    root->setBlack();
}


void FFRbSa::removeNode(FFRbSa::FFRbNode* nd)
{
    if(!nd) return;

    FFRbNode* successor = (nd->left && nd->right) ? nd->next() : nd;
    FFRbNode* x = successor->left ? successor->left : successor->right;

    FFRbNode* parent = successor->parent;
    if(x)
    {
        x->parent = parent;
    }

    if(!parent) { root = x; }
    else if(successor == parent->left) { parent->left = x; }
    else { parent->right = x; }

    // Remove old links
    if(nd->it_back) { nd->it_back->it_fwd = nd->it_fwd; }
    if(nd->it_fwd)  { nd->it_fwd->it_back = nd->it_back; }
    
    // Replace nd data/links with successor data/links as
    // nd will now represent the successor
    if(successor != nd)
    {
        nd->size = successor->size;
        nd->offset = successor->offset;
        nd->it_back = successor->it_back;
        nd->it_fwd = successor->it_fwd;
        if(nd->it_back) { nd->it_back->it_fwd = nd; }
        if(nd->it_fwd)  { nd->it_fwd->it_back = nd; }
    }

    updateMaxSizeRecursive(nd);

    if(!isRed(successor))
    {
        deleteFixup(x, parent);
    }

    // successor should no longer be linked to anything
    pool.release(successor);

}


void FFRbSa::deleteFixup(FFRbSa::FFRbNode* nd, FFRbSa::FFRbNode* parent)
{
    while((nd != root) && !isRed(nd))
    {
        
        if(nd == parent->left) { nd = deleteFixupLeft(nd, parent); }
        else                   { nd = deleteFixupRight(nd, parent); }
        if(nd) { parent = nd->parent; }
    }

    if(nd) { nd->setBlack(); }
}


FFRbSa::FFRbNode* FFRbSa::deleteFixupLeft(FFRbSa::FFRbNode* nd, FFRbSa::FFRbNode* parent)
{
    FFRbNode* w = parent->right;
    if(isRed(w))
    {
        w->setBlack();
        parent->setRed();
        rotateLeft(parent);
        w = parent->right;
    }

    if(!isRed(w->left) && !isRed(w->right))
    {
        w->setRed();
        nd = parent;
    }

    else
    {
        if(!isRed(w->right))
        {
            if(w->left) { w->left->setBlack(); }
            w->setRed();
            rotateRight(w);
            w = parent->right;
        }

        w->copyColour(parent);
        parent->setBlack();
        if(w->right) { w->right->setBlack(); }
        rotateLeft(parent);
        nd = root;
    }

    return nd;
}


FFRbSa::FFRbNode* FFRbSa::deleteFixupRight(FFRbSa::FFRbNode* nd, FFRbSa::FFRbNode* parent)
{
    FFRbNode* w = parent->left;
    if(isRed(w))
    {
        w->setBlack();
        parent->setRed();
        rotateRight(parent);
        w = parent->left;
    }

    if(!isRed(w->left) && !isRed(w->right))
    {
        w->setRed();
        nd = parent;
    }

    else
    {
        if(!isRed(w->left))
        {
            if(w->right) { w->right->setBlack(); }
            w->setRed();
            rotateLeft(w);
            w = parent->left;
        }

        w->copyColour(parent);
        parent->setBlack();
        if(w->left) { w->left->setBlack(); }
        rotateRight(parent);
        nd = root;
    }

    return nd;
}


FFRbSa::CoalasceFlag FFRbSa::canMerge(u32 offset, u32 size, const FFRbSa::FFRbNode* left, const FFRbSa::FFRbNode* right) const
{
    u8 flag = CoalasceFlag::NONE;
    if(left && (left->offset + left->size == offset)) { flag |= CoalasceFlag::LEFT; }
    if(right && (offset + size == right->offset)) { flag |= CoalasceFlag::RIGHT; }
    return (CoalasceFlag)flag;
}


void FFRbSa::writeGraphvizIter(std::FILE* fp, const FFRbSa::FFRbNode* nd, const FFRbSa::FFRbNode* expectedParent)
{
    auto printNodeName = [&](const FFRbNode* nd) { std::fprintf(fp, "nd%p", (const void*)nd); };

    printNodeName(nd);
    const char* blackLabel = "[label=\"offset:%u\\nsize:%u\\nmaxSize:%u%s\"]\n";
    const char* redLabel = "[label=\"offset:%u\\nsize:%u\\nmaxSize:%u%s\",color=red]\n";
    std::fprintf(
        fp,
        nd->colour ? redLabel : blackLabel,
        nd->offset,
        nd->size,
        nd->maxSize,
        expectedParent == nd->parent ? "" : "\\n[BAD PARENT]"
    );

    // Draw regular tree connections
    //  iterate(left)
    //  node->left
    //  iterate(right)
    //  node->right
    if(nd->left)
    {
        writeGraphvizIter(fp, nd->left, nd);
        // "nd->left"1
        printNodeName(nd); std::fprintf(fp, "->"); printNodeName(nd->left); std::fprintf(fp, "[label=L]");
    }
    else
    {
        // Null node
        printNodeName(nd); std::fprintf(fp, "_LEFT[label=NULL]\n");
        // "nd->nd_LEFT"
        printNodeName(nd); std::fprintf(fp, "->"); printNodeName(nd); std::fprintf(fp, "_LEFT[label=L]\n");
    }

    if(nd->right)
    {
        writeGraphvizIter(fp, nd->right, nd);
        // "nd->right"
        printNodeName(nd); std::fprintf(fp, "->"); printNodeName(nd->right); std::fprintf(fp, "[label=R]\n");
    }
    else
    {
        // Null node
        printNodeName(nd); std::fprintf(fp, "_RIGHT[label=NULL]\n");
        // "nd->nd_RIGHT"
        printNodeName(nd); std::fprintf(fp, "->"); printNodeName(nd); std::fprintf(fp, "_RIGHT[label=R]\n");
    }

    // Draw iterator connections (fwd only)
    if(nd->it_fwd)
    {
        printNodeName(nd);
        std::fprintf(fp, "->");
        printNodeName(nd->it_fwd);
        std::fprintf(fp, "[constraint=false, color=blue]\n");
    }
}
