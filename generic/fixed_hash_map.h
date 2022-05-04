#pragma once


// This contains a map which has a fixed size and layout, determined at build time.
// Data is read sequentially, so it can effectively act as a view.
//
// The key is a u64 (so basically a user provided hash or index).
//
// The value must be able to survive a memcpy, because that is how the data is
// fetched.
//
// It priotises being able to get and set values via keys over everything else
// and will typically out perform std::map, std::unordered_map when it comes to
// querying values at runtime.
//
// Internally the map structure is:
//
//  * map header (FhmMapHeader)
//      - u32 bucketCount
//      - u32 entryCount
//
//  * bucket table (FhmBucketHeader[bucketCount])
//      - u32 offset (byte offset from the initial pointer)
//      - u32 count
//
//  * bucket #0
//      - u64 hash0
//      - u64 hash1
//          ...
//      - u8[sizeof(T)] value0
//      - u8[sizeof(T)] value1
//
//  * bucket #1
//      - u64 hash0
//      - u64 hash1
//          ...
//      - u8[sizeof(T)] value0
//      - u8[sizeof(T)] value1
//
//
// The bucket size is by calculated by taking the number of items
// and taking the last power of 2.
// i.e:
//      2 => 1
//      3 => 2
//      7 => 4
//      8 => 4
//    100 => 64
//
// With the bucket id being derived from the lower bits of the key (key & (bucketCount-1)).
// This could be made configurable upon map creation, but haven't found the need
// to do this yet, and as such it currently works a bit like a indirection table.
//
// For the sake of convience all instances can be casted to:
//      FixedHashMap<T, const ByteType*>
//
// Additionally if the data is not read-only:
//      FixedHashMap<T, ByteType*>
//
// For the most part construction, setting/getting and iteration are constexpr
// friendly with the only expection being structures with padded bytes, although
// this appears to be a compiler bug:
//      https://gcc.gnu.org/bugzilla/show_bug.cgi?id=102401
//      https://bugs.llvm.org/show_bug.cgi?id=51925
//
// C++20 is required OR C++17 with the following compiler versions:
//      Clang    12.0.1
//      GCC      11.1
//      MSVC     19.27
//
// This could probably be relaxed a bit if I add the ability to turn off constexpr support
// (which is the reason most of the code here is rather complicated).
//
// For constructing a fixed sized hash map you can use:
//      constexpr auto map1 = createFixedHashMap<Type>(
//           {hash0, value1},
//           {hash2, value2},
//           {hash3, value3},
//           {hash4, value4},
//           ...
//      );
//
//      constexpr auto map2 = createFixedHashMapWithDefaultValue(value, hash0, hash1, hash2, hash3, ... );
//
// And for runtime dependant sizes:
//      FixedHashMap<Type, std::unique_ptr<char[]> map1 = createFixedHashMap<Type>(begin, end);
//
//      container<uint64_t> keys { ... };
//      FixedHashMap<Type, std::unique_ptr<char[]> emptyMap1 = createEmptyFixedHashMap(keys.begin(), keys.end(), 0);
//
//      // Dump to file
//      writeToFile(filepath, map1.data(), map1.byteSize());
//
//      // Then read from file
//      const char* data = loadFile(....);
//      FixedHashMap<Type, const char*> map2 { data };
//
//
// The main motivation for this was for shader compiling and dealing with binding ids between permutations.
// This is something that needs to be queried at run-time, but is (typically) offline generated, and being
// able to offline generate a data blob which is loaded in, or a generated header which is dealt with at
// compile time is ideal.
//
// Additionally, with my implementation of comptile-time wyhash, it becomes possible to do:
//
//      struct Binding
//      {
//          uint32_t    set;
//          uint32_t    binding;
//      };
//      
//      constexpr auto BindingMap = createFixedHashMap<Binding>({
//          {wyhash("diffuse"), {0, 1}},
//          {wyhash("normals"), {0, 2}},
//          {wyhash("height"),  {0, 3}}
//      });
//      
//      ...
//      
//      Binding bnd;
//      bool ok = BindingMap.get(wyhash("diffuse"), bnd);
//
// With the get call being incredibly fast (~6 cycles in a hot-path, last I checked) with the hash itself
// being determined at compile time.


#if (defined(_MSVC_LANG) && (_MSVC_LANG >= 201811L)) || __cplusplus >= 201811L
    #include <bit>
    #define IS_CONSTANT_EVALUATED   std::is_constant_evaluated
    #define BIT_CAST(T, arg)        std::bit_cast<T>(arg)
    #define IF_UNLIKELY(x)          if(x)       [[unlikely]]
