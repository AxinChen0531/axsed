#ifndef AXSED_FIFO_H
#define AXSED_FIFO_H

#include <axsed/core/base.h>
#include <span>

namespace axsed
{
    /**
     * Single Producer Single Consumer FIFO buffer.
     * @attention user must promise 1 thread consume at most & 1 thread produce at most;
     * @note max capacity is 1 << 31;
     * general write/read length is no limitation, ep: UINT64(2) - UINT64_MAX = UINT64(3)
     */
    class AXSEDCORE_API RingBufferSPSC final
    {
    public:
        /**
         * actual capacity will be 1 << n which could meet the requirement, n ∈ [1, 31].
         * @param min_bytes_capacity_req the minimum requirement of the buffer
         */
        explicit RingBufferSPSC(uint64_t min_bytes_capacity_req = 1024);
        /**
         * @attention keep the deconstruction in main thread
         */
        RingBufferSPSC(const RingBufferSPSC&) = delete;
        RingBufferSPSC(RingBufferSPSC&&) = delete;
        RingBufferSPSC& operator=(const RingBufferSPSC&) = delete;
        RingBufferSPSC& operator=(RingBufferSPSC&&) = delete;

        /**
         * must delete instance after the referenced producer & consumer finished
         */
        ~RingBufferSPSC();

        /**
         * get the ringbuffer capacity, the maximum byte size to store data haven't consumed.
         * @return capacity
         * @attention multi-thread safe
         */
        [[nodiscard]] uint64_t capacity() const noexcept;
        /**
         * get the reserved space in bytes for consumer.
         * do not call in high frequency.
         * @return reserved bytes for read
         * @attention only for 1 consumer thread
         */
        [[nodiscard]] uint64_t readAvailable() const noexcept;
        /**
         * get the reserved space in bytes for producer.
         * do not call in high frequency.
         * @return reserved bytes for write
         * @attention only for 1 producer thread
         */
        [[nodiscard]] uint64_t writeAvailable() const noexcept;
        /**
         * try consume data without block
         * @param buffer the container to get the result
         * @return the actual consumed data size in bytes
         * @attention only for 1 consumer thread
         */
        [[nodiscard]] uint64_t tryRead(std::span<uint8_t> buffer);
        /**
         * try consume 1 byte data without block
         * @param container to accept the reading result
         * @return false for failure, otherwise the read byte
         * @attention only for 1 consumer thread
         */
        [[nodiscard]] bool tryReadByte(uint8_t& container);
        /**
         * try produce data without block
         * @param buffer the container to provide the data
         * @return the actual produced data size in bytes
         * @attention only for 1 producer thread
         */
        [[nodiscard]] uint64_t tryWrite(std::span<const uint8_t> buffer);
        /**
         * try produce one byte data without block
         * @param value the byte to try writing in
         * @return true for success
         * @attention only for 1 producer thread
         */
        [[nodiscard]] bool tryWriteByte(uint8_t value);

    private:
        alignas(AXSEDCORE_CACHE_LINE_SIZE) std::atomic<uint64_t> m_rPos;
        uint64_t m_cached_wPos;
        uint64_t m_capacity;
        uint64_t m_mask;
        uint8_t* m_buffer = nullptr;
        alignas(AXSEDCORE_CACHE_LINE_SIZE) std::atomic<uint64_t> m_wPos;
        uint64_t m_cached_rPos;
    };

    /**
     * typed version of @ref axe::RingBufferSPSC.
     * @tparam T the specified type, must be assignable with T& or T&& (T& operator=(const T& v / T&& v)) for pop using
     * @attention input functions may have different limits with the value type, but there is no need to meet all the requirement
     * - T must constructible with const T& to support the copy push
     * - T must constructible with T&& to support the move push
     * - T must constructible with Arg... to support the emplace push(Arg&&...)
     */
    template<typename T>
    class TRingBufferSPSC final
    {
    public:
        /**
         * actual capacity will be 1 << n which could meet the requirement, n ∈ [1, 16].
         * @param min_instance_count_req the minimum requirement of the instance count
         */
        explicit TRingBufferSPSC(const uint64_t min_instance_count_req = 64)
            : m_rPos(0), m_cached_wPos(0), m_wPos(0), m_cached_rPos(0) {
            if (min_instance_count_req <= 2) {
                m_capacity = 2;
            } else if (min_instance_count_req >= 1ull << 16) {
                m_capacity = 1ull << 16;
            } else {
                m_capacity = min_instance_count_req;
                uint64_t k = 2;
                while (k < m_capacity)
                    k <<= 1;
                m_capacity = k;
            }
            m_buffer = AX_ALIGNED_MALLOC(T, m_capacity);
            if (!m_buffer) {
                m_capacity = 0;
            }
            m_mask = m_capacity - 1;
        }

