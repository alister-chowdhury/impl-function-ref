// NB:
// I havne't tested this works yet
// I haven't benchmarked this yet


#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <typeindex>
#include <utility>


struct udim2f {

    union { float x; float u; };
    union { float y; float v; };


    uint64_t packed() const {
        uint64_t value;
        std::memcpy(&value, this, sizeof(*this));
        return value;
    }


    bool operator==(const udim2f other) const {
        return packed() == other.packed();
    }
    bool operator!=(const udim2f other) const {
        return packed() != other.packed();
    }

};

template <> struct std::hash<udim2f> {
    std::size_t operator()(const udim2f t) const {
        return t.packed();
    }
};


class udim2f_set {
    const static size_t max_local_capacity = 2;
public:

    using iterator = udim2f*;
    using const_iterator = const udim2f*;

    udim2f_set()
    : m_begin((udim2f*)&m_stack.data),
      m_end((udim2f*)&m_stack.data)
    {}

    udim2f_set(const std::initializer_list<udim2f>& init) : udim2f_set() {
        for(const udim2f x : init) { insert(x); }
    }

    ~udim2f_set() {
        clear();
    }

    udim2f_set(const udim2f_set& other) {
        if(other.m_begin == (const udim2f*)&other.m_stack.data) {
            std::memcpy(this, &other, sizeof(*this));
            m_begin = (udim2f*)&m_stack.data;
            m_end = m_begin + other.size();
            return;
        }
        m_begin = (udim2f*)std::malloc((sizeof(udim2f)*other.m_heap.capacity));
        m_end = m_begin + other.size();
        std::memcpy(m_begin, other.m_begin, other.size()*sizeof(udim2f));
    }

    udim2f_set(udim2f_set&& other) {
        std::memcpy(this, &other, sizeof(*this));
        if(other.m_begin == (const udim2f*)&other.m_stack.data) {
            m_begin = (udim2f*)&m_stack.data;
            m_end = m_begin + size();
        }

        other.m_begin = (udim2f*)&other.m_stack.data;
        other.m_end = (udim2f*)&other.m_stack.data;
    }

    iterator begin() { return m_begin; }
    iterator end()   { return m_end; }

    const_iterator begin() const { return m_begin; }
    const_iterator end()   const { return m_end; }

    iterator find(const udim2f needle) { return search(needle); }
    const_iterator find(const udim2f needle) const { return search(needle); }

    size_t size() const { return m_end - m_begin; }
    bool empty() const { return size() == 0; }
    void clear() {
        if(m_begin != (udim2f*)&m_stack.data) {
            std::free(m_begin);
        }
        m_begin = (udim2f*)&m_stack.data;
        m_end = (udim2f*)&m_stack.data;
    }

    template<typename Iterator>
    void insert(Iterator begin, Iterator end) {
        for(Iterator iter = begin; iter < end; ++iter) {
            insert(*iter);
        }
    }

    iterator insert(const udim2f value) {
        iterator found = lower_bounds(value);

        // Already in set
        if(found < m_end) {
            if(*found == value) {
                return found;
            }
        }

        // Need to insert
        const size_t current_size = m_end - m_begin;
        const bool on_stack = m_begin == (udim2f*)&m_stack.data;
        const size_t capacity = (on_stack ? max_local_capacity : m_heap.capacity);

        // Simple move, no realloc needed.
        if(capacity > current_size) {
            std::memmove(found, found+1, sizeof(udim2f)*(m_end - found));
            *found = value;
            ++m_end;
            return found;
        }

        // Realloc required
        const size_t new_value_index = found - m_begin;
        const size_t new_capacity = capacity * 2;
        udim2f* new_ptr;

        if(on_stack) {
            new_ptr = (udim2f*)std::malloc(sizeof(udim2f)*new_capacity);
            std::memcpy(new_ptr, m_begin, max_local_capacity);
        }
        else {
            new_ptr = (udim2f*)std::realloc(m_begin, sizeof(udim2f)*new_capacity);
        }

        const iterator new_found = new_ptr + (found - m_begin);
        std::memmove(new_found, new_found+1, sizeof(udim2f)*(m_end - found));
        *new_found = value;

        m_begin = new_ptr;
        m_end = new_ptr + current_size + 1;
        m_heap.capacity = new_capacity;

        return new_found;

    }

private:
    iterator lower_bounds(const udim2f needle) const {
        return std::lower_bound(
            m_begin,
            m_end,
            needle,
            [](const udim2f a, const udim2f b){return a.packed() < b.packed();}
        );
    }
    iterator search(const udim2f needle) const {
        const iterator found = lower_bounds(needle);
        if(found < m_end) {
            if(*found == needle) {
                return found;
            }
        }
        return m_end;
    }

    udim2f*     m_begin;
    udim2f*     m_end;
    union {
        struct {
            udim2f  data[max_local_capacity];
        } m_stack;
        struct {
            size_t  capacity;
        } m_heap;
    };

};
