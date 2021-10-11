#pragma once


// Header stuff

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <memory>
#include <vector>
#include <new>
#include <type_traits>
#include <utility>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;



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







struct FFRbNode
{
    u32 offset;
    u32 size;

    // RBTree based data
    union
    {
        // Composite data, mainly so we can simplify the flip / set instructions
        // GCC seems to do an excessively large amount of when it comes
        // to doing colour ^= 1, so we need to target colourMaxSize instead
        struct
        {
            // 0 = black, 1 = red
            u32 colour:1;
            // Max of all descending nodes
            u32 maxSize:31;
        };

        u32 colourMaxSize;
    };

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

    // This is presumably, little endian dependant
    FORCEINLINE void flipColour()                       { colourMaxSize ^= 1; }
    FORCEINLINE void setBlack()                         { colourMaxSize &= 0xfffffffe; }
    FORCEINLINE void setRed()                           { colourMaxSize |= 1; }
    FORCEINLINE void copyColour(const FFRbNode* other)
    {
        if(other)
        {
            colourMaxSize = (colourMaxSize & 0xfffffffe) | (other->colourMaxSize & 1);
        }
        else
        {
            setBlack();
        }
    }

    FORCEINLINE void reset()
    {
        // Zero all pointers, size etc
        std::memset(this, 0, sizeof(FFRbNode));
        setRed();
    }

    // Find node with the minimum value, within this nodes subtree
    FFRbNode* minNode()
    {
        FFRbNode* nd = this;
        while(nd->left) { nd = nd->left; }
        return nd;
    }

    // Find node with the maximum value, within this nodes subtree
    FFRbNode* maxNode()
    {
        FFRbNode* nd = this;
        while(nd->right) { nd = nd->right; }
        return nd;
    }


    FORCEINLINE FFRbNode* next() { return it_fwd; }
    FORCEINLINE FFRbNode* back() { return it_back; }

};



struct AllocatorToken
{
    u32 offset;
    u32 size:31;
    u32 isValid:1;
};


struct FFRbSuballocator
{

    AllocatorToken allocate(u32 size, u32 alignment=1);
    FORCEINLINE void deallocate(AllocatorToken token) { if(token.isValid) { deallocate(token.offset, token.size); }}
    void deallocate(u32 offset, u32 size);

    FFRbNode* allocateFind(FFRbNode* nd, u32 size, u32 alignment);

    static FORCEINLINE bool updateMaxSize(FFRbNode* nd);
    static void updateMaxSizeRecursive(FFRbNode* nd);

    static FORCEINLINE bool isRed(FFRbNode* nd) { return (nd && nd->colour); }

    void rotateLeft(FFRbNode* nd);
    void rotateRight(FFRbNode* nd);

    void removeNode(FFRbNode* nd);
    
    FFRbNode* insert(u32 offset, u32 size);
    FFRbNode* insertAfter(FFRbNode* parent, u32 offset, u32 size);
    FFRbNode* insertAt(FFRbNode* parent, u8 side, u32 offset, u32 size);
    void insertFixup(FFRbNode* nd);

    void deleteFixup(FFRbNode* nd, FFRbNode* parent);
    FFRbNode* deleteFixupLeft(FFRbNode* nd, FFRbNode* parent);
    FFRbNode* deleteFixupRight(FFRbNode* nd, FFRbNode* parent);

    void printGraphviz() const;
    static void printGraphvizIter(const FFRbNode* nd, const FFRbNode* expectedParent);

    enum CoalasceFlag : u8
    {
        NONE  = 0b00,
        LEFT  = 0b01,
        RIGHT = 0b10,
        BOTH  = 0b11
    };

    FORCEINLINE CoalasceFlag canMerge(u32 offset, u32 size, const FFRbNode* left, const FFRbNode* right)
    {
        u8 flag = CoalasceFlag::NONE;
        if(left && (left->offset + left->size == offset)) { flag |= CoalasceFlag::LEFT; }
        if(right && (offset + size == right->offset)) { flag |= CoalasceFlag::RIGHT; }
        return (CoalasceFlag)flag;
    }


    FFRbNode*                   root = nullptr;
    BrickBasedPool<FFRbNode>    pool;
};


void FFRbSuballocator::printGraphviz() const
{
    std::puts("digraph {\ngraph [ordering=\"out\"];");
    if(root)
    {
        printGraphvizIter(root, nullptr);
    }
    std::puts("}");
}

