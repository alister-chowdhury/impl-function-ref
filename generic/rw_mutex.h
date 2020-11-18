#include <atomic>
#include <thread>
#include <mutex>


// Read/Write mutex
// ----------------
// When only reads are taking place, no locking occurs.
// When writes take place both reads and writes are queued.
//
// General usage:
//
// // Reading
// int get_last_number(std::vector<int>& numbers, rw_mutex& m) {
//     std::lock_guard<rw_mutex::reader> guard(m.rlock());
//     return numbers.back();
// }
//
// // Writing
// void add_number(std::vector<int>& numbers, int n, rw_mutex& m) {
//     std::lock_guard<rw_mutex::writer> guard(m.wlock());
//     numbers.push_back(n);
// }


namespace detail {

    struct rw_mutex_base {
    protected:

        rw_mutex_base() {
            m_active_reads = 0;
        }

        std::atomic_uint32_t    m_active_reads;
        std::mutex  m_lock;
        uint8_t     m_writing = false;
    };

} // namespace detail

struct rw_mutex : detail::rw_mutex_base {


    // rw_mutex, rw_mutex::reader and rw_mutex::writer
    // inherit from rw_mutex_base and don't include any
    // additional members, allowing us to allow each of them
    // to point to the same address in memory.
    struct reader : detail::rw_mutex_base {

        reader() = delete;
        reader(const reader&) = delete;
        reader(reader&&) = delete;
        reader& operator= (reader&) = delete;

        void lock() {
            
            // Attempt to increment the read counter, if it turns out
            // a write is taking place.
            // Decrement the counter, acquire the lock, increment it again
            // treating this read operation as if it was write op.
            ++m_active_reads;
    
            if(!m_writing) {
                return;
            }

            {
                --m_active_reads;
                std::lock_guard<std::mutex> guard(m_lock);
                ++m_active_reads;
            }
        }

        void unlock() {
            --m_active_reads;
        }

    };

    struct writer : detail::rw_mutex_base {

        writer() = delete;
        writer(const writer&) = delete;
        writer(writer&&) = delete;
        writer& operator= (writer&) = delete;

        void lock() {

            // Acquire the lock, then signal writing is taking place.
            m_lock.lock();
            m_writing = true;

            // Wait for anything still reading to stop
            while(m_active_reads) {
                std::this_thread::yield();
            }
        }

        void unlock() {
            m_writing = false;
            m_lock.unlock();
        }

    };

    reader& rlock() {
        return *read_ptr();
    }

    writer& wlock() {
        return *write_ptr();
    }

    // By default use the writers logic
    void lock() { wlock().lock(); }
    void unlock() { wlock().unlock(); }


private:

    reader* read_ptr() {
        return static_cast<reader*>(base_ptr());
    }

    writer* write_ptr() {
        return static_cast<writer*>(base_ptr());
    }

    detail::rw_mutex_base* base_ptr() {
        return this;
    }

};