        TRingBufferSPSC(const TRingBufferSPSC&) = delete;
        TRingBufferSPSC(TRingBufferSPSC&&) = delete;
        TRingBufferSPSC& operator=(const TRingBufferSPSC&) = delete;
        TRingBufferSPSC& operator=(TRingBufferSPSC&&) = delete;

        /**
         * must delete instance after the referenced producer & consumer finished
         */
        ~TRingBufferSPSC() {
            if (m_buffer) {
                uint64_t r = m_rPos.load(std::memory_order_relaxed);
                const uint64_t w = m_wPos.load(std::memory_order_relaxed);
                while (r != w) {
                    std::destroy_at(&m_buffer[r & m_mask]);
                    ++r;
                }
                AX_FREE(m_buffer);
            }
        }

        /**
         * get the ringbuffer capacity, the maximum instance count to store data haven't consumed.
         * @return capacity
         * @attention multi-thread safe
         */
        [[nodiscard]] uint64_t capacity() const noexcept { return m_capacity; }

        /**
         * get the reserved instance count for consumer.
         * do not call in high frequency.
         * @return reserved count for read
         * @attention only for 1 consumer thread
         */
        [[nodiscard]] uint64_t popAvailable() const noexcept {
            const uint64_t r = m_rPos.load(std::memory_order_relaxed);
            const uint64_t w = m_wPos.load(std::memory_order_acquire);
            return w - r;
        }

        /**
         * get the reserved instance count for producer.
         * do not call in high frequency.
         * @return reserved count for write
         * @attention only for 1 producer thread
         */
        [[nodiscard]] uint64_t pushAvailable() const noexcept {
            const uint64_t w = m_wPos.load(std::memory_order_relaxed);
            const uint64_t r = m_rPos.load(std::memory_order_acquire);
            return m_capacity - (w - r);
        }

        /**
         * try consume a instance without block
         * @param container the container to get the result
         * @return true for success
         * @attention only for 1 consumer thread
         */
        [[nodiscard]] bool tryPop(T& container) {
            static_assert(std::is_assignable_v<T&, T&&> || std::is_assignable_v<T&, const T&>);
            if (m_capacity == 0)
                return false;
            const uint64_t r = m_rPos.load(std::memory_order_relaxed);
            if (r == m_cached_wPos) {
                m_cached_wPos = m_wPos.load(std::memory_order_acquire);
                if (r == m_cached_wPos) {
                    return false;
                }
            }
            const uint64_t phyR = r & m_mask;
            container = std::move(m_buffer[phyR]);
            std::destroy_at(&m_buffer[phyR]);
            m_rPos.store(r + 1, std::memory_order_release);
            return true;
        }

        /**
         * try produce one instance (construct) without block
         * @param args the construction parameters for the instance
         * @return true for success
         * @attention only for 1 producer thread
         */
        template<typename... Arg>
        [[nodiscard]] bool tryEmplace(Arg&&... args) {
            static_assert(std::is_constructible_v<T, Arg...>);
            if (m_capacity == 0)
                return false;
            const uint64_t w = m_wPos.load(std::memory_order_relaxed);
            if (w - m_cached_rPos == m_capacity) {
                m_cached_rPos = m_rPos.load(std::memory_order_acquire);
                if (w - m_cached_rPos == m_capacity) {
                    return false;
                }
            }
            std::construct_at(&m_buffer[w & m_mask], std::forward<Arg>(args)...);
            m_wPos.store(w + 1, std::memory_order_release);
            return true;
        }

        /**
         * try produce one instance (copy) without block
         * @param value provide the copy of the instance
         * @return true for success
         * @attention only for 1 producer thread
         */
        [[nodiscard]] bool tryPush(const T& value) {
            static_assert(std::is_copy_constructible_v<T>);
            if (m_capacity == 0)
                return false;
            const uint64_t w = m_wPos.load(std::memory_order_relaxed);
            if (w - m_cached_rPos == m_capacity) {
                m_cached_rPos = m_rPos.load(std::memory_order_acquire);
                if (w - m_cached_rPos == m_capacity) {
                    return false;
                }
            }
            std::construct_at(&m_buffer[w & m_mask], value);
            m_wPos.store(w + 1, std::memory_order_release);
            return true;
        }