void FFRbSuballocator::printGraphvizIter(const FFRbNode* nd, const FFRbNode* expectedParent)
{
    auto printNodeName = [&](const FFRbNode* nd) { std::printf("nd%p", (const void*)nd); };

    printNodeName(nd);
    const char* blackLabel = "[label=\"offset:%u\\nsize:%u\\nmaxSize:%u%s\"]\n";
    const char* redLabel = "[label=\"offset:%u\\nsize:%u\\nmaxSize:%u%s\",color=red]\n";
    std::printf(
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
        printGraphvizIter(nd->left, nd);
        // "nd->left"
        printNodeName(nd); std::printf("->"); printNodeName(nd->left); std::puts("[label=L]");
    }
    else
    {
        // Null node
        printNodeName(nd); std::printf("_LEFT[label=NULL]\n");
        // "nd->nd_LEFT"
        printNodeName(nd); std::printf("->"); printNodeName(nd); std::printf("_LEFT[label=L]\n");
    }

    if(nd->right)
    {
        printGraphvizIter(nd->right, nd);
        // "nd->right"
        printNodeName(nd); std::printf("->"); printNodeName(nd->right); std::puts("[label=R]");
    }
    else
    {
        // Null node
        printNodeName(nd); std::printf("_RIGHT[label=NULL]\n");
        // "nd->nd_RIGHT"
        printNodeName(nd); std::printf("->"); printNodeName(nd); std::printf("_RIGHT[label=R]\n");
    }

    // Draw iterator connections (fwd only)
    if(nd->it_fwd)
    {
        printNodeName(nd);
        std::printf("->");
        printNodeName(nd->it_fwd);
        std::puts("[constraint=false, color=blue]");
    }
}



AllocatorToken FFRbSuballocator::allocate(u32 size, u32 alignment)
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
            u32 alignmentPadding = (found->offset - 1) & (alignment - 1);

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

FFRbNode* FFRbSuballocator::allocateFind(FFRbNode* nd, u32 size, u32 alignment)
{

    if(!nd || (nd->maxSize < size)) { return nullptr; }

    if(FFRbNode* foundLeft = allocateFind(nd->left, size, alignment))
    {
        return foundLeft;
    }

    if(nd->size >= size)
    {
        u32 alignmentPadding = (nd->offset - 1) & (alignment - 1);
        if(nd->size + (size + alignmentPadding))
        {
            return nd;
        }
    }

    return allocateFind(nd->right, size, alignment);
}


void FFRbSuballocator::deallocate(u32 offset, u32 size)
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


bool FFRbSuballocator::updateMaxSize(FFRbNode* nd)
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

void FFRbSuballocator::updateMaxSizeRecursive(FFRbNode* nd)
{
    while(nd && updateMaxSize(nd)) { nd = nd->parent; }
}

void FFRbSuballocator::rotateLeft(FFRbNode* nd)
{

    /*
    child := nd.right
    nd.right = child.left
    */
    FFRbNode* child = nd->right;
    nd->right = child->left;

    /*
    if child.left != nil {
        child.left.parent = nd
    }
    */
    if(nd->right) { nd->right->parent = nd; }

    /*
    child.parent = nd.parent
    if nd.parent == nil {
        t.root = child
    } else if nd == nd.parent.left {
        nd.parent.left = child
    } else {
        nd.parent.right = child
    }
    */
    child->parent = nd->parent;
    if(!child->parent)              { root = child; }
    else if(nd == nd->parent->left) { nd->parent->left = child; }
    else                            { nd->parent->right = child; }

    /*
    child.left = nd
    nd.parent = child
    */
    child->left = nd;
    nd->parent = child;

    updateMaxSize(nd);
    updateMaxSize(child);
}

void FFRbSuballocator::rotateRight(FFRbNode* nd)
{
    /*
    child := nd.left
    nd.left = child.right
    */
    FFRbNode* child = nd->left;
    nd->left = child->right;

    /*
    if child.right != nil {
        child.right.parent = nd
    }
    */
    if(nd->left) { nd->left->parent = nd; }

    /*
    child.parent = nd.parent
    if nd.parent == nil {
        t.root = child
    } else if nd == nd.parent.right {
        nd.parent.right = child
    } else {
        nd.parent.left = child
    }
    */
    child->parent = nd->parent;
    if(!child->parent)               { root = child; }
    else if(nd == nd->parent->right) { nd->parent->right = child; }
    else                             { nd->parent->left = child; }

    /*
    child.right = nd
    nd.parent = child
    */
    child->right = nd;
    nd->parent = child;

    updateMaxSize(nd);
    updateMaxSize(child);

}


FFRbNode* FFRbSuballocator::insert(u32 offset, u32 size)
{
    if(!root)
    {
        return insertAt(nullptr, 0, offset, size);
    }

    return insertAfter(root, offset, size);
}