#else
    #define IS_CONSTANT_EVALUATED   __builtin_is_constant_evaluated
    #define BIT_CAST(T, arg)        __builtin_bit_cast(T, arg)
    #if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
        #define IF_UNLIKELY(x)      if(__builtin_expect(x, 0))
    #else
        #define IF_UNLIKELY(x)      if(x)
    #endif
#endif

#if defined(_MSC_VER)
    #define FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
    #define FORCEINLINE __attribute__((always_inline)) inline
#else
    #define FORCEINLINE inline
#endif

#define CONSTEXPRINLINE constexpr FORCEINLINE

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>
#include <memory>
#include <utility>


struct FhmMapHeader
{
    uint32_t    entryCount;
    uint32_t    bucketCount;
};

struct FhmBucketHeader
{
    uint32_t    offset;
    uint32_t    count;
};

namespace fhmbuilding
{

CONSTEXPRINLINE size_t calculateFixedHashMapSize(
    const size_t itemCount,
    const size_t itemSize,
    const size_t bucketCount)
{
    return sizeof(FhmMapHeader)
           + sizeof(FhmBucketHeader) * bucketCount
           + sizeof(uint64_t) * itemCount
           + itemSize * itemCount;
}

CONSTEXPRINLINE uint32_t pickBucketCount(const uint32_t itemCount)
{
    // lastPowerOf2(size);
    uint64_t bucketCount = itemCount;
    bucketCount--;
    bucketCount |= bucketCount >> 1;
    bucketCount |= bucketCount >> 2;
    bucketCount |= bucketCount >> 4;
    bucketCount |= bucketCount >> 8;
    bucketCount |= bucketCount >> 16;
    bucketCount++;
    bucketCount >>= 1;

    if(!bucketCount)
    {
        bucketCount = 1;
    }

    return bucketCount;
}


template<size_t i, size_t n, typename T, typename F, typename... Args>
CONSTEXPRINLINE auto unpackArray(F&& f, const T* ptr, Args&&... args)
{
    if constexpr(i == n)
    {
        return f(args...);
    }
    else
    {
        return unpackArray<i+1, n>(std::forward<F>(f), ptr, args..., ptr[i]);
    }
}

template<uint32_t itemCount,
         uint32_t itemSize,
         uint32_t bucketCount_=pickBucketCount(itemCount)>
struct FixedStorage
{
    const static uint32_t bucketCount = bucketCount_;
    const static size_t byteSize = calculateFixedHashMapSize(
        itemCount,
        itemSize,
        bucketCount
    );

    CONSTEXPRINLINE const char& operator[] (const int idx) const { return data[idx]; }
    CONSTEXPRINLINE       char& operator[] (const int idx) { return data[idx]; }

    char data[byteSize] {};
};

template<typename T,
         uint32_t itemCount,
         uint32_t bucketCount=pickBucketCount(itemCount)>
using TypedFixedStorage = FixedStorage<itemCount, sizeof(T), bucketCount>;


struct DefaultMapAdapter
{
    CONSTEXPRINLINE auto getKey(const auto iter) const { return iter->first; }
    CONSTEXPRINLINE auto getValue(const auto iter) const { return iter->second; }
};

template<typename T>
struct DefaultValueAdapter
{
    T value;
    CONSTEXPRINLINE auto getKey(const auto iter) const { return *iter; }
    CONSTEXPRINLINE auto getValue(const auto iter) const { return value; }
};

}  // namespace fhmbuilding