        /**
         * try produce one instance (move) without block
         * @param value provide the the instance, be moved after success call
         * @return true for success
         * @attention only for 1 producer thread
         */
        [[nodiscard]] bool tryPush(T&& value) {
            static_assert(std::is_move_constructible_v<T>);
            if (m_capacity == 0)
                return false;
            const uint64_t w = m_wPos.load(std::memory_order_relaxed);
            if (w - m_cached_rPos == m_capacity) {
                m_cached_rPos = m_rPos.load(std::memory_order_acquire);
                if (w - m_cached_rPos == m_capacity) {
                    return false;
                }
            }
            std::construct_at(&m_buffer[w & m_mask], std::move(value));
            m_wPos.store(w + 1, std::memory_order_release);
            return true;
        }

    private:
        alignas(AXSEDCORE_CACHE_LINE_SIZE) std::atomic<uint64_t> m_rPos;
        uint64_t m_cached_wPos;
        uint64_t m_capacity;
        uint64_t m_mask;
        T* m_buffer = nullptr;
        alignas(AXSEDCORE_CACHE_LINE_SIZE) std::atomic<uint64_t> m_wPos;
        uint64_t m_cached_rPos;
    };

    /**
     * for internal using;
     * need construct/destroy manually
     * @tparam T
     */
    template<typename T>
    struct VyukovSequenceCell
    {
        alignas(AXSEDCORE_CACHE_LINE_SIZE) std::atomic<uint64_t> Seq = 0;
        alignas(alignof(T)) uint8_t Buffer[sizeof(T)]{};
    };

    /**
     * Multi Producer Single Consumer FIFO buffer, only support the typed version.
     * @tparam T the specified type, must be assignable with T& or T&& (T& operator=(const T& v / T&& v)) for pop using
     * @attention input functions may have different limits with the value type, but there is no need to meet all the requirement
     * - T must constructible with const T& to support the copy push
     * - T must constructible with T&& to support the move push
     * - T must constructible with Arg... to support the emplace push(Arg&&...)
     */
    template<typename T>
    class TRingBufferMPSC final
    {
    public:
        /**
         * actual capacity will be 1 << n which could meet the requirement, n ∈ [1, 16].
         * @param min_instance_count_req the minimum requirement of the instance count
         */
        explicit TRingBufferMPSC(const uint64_t min_instance_count_req = 64)
            : m_rPos(0), m_wPos(0) {
            if (min_instance_count_req <= 2) {
                m_capacity = 2;
            } else if (min_instance_count_req >= 1ull << 16) {
                m_capacity = 1ull << 16;
            } else {
                m_capacity = min_instance_count_req;
                uint64_t k = 2;
                while (k < m_capacity)
                    k <<= 1;
                m_capacity = k;
            }
            m_sequence = AX_ALIGNED_MALLOC(VyukovSequenceCell<T>, m_capacity);
            if (!m_sequence) {
                m_capacity = 0;
            } else {
                for (uint64_t i = 0; i < m_capacity; ++i) {
                    std::construct_at(&m_sequence[i]);
                    m_sequence[i].Seq.store(i, std::memory_order_relaxed);
                }
            }
            m_mask = m_capacity - 1;
        }

        TRingBufferMPSC(const TRingBufferMPSC&) = delete;
        TRingBufferMPSC(TRingBufferMPSC&&) = delete;
        TRingBufferMPSC& operator=(const TRingBufferMPSC&) = delete;
        TRingBufferMPSC& operator=(TRingBufferMPSC&&) = delete;

        /**
         * must delete instance after the referenced producers & consumer finished
         */
        ~TRingBufferMPSC() {
            if (m_capacity == 0)
                return;
            const uint64_t w = m_wPos.load(std::memory_order_relaxed);
            for (uint64_t r = m_rPos.load(std::memory_order_relaxed); r != w; ++r)
                std::destroy_at(reinterpret_cast<T*>(m_sequence[r & m_mask].Buffer));
            for (uint64_t i = 0; i < m_capacity; ++i)
                std::destroy_at(&m_sequence[i]);
            AX_FREE(m_sequence);
        }

        /**
         * get the ringbuffer capacity, the maximum instance count to store data haven't consumed.
         * @return capacity
         * @attention multi-thread safe
         */
        [[nodiscard]] uint64_t capacity() const noexcept { return m_capacity; }

        /**
         * get the reserved instance count for consumer.
         * do not call in high frequency.
         * @return reserved count for read
         * @attention only for 1 consumer's thread
         */
        [[nodiscard]] uint64_t popAvailable() const noexcept {
            const uint64_t r = m_rPos.load(std::memory_order_relaxed);
            const uint64_t w = m_wPos.load(std::memory_order_acquire);
            return w - r;
        }

