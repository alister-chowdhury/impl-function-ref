
#if defined(_MSC_VER) && defined(_M_X64)
    #include <intrin.h>

#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    #include <x86intrin.h>

#else
    #error "Only really intended for x64 gcc/clang/msc"
#endif


#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>


struct vec2 {
    float x, y;
};


inline
uint64_t uv_to_packed_udim(const vec2& uv) {
    __m128 udim = _mm_floor_ps(_mm_castpd_ps(_mm_load_sd((const double*)&uv)));
    return (uint64_t)_mm_cvtsi128_si64(_mm_castps_si128(udim));
}

inline
vec2 uv_to_udim(const vec2& uv) {
    const uint64_t  udim_v = uv_to_packed_udim(uv);
    vec2 udim;
    std::memcpy(&udim, &udim_v, sizeof(vec2));
    return udim;
}



struct small_udim_2f_set {

    const static uint32_t     max_local = 6;    // Maximum of 6 udims
                                                // to be stored locally
                                                // before moving to the heap
                                                // (Making this struct 64 bytes)

    small_udim_2f_set() noexcept = default;
    ~small_udim_2f_set() noexcept { clear(); }
    small_udim_2f_set(const small_udim_2f_set&) = delete;

    small_udim_2f_set(small_udim_2f_set&& other) {
        std::memcpy(this, &other, sizeof(*this));

        if(other.m_ptr == (uint64_t*)&other.m_local) {
            m_ptr = (uint64_t*)&m_local;
        }

        other.m_ptr = (uint64_t*)&other.m_local;
        other.m_size = 0;
        other.m_capacity = max_local;
    }


    struct iterator {

        iterator(const iterator& other) = default;
        iterator(iterator&& other) = default;
        iterator& operator=(const iterator& other) = default;

        bool operator==(const iterator& other) const noexcept { return m_ptr == other.m_ptr; }
        bool operator!=(const iterator& other) const noexcept { return m_ptr != other.m_ptr; }
        bool operator<=(const iterator& other) const noexcept { return m_ptr <= other.m_ptr; }
        bool operator>=(const iterator& other) const noexcept { return m_ptr >= other.m_ptr; }
        bool operator<(const iterator& other) const noexcept { return m_ptr < other.m_ptr; }
        bool operator>(const iterator& other) const noexcept { return m_ptr > other.m_ptr; }

        iterator& operator++() noexcept { ++m_ptr; return *this; }
        iterator& operator--() noexcept { --m_ptr; return *this; }
        iterator& operator+(const int n) noexcept { m_ptr += n; return *this; }
        iterator& operator-(const int n) noexcept { m_ptr -= n; return *this; }

        vec2 operator* () const noexcept {
            vec2 udim;
            std::memcpy(&udim, m_ptr, sizeof(vec2));
            return udim;
        }

    protected:
        friend small_udim_2f_set;
        iterator(const uint64_t* ptr) : m_ptr(ptr) {}

    private:
        const uint64_t*   m_ptr;
    };


    void insert_uv_range(const vec2& start, const vec2& end) noexcept {

        vec2  udim_start = uv_to_udim(start);
        vec2  udim_end   = uv_to_udim(end);

        // Secretly just a udim
        if(udim_start.x == udim_end.x && udim_start.y == udim_end.y) {
            insert_udim(udim_start);
        }

        // Actual range
        else {
            for(float y=udim_start.y; y <= udim_end.y; y+=1.0f) {
                for(float x=udim_start.x; x <= udim_end.x; x+=1.0f) {
                    insert_udim(vec2{x, y});
                }
            }
        }

    }

    void insert(const small_udim_2f_set& other) noexcept {
        for(const vec2 v : other) { insert_udim(v); }
    }

    void insert_uv(const vec2& v) noexcept {
        const uint64_t  udim = uv_to_packed_udim(v);
        insert_udim(udim);
    }

    void insert_udim(const vec2& v) noexcept {
        uint64_t  udim;
        std::memcpy(&udim, &v, sizeof(uint64_t));
        insert_udim(udim);
    }

    void insert_udim(const uint64_t v) noexcept {
        uint64_t* found = lower_bound(v);
        uint64_t* rend = raw_end();

        // Already accounted for
        if(found != rend) {
            if(*found == v) { return; }
        }

        const uint32_t found_dist = found - m_ptr;

        // We need to do a reallocation, while also making sure
        // to repositioning the pointers
        if(m_size == m_capacity) {
            const uint32_t  new_capacity = m_capacity * 2;  // double the capacity

            if(m_ptr != (uint64_t*)&m_local) {
                m_ptr = (uint64_t*)std::realloc(m_ptr, new_capacity * sizeof(uint64_t));
            }
            else {
                uint64_t* new_ptr = (uint64_t*)std::malloc(new_capacity * sizeof(uint64_t));
                std::memcpy(new_ptr, m_ptr, m_size * sizeof(uint64_t));
                m_ptr = new_ptr;
            }

            m_capacity = new_capacity;
            found = m_ptr + found_dist;
            rend = m_ptr + m_size;
        }

        // Move everything after the found address one position ahead
        // Put v where found was and increment the size
        if(found != rend) {
            std::memmove(found+1, found, (m_size - found_dist) * sizeof(uint64_t));
        }
        *found = v;
        ++m_size;

    }

    void clear() noexcept {
        if(m_ptr != (uint64_t*)&m_local) {
            std::free(m_ptr);
            m_size = 0;
            m_capacity = max_local;
            m_ptr = (uint64_t*)&m_local;
        }
    }


    uint32_t size() const noexcept { return m_size; }

    iterator begin() const noexcept { return raw_begin(); }
    iterator end() const noexcept { return raw_end(); }

    iterator find(const vec2& v) const noexcept {
        uint64_t  udim;
        std::memcpy(&udim, &v, sizeof(uint64_t));
        return find(udim);
    }

    iterator find(const uint64_t v) const {
        const uint64_t* found = lower_bound(v);
        const uint64_t* rend = raw_end();
        if(found != rend) {
            if(*found != v) {
                found = rend;
            }
        }
        return found;
    }


private:
        uint32_t lower_bound_linear(const uint64_t udim) const noexcept {
        const uint32_t size = m_size;
        for(uint32_t i=0; i<size; ++i) {
            if(m_ptr[i] >= udim) { return i; }
        }
        return size;
    }
    uint64_t* lower_bound(const uint64_t udim) noexcept {
        if(m_size <= 8) { return m_ptr + lower_bound_linear(udim); }
        return std::lower_bound(m_ptr, m_ptr + m_size, udim);
    }
    const uint64_t* lower_bound(const uint64_t udim) const noexcept {
        if(m_size <= 8) { return m_ptr + lower_bound_linear(udim); }
        return std::lower_bound(m_ptr, m_ptr + m_size, udim);
    }

    uint64_t* raw_begin() noexcept { return m_ptr; }
    uint64_t* raw_end() noexcept { return m_ptr + m_size; }
    const uint64_t* raw_begin() const noexcept { return m_ptr; }
    const uint64_t* raw_end() const noexcept { return m_ptr + m_size; }


    uint64_t*   m_ptr = (uint64_t*)&m_local;
    uint32_t    m_size = 0;
    uint32_t    m_capacity = max_local;
    uint64_t    m_local[max_local];

};