namespace fhmio
{

namespace detail
{

template<typename T>
constexpr char swcw_getbyte()
{
    T value {};
    struct Tmp { char buf[sizeof(T)]; } tmp {};
    tmp = BIT_CAST(Tmp, value);
    return tmp.buf[0];
}
template<typename T, char=swcw_getbyte<T>()> struct scwc_fetch {};

template<typename T, typename=std::void_t<>>
struct supported_copy_with_constexpr : std::false_type {};

template<typename T>
struct supported_copy_with_constexpr<T, std::void_t<scwc_fetch<T>>> : std::true_type {};

template<typename T>
CONSTEXPRINLINE bool validateTypeInConstexpr()
{
    if(IS_CONSTANT_EVALUATED())
    {
        if constexpr(!supported_copy_with_constexpr<T>::value)
        {
           // Using a static_assert will often evaluate prematurely, so we opt to throw an exception here
            // in its place (which can only happen at compile time).
            throw "This type cannot be converted in a constexpr context, this is likely due to implicit padding.";
            return false;
        }
    }
    return true;
}


}  // namespace detail

// Helper for storing a object within a character sequence.
template<typename T, typename Byte>
CONSTEXPRINLINE void storeObject(Byte* data, const T& input)
{
    if(IS_CONSTANT_EVALUATED())
    {
        struct Tmp { std::remove_cv_t<Byte> data[sizeof(T)]; } tmp {};
        tmp = BIT_CAST(Tmp, input);
        for(uint32_t i=0; i<sizeof(input); ++i)
        {
            data[i] = tmp.data[i];
        }
    }
    else
    {
        std::memcpy(data, &input, sizeof(T));
    }
}

// Helpers for loading a object from a character sequence.
template<typename T, typename Byte>
CONSTEXPRINLINE void loadObject(Byte* data, T& output)
{
    if(IS_CONSTANT_EVALUATED())
    {
        struct Tmp { std::remove_cv_t<Byte> blob[sizeof(T)]; } tmp {};
        for(size_t i=0 ; i<sizeof(T); ++i) { tmp.blob[i] = data[i]; }
        output = BIT_CAST(T, tmp);
    }
    else
    {
        std::memcpy(&output, data, sizeof(T));
    }
}

template<typename T, typename Byte>
CONSTEXPRINLINE T loadObject(Byte* data)
{
    T output;
    loadObject(data, output);
    return output;
}

template<size_t size, typename Byte>
CONSTEXPRINLINE void copyBytes(Byte* dst, const Byte* src)
{
    if(IS_CONSTANT_EVALUATED())
    {
        for(size_t i=0 ; i<size; ++i) { dst[i] = src[i]; }
    }
    else
    {
        std::memcpy(dst, src, size);
    }
}

template<typename T, typename Byte>
CONSTEXPRINLINE void streamInObject(Byte*& data, T& output)
{
    loadObject(data, output);
    data += sizeof(T);
}

template<typename T, typename Byte>
CONSTEXPRINLINE T streamInObject(Byte*& data)
{
    T output;
    streamInObject(data, output);
    return output;
}


template<typename Byte>
CONSTEXPRINLINE bool hasKey(Byte* root,
                            const uint64_t hash)
{
    Byte* data = root;

    const FhmMapHeader mapHeader = streamInObject<FhmMapHeader>(data);
    const uint64_t bucketCount = mapHeader.bucketCount;

    // Select the correct bucket and fetch its header
    const uint64_t bucketId = (hash & (bucketCount - 1));
    data += sizeof(FhmBucketHeader) * bucketId;
    const FhmBucketHeader header = loadObject<FhmBucketHeader>(data);
    data = root + header.offset;

    // Linear search for the correct key
    uint32_t keyId = 0;
    for(; keyId<header.count && (streamInObject<uint64_t>(data) != hash); ++keyId);

    return (keyId < header.count);
}

template<typename Byte>
CONSTEXPRINLINE uint32_t getBucketCount(Byte* root)
{
    return loadObject<FhmMapHeader>(root).bucketCount;
}

template<typename Byte>
CONSTEXPRINLINE uint32_t getEntryCount(Byte* root)
{
    return loadObject<FhmMapHeader>(root).entryCount;
}

template<typename Value, typename Byte>
CONSTEXPRINLINE uint64_t byteSize(Byte* root)
{
    Byte* data = root;

    const FhmMapHeader mapHeader = streamInObject<FhmMapHeader>(data);
    const uint64_t bucketCount = mapHeader.bucketCount;
    const uint64_t entryCount = mapHeader.entryCount;

    return fhmbuilding::calculateFixedHashMapSize(
        entryCount,
        sizeof(Value),
        bucketCount
    );
}


// Main routine that deals with finding a key, so a value may subsequently
// be fetched or stored
template<typename Value, typename Byte>
CONSTEXPRINLINE Byte* getAddressImpl(Byte* root, const uint64_t hash)
{
    if(!detail::validateTypeInConstexpr<Value>()) { return 0; }
    Byte* data = root;

    const FhmMapHeader mapHeader = streamInObject<FhmMapHeader>(data);
    const uint64_t bucketCount = mapHeader.bucketCount;

    // Select the correct bucket and fetch its header
    const uint64_t bucketId = (hash & (bucketCount - 1));
    data += sizeof(FhmBucketHeader) * bucketId;
    const FhmBucketHeader header = loadObject<FhmBucketHeader>(data);
    data = root + header.offset;

    // Linear search for the correct key
    uint32_t keyId = 0;
    for(; keyId<header.count && (streamInObject<uint64_t>(data) != hash); ++keyId);

    // Assume that generally people aren't using this to test if a key exists
    IF_UNLIKELY(keyId >= header.count)
    {
        return 0;
    }

    // Go to the data block
    data += (header.count - keyId - 1) * sizeof(FhmBucketHeader)
         + sizeof(Value) * keyId;

    return data;
}


// mode 0 = get
// mode 1 = set
// mode 2 = swap
template<int mode, typename Value, typename Byte>
CONSTEXPRINLINE bool getSetImpl(Byte* root,
                                const uint64_t hash,
                                Value& inputOutput)
{
    constexpr bool setting = mode >= 1;
    static_assert(!(setting && std::is_const_v<Byte>), "Cannot set a value, on a non-writeable map!");
    if(!detail::validateTypeInConstexpr<Value>()) { return false; }
    Byte* data = getAddressImpl<Value, Byte>(root, hash);

    if(!data) { return false; }

    // Get or set the value depending on what op we're doing
    if constexpr(setting)
    {
        // Swap
        if constexpr (mode == 2)
        {
            Byte tmp[sizeof(Value)] {};
            storeObject(tmp, inputOutput);
            loadObject(data, inputOutput);
            copyBytes<sizeof(Value)>(data, &tmp[0]);
        }

        // Set
        else
        {
            storeObject(data, inputOutput);
        }
    }
    else
    {
        loadObject(data, inputOutput);
    }

    return true;
}


} // namespace fhmio