FFRbNode* FFRbSuballocator::insertAfter(FFRbNode* nd, u32 offset, u32 size)
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

FFRbNode* FFRbSuballocator::insertAt(FFRbNode* parent, u8 side, u32 offset, u32 size)
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

void FFRbSuballocator::insertFixup(FFRbNode* nd)
{
    /*
    for nd.parent != nil && nd.parent.color == RED {
    */
    while(isRed(nd->parent))
    {

        /*
        if nd.parent == nd.parent.parent.left {
            uncle = nd.parent.parent.right
            if uncle != nil && uncle.color == RED {
                nd.parent.color = BLACK
                uncle.color = BLACK
                nd.parent.parent.color = RED
                nd = nd.parent.parent
            } else {
                if nd == nd.parent.right {
                    nd = nd.parent
                    t.leftRotate(nd)
                }
                nd.parent.color = BLACK
                nd.parent.parent.color = RED
                t.rightRotate(nd.parent.parent)
            }
        }
        */
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
        
        /*
        } else {
            uncle = nd.parent.parent.left
            if uncle != nil && uncle.color == RED {
                nd.parent.color = BLACK
                uncle.color = BLACK
                nd.parent.parent.color = RED
                nd = nd.parent.parent
            } else {
                if nd == nd.parent.left {
                    nd = nd.parent
                    t.rightRotate(nd)
                }
                nd.parent.color = BLACK
                nd.parent.parent.color = RED
                t.leftRotate(nd.parent.parent)
            }
        }
        */
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

    /*
    t.root.color = BLACK
    */
    root->setBlack();
}

void FFRbSuballocator::removeNode(FFRbNode* nd)
{
    if(!nd) return;

    /*

            A
           /
          B
         / \
        C   D <-
           /
          E

        nd = D
        successor = A
        x = B
        parent = nullptr
        x->parent = parent | B->parent = nullptr
        D <= A
        root = B

          B
         / \
        C   A
           /
          E

    ---
            A
           /
          B
         / \
     -> C   D
           /
          E

        nd = C
        successor = C
        x = nullptr
        parent = B
        parent->left = x | B->left = nullptr

            A
           /
          B
           \
            D
           /
          E
    */

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


void FFRbSuballocator::deleteFixup(FFRbNode* nd, FFRbNode* parent)
{
    /*
    for x != t.root && getColor(x) == BLACK {
        if x != nil {
            parent = x.parent
        }
        if x == parent.left {
            x, w = t.rbFixupLeft(x, parent, w)
        } else {
            x, w = t.rbFixupRight(x, parent, w)
        }
    }
    */
    while((nd != root) && !isRed(nd))
    {
        
        if(nd == parent->left) { nd = deleteFixupLeft(nd, parent); }
        else                   { nd = deleteFixupRight(nd, parent); }
        if(nd) { parent = nd->parent; }
    }

    /*
    if x != nil {
        x.color = BLACK
    }
    */
    if(nd) { nd->setBlack(); }
}


FFRbNode* FFRbSuballocator::deleteFixupLeft(FFRbNode* nd, FFRbNode* parent)
{
    /*
    w = parent.right
    if w.color == RED {
        w.color = BLACK
        parent.color = RED
        t.leftRotate(parent)
        w = parent.right
    }
    */

    FFRbNode* w = parent->right;
    if(isRed(w))
    {
        w->setBlack();
        parent->setRed();
        rotateLeft(parent);
        w = parent->right;
    }

    /*
    if getColor(w.left) == BLACK && getColor(w.right) == BLACK {
        w.color = RED
        x = parent
    }
    */
    if(!isRed(w->left) && !isRed(w->right))
    {
        w->setRed();
        nd = parent;
    }

    /*
    } else {
        if getColor(w.right) == BLACK {
            if w.left != nil {
                w.left.color = BLACK
            }
            w.color = RED
            t.rightRotate(w)
            w = parent.right
        }
        w.color = parent.color
        parent.color = BLACK
        if w.right != nil {
            w.right.color = BLACK
        }
        t.leftRotate(parent)
        x = t.root
    }
    */
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


FFRbNode* FFRbSuballocator::deleteFixupRight(FFRbNode* nd, FFRbNode* parent)
{
    /*
    w = parent.left
    if w.color == RED {
        w.color = BLACK
        parent.color = RED
        t.rightRotate(parent)
        w = parent.left
    }
    */
    FFRbNode* w = parent->left;
    if(isRed(w))
    {
        w->setBlack();
        parent->setRed();
        rotateRight(parent);
        w = parent->left;
    }

    /*
    if getColor(w.left) == BLACK && getColor(w.right) == BLACK {
        w.color = RED
        x = parent
    }
    */
    if(!isRed(w->left) && !isRed(w->right))
    {
        w->setRed();
        nd = parent;
    }

    /*
    } else {
        if getColor(w.left) == BLACK {
            if w.right != nil {
                w.right.color = BLACK
            }
            w.color = RED
            t.leftRotate(w)
            w = parent.left
        }
        w.color = parent.color
        parent.color = BLACK
        if w.left != nil {
            w.left.color = BLACK
        }
        t.rightRotate(parent)
        x = t.root
    }
    */
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



#if 0
int main(void)
{
    FFRbSuballocator al;
    auto* nd0 = al.pool.get();
    auto* nd1 = al.pool.get();
    auto* nd2 = al.pool.get();
    auto* nd3 = al.pool.get();
    auto* nd4 = al.pool.get();
    auto* nd5 = al.pool.get();
    auto* nd6 = al.pool.get();

    al.root = nd0;
    nd0->left = nd1;
    nd0->right = nd2;
    nd2->left = nd3;
    nd3->left = nd4;
    nd4->left = nd5;
    nd5->left = nd6;

    nd1->it_fwd = nd0;
    nd0->it_fwd = nd6;
    nd6->it_fwd = nd5;
    nd5->it_fwd = nd4;
    nd4->it_fwd = nd3;
    nd3->it_fwd = nd2;

    al.printGraphviz();

}
#else

#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>



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




void handler(int sig) {
  void *array[100];
  size_t size;
  // get void*'s for all entries on the stack
  size = backtrace(array, 100);
  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}


#include <cstdlib>

int main(void)
{
    signal(SIGSEGV, handler);   // install our handler
    FFRbSuballocator al;

#if 0
    al.insert(0, 10);
    al.insert(100, 1);
    al.insert(102, 1);
    al.insert(20, 15);
    al.insert(50, 7);
    al.insert(60, 3);
    al.insert(82, 3);
    al.insert(86, 1);
    al.insert(90, 9);
    al.insert(200, 15);
    al.insert(555, 7);
    al.insert(69, 3);
    al.insert(1000, 100);
    al.insert(10000, 10);
    al.insert(10000 + 100, 1);
    al.insert(10000 + 102, 1);
    al.insert(10000 +20, 15);
    al.insert(10000 +50, 7);
    al.insert(10000 +60, 3);
    al.insert(10000 +82, 3);
    al.insert(10000 +86, 1);
    al.insert(10000 +90, 9);
    al.insert(10000 +200, 15);
    al.insert(10000 +555, 7);
    al.insert(10000 +69, 3);
    al.removeNode(al.root->right);
#elif 1
    al.deallocate(0, 1000);
 
    for(int i=0; i<10; ++i)
    {
        AllocatorToken t1, t2, t3, t4, t5, t6;

        u64 a0 = measure_cycles([&]{ t1 = al.allocate(rand()&15 + 1); });
        u64 a1 = measure_cycles([&]{ t2 = al.allocate(rand()&31 + 1); });
        u64 a2 = measure_cycles([&]{ t3 = al.allocate(rand()&7 + 1); });
        u64 a3 = measure_cycles([&]{ t4 = al.allocate(rand()&15 + 1); });
        u64 a4 = measure_cycles([&]{ t5 = al.allocate(rand()&63 + 1); });
        u64 a5 = measure_cycles([&]{ t6 = al.allocate(rand()&3 + 1); });
        u64 d0 = measure_cycles([&]{al.deallocate(t2);});
        u64 d1 = measure_cycles([&]{al.deallocate(t3);});
        u64 d2 = measure_cycles([&]{al.deallocate(t5);});
        
        std::printf("%lu %lu %lu %lu %lu %lu\n", a0, a1, a2, a3, a4, a5);
        std::printf("%lu %lu %lu\n", d0, d1, d2);
    }

    // al.insert(0x100, 20);

    /*
    auto t1 = al.allocate(1);
    auto t2 = al.allocate(2);
    auto t3 = al.allocate(3);
    auto t4 = al.allocate(4);
    auto t5 = al.allocate(5);
    auto t6 = al.allocate(6);

    al.deallocate(t1);
    al.deallocate(t3);
    al.deallocate(t5);
    */

#else


    al.insert(0, 180);


    auto t0 = al.allocate(126);
    auto t1 = al.allocate(1);

    al.deallocate(t0);
    al.deallocate(t1);
    


#endif
    al.printGraphviz();
}


#endif