        /**
         * get the reserved instance count for producer.
         * do not call in high frequency.
         * @return reserved count for write
         * @attention for producers' threads
         */
        [[nodiscard]] uint64_t pushAvailable() const noexcept {
            const uint64_t w = m_wPos.load(std::memory_order_relaxed);
            const uint64_t r = m_rPos.load(std::memory_order_acquire);
            return m_capacity - (w - r);
        }

        /**
         * try consume a instance without block
         * @param container the container to get the result
         * @return true for success
         * @attention only for 1 consumer's thread
         */
        [[nodiscard]] bool tryPop(T& container) requires
            std::is_assignable_v<T&, T&&> || std::is_assignable_v<T&, const T&> {
            if (m_capacity == 0)
                return false;
            const uint64_t r = m_rPos.load(std::memory_order_relaxed);
            const uint64_t index = r & m_mask;
            if (const uint64_t seq = m_sequence[index].Seq.load(std::memory_order_acquire); seq != r + 1)
                return false;
            container = std::move(*reinterpret_cast<T*>(m_sequence[index].Buffer));
            std::destroy_at(reinterpret_cast<T*>(m_sequence[index].Buffer));
            m_sequence[index].Seq.store(r + m_capacity, std::memory_order_release);
            m_rPos.store(r + 1, std::memory_order_relaxed);
            return true;
        }

        /**
         * try produce one instance (construct) without block
         * @param args the construction parameters for the instance
         * @return true for success
         * @attention work for producers' threads
         */
        template<typename... Arg> requires std::is_constructible_v<T, Arg...>
        [[nodiscard]] bool tryEmplace(Arg&&... args) {
            if (m_capacity == 0)
                return false;
            uint8_t spin = 0;
            while (true) {
                uint64_t w = m_wPos.load(std::memory_order_relaxed);
                const uint64_t index = w & m_mask;
                const uint64_t seq = m_sequence[index].Seq.load(std::memory_order_acquire);
                if (const int64_t interval = static_cast<int64_t>(seq) - static_cast<int64_t>(w); interval == 0) {
                    if (m_wPos.compare_exchange_weak(w, w + 1, std::memory_order_relaxed)) {
                        std::construct_at(reinterpret_cast<T*>(m_sequence[index].Buffer), std::forward<Arg>(args)...);
                        m_sequence[index].Seq.store(w + 1, std::memory_order_release);
                        return true;
                    }
                } else if (interval < 0) {
                    return false;
                }
                if (spin++; spin == AXSEDCORE_RINGBUFFER_SPIN_COUNT) {
                    AX_CPU_PAUSE_HINT();
                    spin = 0;
                }
            }
        }

        /**
         * try produce one instance (copy) without block
         * @param value provide the copy of the instance
         * @return true for success
         * @attention work for producers' threads
         */
        [[nodiscard]] bool tryPush(const T& value) requires std::is_copy_constructible_v<T> {
            if (m_capacity == 0)
                return false;
            uint8_t spin = 0;
            while (true) {
                uint64_t w = m_wPos.load(std::memory_order_relaxed);
                const uint64_t index = w & m_mask;
                const uint64_t seq = m_sequence[index].Seq.load(std::memory_order_acquire);
                if (const int64_t interval = static_cast<int64_t>(seq) - static_cast<int64_t>(w); interval == 0) {
                    if (m_wPos.compare_exchange_weak(w, w + 1, std::memory_order_relaxed)) {
                        std::construct_at(reinterpret_cast<T*>(m_sequence[index].Buffer), value);
                        m_sequence[index].Seq.store(w + 1, std::memory_order_release);
                        return true;
                    }
                } else if (interval < 0) {
                    return false;
                }
                if (spin++; spin == AXSEDCORE_RINGBUFFER_SPIN_COUNT) {
                    AX_CPU_PAUSE_HINT();
                    spin = 0;
                }
            }
        }