namespace fhmiter
{

template<typename T, typename CharPointer>
struct NodeItIndirection
{
    const static bool readOnly = std::is_const_v<CharPointer>;

    CONSTEXPRINLINE operator T () const
    {
        return fhmio::loadObject<T>(offset);
    }

    FORCEINLINE const T* getPtr() const
    {
        return (const T*)offset;
    }

    template<typename=std::enable_if_t<!readOnly>>
    CONSTEXPRINLINE NodeItIndirection& operator= (const T& value)
    {
        fhmio::storeObject(offset, value);
        return *this;
    }

    CharPointer     offset {};
};


template<typename FixedHashMapT>
struct Node
{
    using Value                         = typename FixedHashMapT::Value;
    using NativeCharPointer             = typename FixedHashMapT::CharPointer;
    using ConstCharPointer              = typename FixedHashMapT::ConstCharPointer;
    const static bool readOnly          = std::is_const_v<FixedHashMapT>;
    using CharPointer = std::conditional_t<readOnly, ConstCharPointer, NativeCharPointer>;

    CONSTEXPRINLINE operator std::pair<uint64_t, Value> () const
    {
        return {first, second};
    }

    NodeItIndirection<uint64_t, ConstCharPointer>   first   {};  // key
    NodeItIndirection<Value, CharPointer>           second  {};  // value
};

template<typename FixedHashMapT>
struct Iterator
{
    using Value       = typename FixedHashMapT::Value;
    using CharPointer = std::conditional_t<
        std::is_const_v<FixedHashMapT>,
        typename FixedHashMapT::ConstCharPointer,
        typename FixedHashMapT::CharPointer
    >;


    const Node<FixedHashMapT>& operator *() const  { return node; }
    const Node<FixedHashMapT>* operator ->() const { return &node; }
    Node<FixedHashMapT>& operator *()  { return node; }
    Node<FixedHashMapT>* operator ->() { return &node; }

    CONSTEXPRINLINE bool operator == (const Iterator& other) const { return iteratorIndex == other.iteratorIndex && root == other.root; }
    CONSTEXPRINLINE bool operator != (const Iterator& other) const { return !(iteratorIndex == other.iteratorIndex); }
    CONSTEXPRINLINE bool operator <= (const Iterator& other) const { return iteratorIndex <= other.iteratorIndex; }
    CONSTEXPRINLINE bool operator >= (const Iterator& other) const { return iteratorIndex >= other.iteratorIndex; }
    CONSTEXPRINLINE bool operator <  (const Iterator& other) const { return iteratorIndex < other.iteratorIndex; }
    CONSTEXPRINLINE bool operator >  (const Iterator& other) const { return iteratorIndex > other.iteratorIndex; }

    CONSTEXPRINLINE intptr_t operator-(const Iterator& other) const
    {
        return intptr_t(iteratorIndex) - intptr_t(other.iteratorIndex);
    }

