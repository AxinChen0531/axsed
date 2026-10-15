#include <cstring>
#include <axsed/core/base.h>
#include <axsed/core/fifo.h>

namespace axsed
{
    RingBufferSPSC::RingBufferSPSC(const uint64_t min_bytes_capacity_req)
        : m_rPos(0), m_cached_wPos(0), m_wPos(0), m_cached_rPos(0) {
        if (min_bytes_capacity_req <= 2) {
            m_capacity = 2;
        } else if (min_bytes_capacity_req >= 1ull << 31) {
            m_capacity = 1ull << 31;
        } else {
            m_capacity = min_bytes_capacity_req;
            uint64_t k = 2;
            while (k < m_capacity)
                k <<= 1;
            m_capacity = k;
        }
        m_buffer = static_cast<uint8_t*>(ax_malloc(m_capacity));
        if (!m_buffer) {
            m_capacity = 0;
        }
        m_mask = m_capacity - 1;
    }

    RingBufferSPSC::~RingBufferSPSC() {
        if (m_buffer) {
            ax_free(m_buffer);
        }
    }

    uint64_t RingBufferSPSC::capacity() const noexcept {
        return m_capacity;
    }

    uint64_t RingBufferSPSC::readAvailable() const noexcept {
        const uint64_t r = m_rPos.load(std::memory_order_relaxed);
        const uint64_t w = m_wPos.load(std::memory_order_acquire);
        return w - r;
    }

    uint64_t RingBufferSPSC::writeAvailable() const noexcept {
        const uint64_t w = m_wPos.load(std::memory_order_relaxed);
        const uint64_t r = m_rPos.load(std::memory_order_acquire);
        return m_capacity - (w - r);
    }

    uint64_t RingBufferSPSC::tryRead(const std::span<uint8_t> buffer) {
        if (m_capacity == 0)
            return 0;
        if (buffer.empty())
            return 0;
        const uint64_t r = m_rPos.load(std::memory_order_relaxed);
        uint64_t actualRLen = buffer.size();
        if (m_cached_wPos - r < buffer.size()) {
            m_cached_wPos = m_wPos.load(std::memory_order_acquire);
            actualRLen = std::min(m_cached_wPos - r, buffer.size());
        }
        const uint64_t phyR = r & m_mask;
        if (const uint64_t tail = m_capacity - phyR; tail >= actualRLen) {
            memcpy(buffer.data(), m_buffer + phyR, actualRLen);
        } else {
            memcpy(buffer.data(), m_buffer + phyR, tail);
            memcpy(buffer.data() + tail, m_buffer, actualRLen - tail);
        }
        m_rPos.store(r + actualRLen, std::memory_order_release);
        return actualRLen;
    }

    bool RingBufferSPSC::tryReadByte(uint8_t& container) {
        if (m_capacity == 0)
            return false;
        const uint64_t r = m_rPos.load(std::memory_order_relaxed);
        if (r == m_cached_wPos) {
            m_cached_wPos = m_wPos.load(std::memory_order_acquire);
            if (r == m_cached_wPos) {
                return false;
            }
        }
        container = m_buffer[r & m_mask];
        m_rPos.store(r + 1, std::memory_order_release);
        return true;
    }

    uint64_t RingBufferSPSC::tryWrite(const std::span<const uint8_t> buffer) {
        if (m_capacity == 0)
            return 0;
        if (buffer.empty())
            return 0;
        const uint64_t w = m_wPos.load(std::memory_order_relaxed);
        uint64_t actualWLen = buffer.size();
        if (m_capacity < buffer.size() + (w - m_cached_rPos)) {
            m_cached_rPos = m_rPos.load(std::memory_order_acquire);
            actualWLen = std::min(m_capacity - (w - m_cached_rPos), buffer.size());
        }
        const uint64_t phyW = w & m_mask;
        if (const uint64_t tail = m_capacity - phyW; tail >= actualWLen) {
            memcpy(m_buffer + phyW, buffer.data(), actualWLen);
        } else {
            memcpy(m_buffer + phyW, buffer.data(), tail);
            memcpy(m_buffer, buffer.data() + tail, actualWLen - tail);
        }
        m_wPos.store(w + actualWLen, std::memory_order_release);
        return actualWLen;
    }

    bool RingBufferSPSC::tryWriteByte(const uint8_t value) {
        if (m_capacity == 0)
            return false;
        const uint64_t w = m_wPos.load(std::memory_order_relaxed);
        if (w - m_cached_rPos == m_capacity) {
            m_cached_rPos = m_rPos.load(std::memory_order_acquire);
            if (w - m_cached_rPos == m_capacity) {
                return false;
            }
        }
        m_buffer[w & m_mask] = value;
        m_wPos.store(w + 1, std::memory_order_release);
        return true;
    }
}