        /**
         * try produce one instance (move) without block
         * @param value provide the the instance, be moved after success call
         * @return true for success
         * @attention work for producers' threads
         */
        [[nodiscard]] bool tryPush(T&& value) requires std::is_move_constructible_v<T> {
            if (m_capacity == 0)
                return false;
            uint8_t spin = 0;
            while (true) {
                uint64_t w = m_wPos.load(std::memory_order_relaxed);
                const uint64_t index = w & m_mask;
                const uint64_t seq = m_sequence[index].Seq.load(std::memory_order_acquire);
                if (const int64_t interval = static_cast<int64_t>(seq) - static_cast<int64_t>(w); interval == 0) {
                    if (m_wPos.compare_exchange_weak(w, w + 1, std::memory_order_relaxed)) {
                        std::construct_at(reinterpret_cast<T*>(m_sequence[index].Buffer), std::move(value));
                        m_sequence[index].Seq.store(w + 1, std::memory_order_release);
                        return true;
                    }
                } else if (interval < 0) {
                    return false;
                }
                if (spin++; spin == AXSEDCORE_RINGBUFFER_SPIN_COUNT) {
                    AX_CPU_PAUSE_HINT();
                    spin = 0;
                }
            }
        }

    private:
        alignas(AXSEDCORE_CACHE_LINE_SIZE) std::atomic<uint64_t> m_rPos;
        alignas(AXSEDCORE_CACHE_LINE_SIZE) std::atomic<uint64_t> m_wPos;
        VyukovSequenceCell<T>* m_sequence = nullptr;
        uint64_t m_capacity;
        uint64_t m_mask;
    };

    /**
     * Single Producer Multi Consumer FIFO buffer, only support the typed version.
     * @tparam T the specified type, must be assignable with T& or T&& (T& operator=(const T& v / T&& v)) for pop using
     * @attention input functions may have different limits with the value type, but there is no need to meet all the requirement
     * - T must constructible with const T& to support the copy push
     * - T must constructible with T&& to support the move push
     * - T must constructible with Arg... to support the emplace push(Arg&&...)
     */
    template<typename T>
    class TRingBufferSPMC final
    {
    public:
        /**
         * actual capacity will be 1 << n which could meet the requirement, n ∈ [1, 16].
         * @param min_instance_count_req the minimum requirement of the instance count
         */
        explicit TRingBufferSPMC(const uint64_t min_instance_count_req = 64)
            : m_rPos(0), m_wPos(0) {
            if (min_instance_count_req <= 2) {
                m_capacity = 2;
            } else if (min_instance_count_req >= 1ull << 16) {
                m_capacity = 1ull << 16;
            } else {
                m_capacity = min_instance_count_req;
                uint64_t k = 2;
                while (k < m_capacity)
                    k <<= 1;
                m_capacity = k;
            }
            m_sequence = AX_ALIGNED_MALLOC(VyukovSequenceCell<T>, m_capacity);
            if (!m_sequence) {
                m_capacity = 0;
            } else {
                for (uint64_t i = 0; i < m_capacity; ++i) {
                    std::construct_at(&m_sequence[i]);
                    m_sequence[i].Seq.store(i, std::memory_order_relaxed);
                }
            }

            m_mask = m_capacity - 1;
        }

        TRingBufferSPMC(const TRingBufferSPMC&) = delete;
        TRingBufferSPMC(TRingBufferSPMC&&) = delete;
        TRingBufferSPMC& operator=(const TRingBufferSPMC&) = delete;
        TRingBufferSPMC& operator=(TRingBufferSPMC&&) = delete;

        /**
         * must delete instance after the referenced producers & consumer finished
         */
        ~TRingBufferSPMC() {
            if (m_capacity == 0)
                return;
            const uint64_t w = m_wPos.load(std::memory_order_relaxed);
            for (uint64_t r = m_rPos.load(std::memory_order_relaxed); r != w; ++r)
                std::destroy_at(reinterpret_cast<T*>(m_sequence[r & m_mask].Buffer));
            for (uint64_t i = 0; i < m_capacity; ++i)
                std::destroy_at(&m_sequence[i]);
            AX_FREE(m_sequence);
        }

        /**
         * get the ringbuffer capacity, the maximum instance count to store data haven't consumed.
         * @return capacity
         * @attention multi-thread safe
         */
        [[nodiscard]] uint64_t capacity() const noexcept { return m_capacity; }

        /**
         * get the reserved instance count for consumer.
         * do not call in high frequency.
         * @return reserved count for read
         * @attention for consumers' threads
         */
        [[nodiscard]] uint64_t popAvailable() const noexcept {
            const uint64_t r = m_rPos.load(std::memory_order_relaxed);
            const uint64_t w = m_wPos.load(std::memory_order_acquire);
            return w - r;
        }

        /**
         * get the reserved instance count for producer.
         * do not call in high frequency.
         * @return reserved count for write
         * @attention only for 1 producer's thread
         */
        [[nodiscard]] uint64_t pushAvailable() const noexcept {
            const uint64_t w = m_wPos.load(std::memory_order_relaxed);
            const uint64_t r = m_rPos.load(std::memory_order_acquire);
            return m_capacity - (w - r);
        }