    CONSTEXPRINLINE Iterator& operator++()
    {
        ++iteratorIndex;

        // Go to the next bucket if we're at the end
        if(++itemId >= header.count)
        {
            itemId = 0;
            ++bucketId;
            const uint32_t bucketCount = fhmio::getBucketCount(root);
            CharPointer data = root + sizeof(FhmMapHeader)
                                    + sizeof(FhmBucketHeader) * bucketId;

            for(; bucketId < bucketCount; ++bucketId)
            {
                header = fhmio::streamInObject<FhmBucketHeader>(data);
                if(header.count > 0)
                {
                    break;
                }
            }
        }

        init();
        return *this;
    }

    void init()
    {
        node.first.offset = root
                            + header.offset
                            + sizeof(uint64_t) * itemId;
        node.second.offset = root
                            + header.offset
                            + sizeof(uint64_t) * header.count
                            + sizeof(Value) * itemId;
    }

    Node<FixedHashMapT>     node {};

    CharPointer             root = nullptr;
    FhmBucketHeader         header {};
    uint32_t                bucketId = 0;
    uint32_t                itemId = 0;
    uint32_t                iteratorIndex = 0;
};


template<typename IteratorT, typename Byte>
CONSTEXPRINLINE IteratorT createBegin(Byte* storage)
{
    Byte* data = &storage[0];
    const FhmMapHeader mapHeader = fhmio::streamInObject<FhmMapHeader>(data);

    IteratorT it {};
    it.root = storage;
    it.iteratorIndex = 0;
    it.itemId = 0;

    // Find the first valid bucket
    for(it.bucketId=0; it.bucketId<mapHeader.bucketCount; ++it.bucketId)
    {
        it.header = fhmio::streamInObject<FhmBucketHeader>(data);
        if(it.header.count > 0)
        {
            break;
        }
    }

    it.init();
    return it;
}

template<typename IteratorT, typename Byte>
CONSTEXPRINLINE IteratorT createEnd(Byte* storage)
{
    Byte* data = &storage[0];
    const FhmMapHeader mapHeader = fhmio::streamInObject<FhmMapHeader>(data);

    IteratorT it {};
    it.root = storage;
    it.iteratorIndex = mapHeader.entryCount;
    it.itemId = 0;
    it.bucketId = mapHeader.bucketCount;

    return it;
}


}  // namespace fhmiter


template<typename T, typename Storage=const char*>
struct FixedHashMap
{
    using Value = T;
    static_assert(!std::is_const_v<T>, "Value type cannot be const!");
    static_assert(!std::is_volatile_v<T>, "Value type cannot be volatile!");

    // Long-winded way to get the char type while removing references etc
    using RawAccessType = decltype(std::declval<Storage>()[0]);
    const static bool readOnly = std::is_const_v<RawAccessType>;

    using CharType = std::conditional_t<readOnly, const std::decay_t<RawAccessType>, std::decay_t<RawAccessType>>;
    using CharPointer = CharType*;
    using ConstCharPointer = const std::decay_t<RawAccessType>*;

    static_assert(sizeof(CharType) == 1, "Character type must be a size of 1!");

    using iterator       = fhmiter::Iterator<FixedHashMap>;
    using const_iterator = fhmiter::Iterator<const FixedHashMap>;

    // Only enable setting if the storage pointer isn't read only
    template<typename Dummy=void, typename=std::enable_if_t<std::is_same_v<Dummy, void> && !readOnly>>
    CONSTEXPRINLINE bool swap(const uint64_t key, Value& input)
    {
        return fhmio::getSetImpl<2>( &storage[0], key, input );
    }

    template<typename Dummy=void, typename=std::enable_if_t<std::is_same_v<Dummy, void> && !readOnly>>
    CONSTEXPRINLINE bool set(const uint64_t key, const Value& input)
    {
        return fhmio::getSetImpl<1>( &storage[0], key, input );
    }

    CONSTEXPRINLINE bool get(const uint64_t key, Value& output) const
    {
        return fhmio::getSetImpl<0>( &storage[0], key, output );
    }

    CONSTEXPRINLINE uint64_t getRawOffset(const uint64_t key) const
    {
        return (fhmio::getAddressImpl<Value>( &storage[0], key ) - (&storage[0]));
    }

    // NB: Cannot be constexpr due to casting
    template<typename Dummy=void, typename=std::enable_if_t<std::is_same_v<Dummy, void> && !readOnly>>
    FORCEINLINE Value* getRawPtr(const uint64_t key)
    {
        return (Value*)fhmio::getAddressImpl<Value>( &storage[0], key );
    }

    FORCEINLINE const Value* getRawPtr(const uint64_t key) const
    {
        return (const  Value*)fhmio::getAddressImpl<Value>( &storage[0], key );
    }

