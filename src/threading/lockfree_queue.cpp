#include "lockfree_queue.h"

namespace acheron::threading
{
    static_assert(CACHE_LINE_BYTES == 64u);
    static_assert(alignof(MpscQueueSlot<std::uint32_t>) == CACHE_LINE_BYTES);
}

#if defined(ACHERON_LOCKFREE_QUEUE_UNIT_TEST)

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

namespace
{
    struct QueueItem
    {
        std::uint32_t producer = 0;
        std::uint32_t value = 0;
    };

    static bool race_condition_validation() noexcept
    {
        acheron::threading::StaticMpscBoundedQueue<QueueItem, 1024> queue;

        constexpr std::uint32_t producer_count = 8;
        constexpr std::uint32_t per_producer = 4096;
        constexpr std::uint32_t total = producer_count * per_producer;

        std::atomic<std::uint32_t> ready{ 0 };
        std::atomic<bool> start{ false };
        std::atomic<std::uint32_t> produced{ 0 };
        std::atomic<std::uint32_t> consumed{ 0 };

        std::uint32_t seen[producer_count][per_producer]{};

        std::thread producers[producer_count];
        for (std::uint32_t p = 0; p < producer_count; ++p)
        {
            producers[p] = std::thread([p, &queue, &ready, &start, &produced]() noexcept {
                ready.fetch_add(1, std::memory_order_acq_rel);
                while (!start.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }

                for (std::uint32_t i = 0; i < per_producer; ++i)
                {
                    const QueueItem item{ p, i };
                    while (!queue.try_push(item))
                    {
                        std::this_thread::yield();
                    }
                    produced.fetch_add(1, std::memory_order_acq_rel);
                }
            });
        }

        while (ready.load(std::memory_order_acquire) != producer_count)
        {
            std::this_thread::yield();
        }

        const auto begin = std::chrono::steady_clock::now();
        start.store(true, std::memory_order_release);

        while (consumed.load(std::memory_order_acquire) != total)
        {
            QueueItem item{};
            if (queue.try_pop(item))
            {
                if (item.producer >= producer_count || item.value >= per_producer)
                {
                    return false;
                }
                ++seen[item.producer][item.value];
                consumed.fetch_add(1, std::memory_order_acq_rel);
            }
            else
            {
                std::this_thread::yield();
            }
        }

        const auto end = std::chrono::steady_clock::now();

        for (std::uint32_t p = 0; p < producer_count; ++p)
        {
            producers[p].join();
        }

        if (produced.load(std::memory_order_acquire) != total)
        {
            return false;
        }

        for (std::uint32_t p = 0; p < producer_count; ++p)
        {
            for (std::uint32_t i = 0; i < per_producer; ++i)
            {
                if (seen[p][i] != 1u)
                {
                    return false;
                }
            }
        }

        const auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
        std::printf("[lockfree_queue] MPSC contention %u items: %lld us\n",
                    total,
                    static_cast<long long>(elapsed_us));
        return queue.empty();
    }

    static bool alignment_validation() noexcept
    {
        using Slot = acheron::threading::MpscQueueSlot<QueueItem>;
        if (alignof(Slot) != acheron::threading::CACHE_LINE_BYTES)
        {
            return false;
        }

        acheron::threading::StaticMpscBoundedQueue<std::uint32_t, 8> queue;
        for (std::uint32_t i = 0; i < 8; ++i)
        {
            if (!queue.try_push(i))
            {
                return false;
            }
        }
        if (queue.try_push(99u))
        {
            return false;
        }
        for (std::uint32_t i = 0; i < 8; ++i)
        {
            std::uint32_t value = 0;
            if (!queue.try_pop(value) || value != i)
            {
                return false;
            }
        }
        return queue.empty();
    }
}

int main()
{
    if (!alignment_validation())
    {
        std::printf("[lockfree_queue] alignment/capacity: FAIL\n");
        return 1;
    }
    std::printf("[lockfree_queue] alignment/capacity: OK\n");

    if (!race_condition_validation())
    {
        std::printf("[lockfree_queue] race validation: FAIL\n");
        return 1;
    }
    std::printf("[lockfree_queue] race validation: OK\n");
    return 0;
}

#endif