        /**
         * try consume a instance without block
         * @param container the container to get the result
         * @return true for success
         * @attention work for consumers' threads
         */
        [[nodiscard]] bool tryPop(T& container) requires
            std::is_assignable_v<T&, T&&> || std::is_assignable_v<T&, const T&> {
            if (m_capacity == 0)
                return false;
            uint8_t spin = 0;
            while (true) {
                uint64_t r = m_rPos.load(std::memory_order_relaxed);
                const uint64_t index = r & m_mask;
                const uint64_t seq = m_sequence[index].Seq.load(std::memory_order_acquire);
                if (const int64_t interval = static_cast<int64_t>(seq) - static_cast<int64_t>(r + 1); interval == 0) {
                    if (m_rPos.compare_exchange_weak(r, r + 1, std::memory_order_relaxed)) {
                        container = std::move(*reinterpret_cast<T*>(m_sequence[index].Buffer));
                        std::destroy_at(reinterpret_cast<T*>(m_sequence[index].Buffer));
                        m_sequence[index].Seq.store(r + m_capacity, std::memory_order_release);
                        return true;
                    }
                } else if (interval < 0) {
                    return false;
                }
                if (spin++; spin == AXSEDCORE_RINGBUFFER_SPIN_COUNT) {
                    AX_CPU_PAUSE_HINT();
                    spin = 0;
                }
            }
        }

        /**
         * try produce one instance (construct) without block
         * @param args the construction parameters for the instance
         * @return true for success
         * @attention only for 1 producer's thread
         */
        template<typename... Arg>
        [[nodiscard]] bool tryEmplace(Arg&&... args) requires std::is_constructible_v<T, Arg...> {
            if (m_capacity == 0)
                return false;
            const uint64_t w = m_wPos.load(std::memory_order_relaxed);
            const uint64_t index = w & m_mask;
            if (const uint64_t seq = m_sequence[index].Seq.load(std::memory_order_acquire); seq != w) {
                return false;
            }
            std::construct_at(reinterpret_cast<T*>(m_sequence[index].Buffer), std::forward<Arg>(args)...);
            m_sequence[index].Seq.store(w + 1, std::memory_order_release);
            m_wPos.store(w + 1, std::memory_order_relaxed);
            return true;
        }

        /**
         * try produce one instance (copy) without block
         * @param value provide the copy of the instance
         * @return true for success
         * @attention only for 1 producer's thread
         */
        [[nodiscard]] bool tryPush(const T& value) requires std::is_copy_constructible_v<T> {
            if (m_capacity == 0)
                return false;
            const uint64_t w = m_wPos.load(std::memory_order_relaxed);
            const uint64_t index = w & m_mask;
            if (const uint64_t seq = m_sequence[index].Seq.load(std::memory_order_acquire); seq != w) {
                return false;
            }
            std::construct_at(reinterpret_cast<T*>(m_sequence[index].Buffer), value);
            m_sequence[index].Seq.store(w + 1, std::memory_order_release);
            m_wPos.store(w + 1, std::memory_order_relaxed);
            return true;
        }

        /**
         * try produce one instance (move) without block
         * @param value provide the the instance, be moved after success call
         * @return true for success
         * @attention only for 1 producer's thread
         */
        [[nodiscard]] bool tryPush(T&& value) requires std::is_move_constructible_v<T> {
            if (m_capacity == 0)
                return false;
            const uint64_t w = m_wPos.load(std::memory_order_relaxed);
            const uint64_t index = w & m_mask;
            if (const uint64_t seq = m_sequence[index].Seq.load(std::memory_order_acquire); seq != w) {
                return false;
            }
            std::construct_at(reinterpret_cast<T*>(m_sequence[index].Buffer), std::move(value));
            m_sequence[index].Seq.store(w + 1, std::memory_order_release);
            m_wPos.store(w + 1, std::memory_order_relaxed);
            return true;
        }

    private:
        alignas(AXSEDCORE_CACHE_LINE_SIZE) std::atomic<uint64_t> m_rPos;
        alignas(AXSEDCORE_CACHE_LINE_SIZE) std::atomic<uint64_t> m_wPos;
        VyukovSequenceCell<T>* m_sequence = nullptr;
        uint64_t m_capacity;
        uint64_t m_mask;
    };