    CONSTEXPRINLINE bool hasKey(const uint64_t key) const
    {
        return fhmio::hasKey( &storage[0], key );
    }

    CONSTEXPRINLINE size_t size() const
    {
        return fhmio::getEntryCount( &storage[0] );
    }

    CONSTEXPRINLINE size_t bucketCount() const
    {
        return fhmio::getBucketCount( &storage[0] );
    }

    CONSTEXPRINLINE size_t byteSize() const
    {
        return fhmio::byteSize<Value>( &storage[0] );
    }

    CONSTEXPRINLINE ConstCharPointer data() const { return &storage[0]; }
    CONSTEXPRINLINE CharPointer      data()       { return &storage[0]; }

    // Always be able to up-const cast to a raw version of the storage pointer (via a lot of template indirection)
    template<
        typename ConstCharPointerT=ConstCharPointer,
        typename=std::enable_if_t<
            std::is_same_v<ConstCharPointerT, ConstCharPointer>
            && !std::is_same_v<CharPointer, ConstCharPointer>
        >
    >
    CONSTEXPRINLINE operator FixedHashMap<Value, ConstCharPointerT> () const
    {
        return { &storage[0] };
    }

    CONSTEXPRINLINE const_iterator begin() const { return fhmiter::createBegin<const_iterator>( &storage[0] ); }
    CONSTEXPRINLINE iterator begin() { return fhmiter::createBegin<iterator>( &storage[0] ); }
    CONSTEXPRINLINE const_iterator end() const { return fhmiter::createEnd<const_iterator>( &storage[0] ); }
    CONSTEXPRINLINE iterator end() { return fhmiter::createEnd<iterator>( &storage[0] ); }

    // Always be able to cast to a raw version of the storage pointer (via a lot of template indirection)
    template<
        typename CharPointerT=CharPointer,
        typename=std::enable_if_t<
            std::is_same_v<CharPointerT, CharPointer>
            && !std::is_same_v<Storage, CharPointer>
            && !std::is_same_v<CharPointer, ConstCharPointer>
        >
    >
    CONSTEXPRINLINE operator FixedHashMap<Value, CharPointerT> ()
    {
        return { &storage[0] };
    }

    Storage storage;
};


// This creates a fixed hash map where-by the layout of the map itself has been determined
// but no hashes or values are actually written.
template<typename T, typename... HashType>
CONSTEXPRINLINE FixedHashMap<T, fhmbuilding::TypedFixedStorage<T, sizeof...(HashType)>> createEmptyFixedHashMapFromHashes(const HashType&... hashes)
{
    using Storage = fhmbuilding::TypedFixedStorage<T, sizeof...(hashes)>;
    using MapT = FixedHashMap<T, Storage>;

    constexpr uint32_t bucketCount = Storage::bucketCount;

    FhmMapHeader mapHeader {};
    mapHeader.entryCount = sizeof...(hashes);
    mapHeader.bucketCount = bucketCount;

    // Calculate the count each bucket is required to be.
    struct Headers
    {
        FhmBucketHeader data[bucketCount];
        CONSTEXPRINLINE FhmBucketHeader& operator[] (uint32_t i) { return data[i]; }
    
    } bucketHeaders {};

    {
        auto incrementBucketCount = [&](const auto& entry)
        {
            ++bucketHeaders[entry & (bucketCount - 1)].count;
        };

        using Dummy = int[];
        (void)Dummy{( (void)incrementBucketCount(hashes), 0 )...};
    }

    // Determine the offset of each bucket given the counts
    {
        uint32_t offset = sizeof(FhmMapHeader) + sizeof(FhmBucketHeader) * bucketCount;
        constexpr uint32_t perEntrySize = (sizeof(uint64_t) + sizeof(T));
        for(uint64_t i=0; i<bucketCount; ++i)
        {
            bucketHeaders[i].offset = offset;
            offset += perEntrySize * bucketHeaders[i].count;
        }
    }

    MapT result;

    fhmio::storeObject(&result.storage[0],                    mapHeader);
    fhmio::storeObject(&result.storage[sizeof(FhmMapHeader)], bucketHeaders);

    return result;
}


