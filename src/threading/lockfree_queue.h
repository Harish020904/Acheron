#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace acheron::threading
{
    static constexpr std::uint32_t CACHE_LINE_BYTES = 64;

    constexpr bool is_power_of_two(std::uint64_t value) noexcept
    {
        return value != 0u && ((value & (value - 1u)) == 0u);
    }

    template <typename T>
    struct alignas(CACHE_LINE_BYTES) MpscQueueSlot
    {
        std::atomic<std::uint64_t> sequence{ 0 };
        T value{};
    };

    template <typename T>
    class MpscBoundedQueue final
    {
        static_assert(std::is_trivially_copyable_v<T>, "MpscBoundedQueue requires trivially copyable values");

    public:
        using Slot = MpscQueueSlot<T>;

        MpscBoundedQueue() noexcept = default;

        MpscBoundedQueue(const MpscBoundedQueue&) = delete;
        MpscBoundedQueue& operator=(const MpscBoundedQueue&) = delete;

        bool init(Slot* slots, std::uint32_t capacity) noexcept
        {
            if (!slots || capacity < 2u || !is_power_of_two(capacity))
            {
                reset();
                return false;
            }

            m_slots = slots;
            m_capacity = capacity;
            m_mask = capacity - 1u;
            m_enqueue_pos.store(0, std::memory_order_relaxed);
            m_dequeue_pos.store(0, std::memory_order_relaxed);

            for (std::uint32_t i = 0; i < capacity; ++i)
            {
                m_slots[i].sequence.store(static_cast<std::uint64_t>(i), std::memory_order_relaxed);
                m_slots[i].value = T{};
            }

            return true;
        }

        void reset() noexcept
        {
            m_slots = nullptr;
            m_capacity = 0;
            m_mask = 0;
            m_enqueue_pos.store(0, std::memory_order_relaxed);
            m_dequeue_pos.store(0, std::memory_order_relaxed);
        }

        bool try_push(const T& value) noexcept
        {
            if (!m_slots)
            {
                return false;
            }

            Slot* slot = nullptr;
            std::uint64_t pos = m_enqueue_pos.load(std::memory_order_relaxed);

            for (;;)
            {
                slot = &m_slots[pos & m_mask];
                const std::uint64_t seq = slot->sequence.load(std::memory_order_acquire);
                const std::int64_t diff = static_cast<std::int64_t>(seq) - static_cast<std::int64_t>(pos);

                if (diff == 0)
                {
                    if (m_enqueue_pos.compare_exchange_weak(pos,
                                                            pos + 1u,
                                                            std::memory_order_relaxed,
                                                            std::memory_order_relaxed))
                    {
                        break;
                    }
                }
                else if (diff < 0)
                {
                    return false;
                }
                else
                {
                    pos = m_enqueue_pos.load(std::memory_order_relaxed);
                }
            }

            slot->value = value;
            slot->sequence.store(pos + 1u, std::memory_order_release);
            return true;
        }

        bool try_pop(T& out) noexcept
        {
            if (!m_slots)
            {
                return false;
            }

            const std::uint64_t pos = m_dequeue_pos.load(std::memory_order_relaxed);
            Slot& slot = m_slots[pos & m_mask];
            const std::uint64_t seq = slot.sequence.load(std::memory_order_acquire);
            const std::int64_t diff = static_cast<std::int64_t>(seq) - static_cast<std::int64_t>(pos + 1u);

            if (diff != 0)
            {
                return false;
            }

            out = slot.value;
            slot.sequence.store(pos + static_cast<std::uint64_t>(m_capacity), std::memory_order_release);
            m_dequeue_pos.store(pos + 1u, std::memory_order_relaxed);
            return true;
        }

        bool empty() const noexcept
        {
            return approximate_size() == 0u;
        }

        std::uint32_t capacity() const noexcept
        {
            return m_capacity;
        }

        std::uint32_t approximate_size() const noexcept
        {
            const std::uint64_t enq = m_enqueue_pos.load(std::memory_order_acquire);
            const std::uint64_t deq = m_dequeue_pos.load(std::memory_order_acquire);
            const std::uint64_t size = (enq >= deq) ? (enq - deq) : 0u;
            return (size > m_capacity) ? m_capacity : static_cast<std::uint32_t>(size);
        }

    private:
        alignas(CACHE_LINE_BYTES) std::atomic<std::uint64_t> m_enqueue_pos{ 0 };
        alignas(CACHE_LINE_BYTES) std::atomic<std::uint64_t> m_dequeue_pos{ 0 };

        Slot* m_slots = nullptr;
        std::uint32_t m_capacity = 0;
        std::uint32_t m_mask = 0;
    };

    template <typename T, std::uint32_t Capacity>
    class StaticMpscBoundedQueue final
    {
        static_assert(is_power_of_two(Capacity), "StaticMpscBoundedQueue capacity must be a power of two");
        static_assert(Capacity >= 2u, "StaticMpscBoundedQueue capacity must be at least two");

    public:
        using SlotStorage = MpscQueueSlot<T>;

        StaticMpscBoundedQueue() noexcept
        {
            (void)m_queue.init(m_slots, Capacity);
        }

        StaticMpscBoundedQueue(const StaticMpscBoundedQueue&) = delete;
        StaticMpscBoundedQueue& operator=(const StaticMpscBoundedQueue&) = delete;

        bool try_push(const T& value) noexcept { return m_queue.try_push(value); }
        bool try_pop(T& out) noexcept { return m_queue.try_pop(out); }
        bool empty() const noexcept { return m_queue.empty(); }
        std::uint32_t capacity() const noexcept { return m_queue.capacity(); }
        std::uint32_t approximate_size() const noexcept { return m_queue.approximate_size(); }

    private:
        alignas(CACHE_LINE_BYTES) SlotStorage m_slots[Capacity]{};
        MpscBoundedQueue<T> m_queue{};
    };
}