    /**
     * Multi Producer Multi Consumer FIFO buffer, only support the typed version.
     * @tparam T the specified type, must be assignable with T& or T&& (T& operator=(const T& v / T&& v)) for pop using
     * @attention input functions may have different limits with the value type, but there is no need to meet all the requirement
     * - T must constructible with const T& to support the copy push
     * - T must constructible with T&& to support the move push
     * - T must constructible with Arg... to support the emplace push(Arg&&...)
     */
    template<typename T>
    class TRingBufferMPMC final
    {
    public:
        /**
         * actual capacity will be 1 << n which could meet the requirement, n ∈ [1, 16].
         * @param min_instance_count_req the minimum requirement of the instance count
         */
        explicit TRingBufferMPMC(const uint64_t min_instance_count_req = 64)
            : m_rPos(0), m_wPos(0) {
            if (min_instance_count_req <= 2) {
                m_capacity = 2;
            } else if (min_instance_count_req >= 1ull << 16) {
                m_capacity = 1ull << 16;
            } else {
                m_capacity = min_instance_count_req;
                uint64_t k = 2;
                while (k < m_capacity)
                    k <<= 1;
                m_capacity = k;
            }
            m_sequence = AX_ALIGNED_MALLOC(VyukovSequenceCell<T>, m_capacity);
            if (!m_sequence) {
                m_capacity = 0;
            } else {
                for (uint64_t i = 0; i < m_capacity; ++i) {
                    std::construct_at(&m_sequence[i]);
                    m_sequence[i].Seq.store(i, std::memory_order_relaxed);
                }
            }
            m_mask = m_capacity - 1;
        }

        TRingBufferMPMC(const TRingBufferMPMC&) = delete;
        TRingBufferMPMC(TRingBufferMPMC&&) = delete;
        TRingBufferMPMC& operator=(const TRingBufferMPMC&) = delete;
        TRingBufferMPMC& operator=(TRingBufferMPMC&&) = delete;

        /**
         * must delete instance after the referenced producers & consumer finished
         */
        ~TRingBufferMPMC() {
            if (m_capacity == 0)
                return;
            const uint64_t w = m_wPos.load(std::memory_order_relaxed);
            for (uint64_t r = m_rPos.load(std::memory_order_relaxed); r != w; ++r)
                std::destroy_at(reinterpret_cast<T*>(m_sequence[r & m_mask].Buffer));
            for (uint64_t i = 0; i < m_capacity; ++i)
                std::destroy_at(&m_sequence[i]);
            AX_FREE(m_sequence);
        }

        /**
         * get the ringbuffer capacity, the maximum instance count to store data haven't consumed.
         * @return capacity
         * @attention multi-thread safe
         */
        [[nodiscard]] uint64_t capacity() const noexcept { return m_capacity; }

        /**
         * get the reserved instance count for consumer.
         * do not call in high frequency.
         * @return reserved count for read
         * @attention for consumers' threads
         */
        [[nodiscard]] uint64_t popAvailable() const noexcept {
            const uint64_t r = m_rPos.load(std::memory_order_relaxed);
            const uint64_t w = m_wPos.load(std::memory_order_acquire);
            return w - r;
        }

        /**
         * get the reserved instance count for producer.
         * do not call in high frequency.
         * @return reserved count for write
         * @attention for producers' threads
         */
        [[nodiscard]] uint64_t pushAvailable() const noexcept {
            const uint64_t w = m_wPos.load(std::memory_order_relaxed);
            const uint64_t r = m_rPos.load(std::memory_order_acquire);
            return m_capacity - (w - r);
        }

        /**
         * try consume a instance without block
         * @param container the container to get the result
         * @return true for success
         * @attention work for consumers' threads
         */
        [[nodiscard]] bool tryPop(T& container) requires
            std::is_assignable_v<T&, T&&> || std::is_assignable_v<T&, const T&> {
            if (m_capacity == 0)
                return false;
            uint8_t spin = 0;
            while (true) {
                uint64_t r = m_rPos.load(std::memory_order_relaxed);
                const uint64_t index = r & m_mask;
                const uint64_t seq = m_sequence[index].Seq.load(std::memory_order_acquire);
                if (const int64_t interval = static_cast<int64_t>(seq) - static_cast<int64_t>(r + 1); interval == 0) {
                    if (m_rPos.compare_exchange_weak(r, r + 1, std::memory_order_relaxed)) {
                        container = std::move(*reinterpret_cast<T*>(m_sequence[index].Buffer));
                        std::destroy_at(reinterpret_cast<T*>(m_sequence[index].Buffer));
                        m_sequence[index].Seq.store(r + m_capacity, std::memory_order_release);
                        return true;
                    }
                } else if (interval < 0) {
                    return false;
                }
                if (spin++; spin == AXSEDCORE_RINGBUFFER_SPIN_COUNT) {
                    AX_CPU_PAUSE_HINT();
                    spin = 0;
                }
            }
        }