// This creates a fixed hash map with every value set to specified defaultValue
template<typename T, typename... HashType>
CONSTEXPRINLINE FixedHashMap<T, fhmbuilding::TypedFixedStorage<T, sizeof...(HashType)>> createFixedHashMapWithDefaultValue(const T& defaultValue, const HashType&... hashes)
{
    auto result = createEmptyFixedHashMapFromHashes<T>(hashes...);

    constexpr uint32_t bucketCount = decltype(result.storage)::bucketCount;

    struct loader_ { FhmBucketHeader bucketHeaders[bucketCount]; } bucketHeadersLoader {};
    fhmio::loadObject(&result.storage[sizeof(FhmMapHeader)], bucketHeadersLoader);
    FhmBucketHeader* bucketHeaders = &bucketHeadersLoader.bucketHeaders[0];

    {
        uint64_t writtenEntries[bucketCount] {};
        auto writeEntry = [&](const uint64_t hash)
        {
            const uint64_t bucketId = hash & (bucketCount - 1);
            const uint64_t hashWriteOffset = (
                bucketHeaders[bucketId].offset
                + sizeof(uint64_t) * writtenEntries[bucketId]
            );
            const uint64_t valueWriteOffset = (
                bucketHeaders[bucketId].offset
                + sizeof(uint64_t) * bucketHeaders[bucketId].count
                + sizeof(T) * writtenEntries[bucketId]
            );

            fhmio::storeObject(&result.storage[hashWriteOffset],  hash);
            fhmio::storeObject(&result.storage[valueWriteOffset], defaultValue);
            ++writtenEntries[bucketId];
        };

        using Dummy = int[];
        (void)Dummy{((void)writeEntry(hashes), 0)...};
    }

    return result;

}

// This creates a fixed hash map from hash-value pairs
template<typename T, typename... PairTypes>
CONSTEXPRINLINE FixedHashMap<T, fhmbuilding::TypedFixedStorage<T, sizeof...(PairTypes)>>
createFixedHashMapFromPairs(const PairTypes&... pairs)
{
    auto result = createEmptyFixedHashMapFromHashes<T>(pairs.first...);

    constexpr uint32_t bucketCount = decltype(result.storage)::bucketCount;

    struct loader_ { FhmBucketHeader bucketHeaders[bucketCount]; } bucketHeadersLoader {};
    fhmio::loadObject(&result.storage[sizeof(FhmMapHeader)], bucketHeadersLoader);
    FhmBucketHeader* bucketHeaders = &bucketHeadersLoader.bucketHeaders[0];

    {
        uint64_t writtenEntries[bucketCount] {};
        auto writeEntry = [&](const auto& entry)
        {
            const uint64_t hash = entry.first;
            const uint64_t bucketId = hash & (bucketCount - 1);

            const uint64_t hashWriteOffset = (
                bucketHeaders[bucketId].offset
                + sizeof(uint64_t) * writtenEntries[bucketId]
            );
            const uint64_t valueWriteOffset = (
                bucketHeaders[bucketId].offset
                + sizeof(uint64_t) * bucketHeaders[bucketId].count
                + sizeof(T) * writtenEntries[bucketId]
            );

            fhmio::storeObject(&result.storage[hashWriteOffset],  hash);
            fhmio::storeObject(&result.storage[valueWriteOffset], entry.second);
            ++writtenEntries[bucketId];
        };

        using Dummy = int[];
        (void)Dummy{((void)writeEntry(pairs), 0)...};
    }

    return result;
}


template<typename T, size_t itemCount>
CONSTEXPRINLINE FixedHashMap<T, fhmbuilding::TypedFixedStorage<T, itemCount>> createFixedHashMap(const std::pair<uint64_t, T> (&pairs)[itemCount])
{
    return fhmbuilding::unpackArray<0, itemCount>(
        [](auto&&... args)
        {
            return createFixedHashMapFromPairs<T>(std::forward<decltype(args)>(args)...);
        },
        &pairs[0]
    );
}


template<typename T>
CONSTEXPRINLINE FixedHashMap<T, fhmbuilding::TypedFixedStorage<T, 0>> createFixedHashMap()
{
    return createEmptyFixedHashMapFromHashes<T>();
}


// Runtime specific hash-map generation, where a fixed data size cannot be used
template<typename T, uint32_t maxStackProtection=4096, typename Adapter=fhmbuilding::DefaultMapAdapter, typename IteratorType=void, typename AllocateFuncType=void>
CONSTEXPRINLINE auto createFixedHashMap(IteratorType begin, IteratorType end, AllocateFuncType&& allocateFunc, Adapter adapter={})
{
    using Storage = decltype(std::declval<decltype(allocateFunc)>()(1));
    const uint32_t itemCount = std::distance(begin, end);
    const uint32_t bucketCount = fhmbuilding::pickBucketCount(itemCount);
    const uint32_t itemSize = sizeof(T);
    const size_t mapByteSize = fhmbuilding::calculateFixedHashMapSize(itemCount, itemSize, bucketCount);

    FixedHashMap<T, Storage> result { allocateFunc(mapByteSize) };
    using Byte = std::decay_t<decltype(result.storage[0])>;

    FhmMapHeader mapHeader;
    mapHeader.entryCount = itemCount;
    mapHeader.bucketCount = bucketCount;
    fhmio::storeObject(&result.storage[0], mapHeader);

    // We're going to use the allocated storage as scratch space
    // this the number of buckets isn't static, and dynamic allocation
    // is a bit sad.
    // Calculate the bucket counts
    for(auto iter=begin; iter!=end; ++iter)
    {
        const uint64_t hash = adapter.getKey(iter);
        const uint64_t bucketId = hash & (bucketCount - 1);

        Byte* bucketOffset = &result.storage[
            sizeof(FhmMapHeader)
            + sizeof(FhmBucketHeader) * bucketId
        ];

        FhmBucketHeader header = fhmio::loadObject<FhmBucketHeader>(bucketOffset);
        ++header.count;
        fhmio::storeObject(bucketOffset, header);
    }

    // Determine the offsets given the counts
    {
        uint32_t offset = sizeof(FhmMapHeader) + sizeof(FhmBucketHeader) * bucketCount;
        for(uint64_t i=0; i<bucketCount; ++i)
        {
            Byte* bucketOffset = &result.storage[
                sizeof(FhmMapHeader)
                + sizeof(FhmBucketHeader) * i
            ];
            FhmBucketHeader header = fhmio::loadObject<FhmBucketHeader>(bucketOffset);
            header.offset = offset;
            offset += (sizeof(uint64_t) + sizeof(T)) * header.count;
            fhmio::storeObject(bucketOffset, header);
        }
    }

    // Start writing the data!
    // Unfortunatley, it's not really possible to avoid needing another
    // array for counting written entries, GCC and Clang are fine with
    // dynamic stack sizes, but MSVC complains, so we're having to use
    // a fixed upper bound or invoke new[] / delete[]
    uint32_t writtenEntriesStack[maxStackProtection] {};
    uint32_t* writtenEntries = &writtenEntriesStack[0];

    if (maxStackProtection <= bucketCount)
    {
        writtenEntries = new uint32_t[bucketCount];
    }
    else
    {
        writtenEntries = &writtenEntriesStack[0];
    }

    for(uint32_t i=0; i<bucketCount; ++i)
    {
        writtenEntries[i] = 0;
    }
    for(auto iter=begin; iter!=end; ++iter)
    {
        const uint64_t hash = adapter.getKey(iter);
        const uint64_t bucketId = hash & (bucketCount - 1);
        Byte* bucketOffset = &result.storage[
            sizeof(FhmMapHeader)
            + sizeof(FhmBucketHeader) * bucketId
        ];
        FhmBucketHeader header = fhmio::loadObject<FhmBucketHeader>(bucketOffset);

        const uint32_t hashWriteOffset = (
            header.offset
            + sizeof(uint64_t) * writtenEntries[bucketId]
        );
        const uint64_t valueWriteOffset = (
            header.offset
            + sizeof(uint64_t) * header.count
            + sizeof(T) * writtenEntries[bucketId]
        );

        fhmio::storeObject(&result.storage[hashWriteOffset],  hash);
        fhmio::storeObject(&result.storage[valueWriteOffset], adapter.getValue(iter));
        ++writtenEntries[bucketId];
    }

    if (maxStackProtection <= bucketCount)
    {
        delete[] writtenEntries;
    }

    return result;
}

template<typename T, uint32_t maxStackProtection=4096, typename IteratorType=void>
constexpr FixedHashMap<T, std::unique_ptr<char[]>> createFixedHashMap(IteratorType begin,
                                                                      IteratorType end)
{
    return std::move(
        createFixedHashMap<T, maxStackProtection>(
            begin,
            end,
            [](size_t bytes){ return std::unique_ptr<char[]>(new char[bytes]); }
        )
    );
}

template<typename T, uint32_t maxStackProtection=4096, typename IteratorType=void>
constexpr FixedHashMap<T, std::unique_ptr<char[]>> createEmptyFixedHashMap(IteratorType begin,
                                                                            IteratorType end,
                                                                            const T& defaultValue = {})
{
    fhmbuilding::DefaultValueAdapter<T> adapter { defaultValue };
    return std::move(
        createFixedHashMap<T, maxStackProtection>(
            begin,
            end,
            [](size_t bytes){ return std::unique_ptr<char[]>(new char[bytes]); },
            adapter
        )
    );
}