        /**
         * try produce one instance (construct) without block
         * @param args the construction parameters for the instance
         * @return true for success
         * @attention work for producers' threads
         */
        template<typename... Arg>
        [[nodiscard]] bool tryEmplace(Arg&&... args) requires std::is_constructible_v<T, Arg...> {
            if (m_capacity == 0)
                return false;
            uint8_t spin = 0;
            while (true) {
                uint64_t w = m_wPos.load(std::memory_order_relaxed);
                const uint64_t index = w & m_mask;
                const uint64_t seq = m_sequence[index].Seq.load(std::memory_order_acquire);
                if (const int64_t interval = static_cast<int64_t>(seq) - static_cast<int64_t>(w); interval == 0) {
                    if (m_wPos.compare_exchange_weak(w, w + 1, std::memory_order_relaxed)) {
                        std::construct_at(reinterpret_cast<T*>(m_sequence[index].Buffer), std::forward<Arg>(args)...);
                        m_sequence[index].Seq.store(w + 1, std::memory_order_release);
                        return true;
                    }
                } else if (interval < 0) {
                    return false;
                }
                if (spin++; spin == AXSEDCORE_RINGBUFFER_SPIN_COUNT) {
                    AX_CPU_PAUSE_HINT();
                    spin = 0;
                }
            }
        }

        /**
         * try produce one instance (copy) without block
         * @param value provide the copy of the instance
         * @return true for success
         * @attention work for producers' threads
         */
        [[nodiscard]] bool tryPush(const T& value) requires std::is_copy_constructible_v<T> {
            if (m_capacity == 0)
                return false;
            uint8_t spin = 0;
            while (true) {
                uint64_t w = m_wPos.load(std::memory_order_relaxed);
                const uint64_t index = w & m_mask;
                const uint64_t seq = m_sequence[index].Seq.load(std::memory_order_acquire);
                if (const int64_t interval = static_cast<int64_t>(seq) - static_cast<int64_t>(w); interval == 0) {
                    if (m_wPos.compare_exchange_weak(w, w + 1, std::memory_order_relaxed)) {
                        std::construct_at(reinterpret_cast<T*>(m_sequence[index].Buffer), value);
                        m_sequence[index].Seq.store(w + 1, std::memory_order_release);
                        return true;
                    }
                } else if (interval < 0) {
                    return false;
                }
                if (spin++; spin == AXSEDCORE_RINGBUFFER_SPIN_COUNT) {
                    AX_CPU_PAUSE_HINT();
                    spin = 0;
                }
            }
        }

        /**
         * try produce one instance (move) without block
         * @param value provide the the instance, be moved after success call
         * @return true for success
         * @attention work for producers' threads
         */
        [[nodiscard]] bool tryPush(T&& value) requires std::is_move_constructible_v<T> {
            if (m_capacity == 0)
                return false;
            uint8_t spin = 0;
            while (true) {
                uint64_t w = m_wPos.load(std::memory_order_relaxed);
                const uint64_t index = w & m_mask;
                const uint64_t seq = m_sequence[index].Seq.load(std::memory_order_acquire);
                if (const int64_t interval = static_cast<int64_t>(seq) - static_cast<int64_t>(w); interval == 0) {
                    if (m_wPos.compare_exchange_weak(w, w + 1, std::memory_order_relaxed)) {
                        std::construct_at(reinterpret_cast<T*>(m_sequence[index].Buffer), std::move(value));
                        m_sequence[index].Seq.store(w + 1, std::memory_order_release);
                        return true;
                    }
                } else if (interval < 0) {
                    return false;
                }
                if (spin++; spin == AXSEDCORE_RINGBUFFER_SPIN_COUNT) {
                    AX_CPU_PAUSE_HINT();
                    spin = 0;
                }
            }
        }

    private:
        alignas(AXSEDCORE_CACHE_LINE_SIZE) std::atomic<uint64_t> m_rPos;
        alignas(AXSEDCORE_CACHE_LINE_SIZE) std::atomic<uint64_t> m_wPos;
        VyukovSequenceCell<T>* m_sequence = nullptr;
        uint64_t m_capacity;
        uint64_t m_mask;
    };
}

#endif //AXSED_FIFO_H
