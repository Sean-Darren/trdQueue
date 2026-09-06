#include "trdqueue/spsc_ring_buffer.hpp"
#include "trdqueue/types.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

using trdqueue::Event;
using trdqueue::EventType;
using trdqueue::Side;
using trdqueue::SpscRingBuffer;

TEST(SpscRingBuffer, SingleThreadFifo) {
    SpscRingBuffer<int, 8> ring;
    EXPECT_TRUE(ring.try_push(1));
    EXPECT_TRUE(ring.try_push(2));
    EXPECT_TRUE(ring.try_push(3));
    EXPECT_EQ(ring.size_approx(), 3u);

    int v = 0;
    EXPECT_TRUE(ring.try_pop(v));
    EXPECT_EQ(v, 1);
    EXPECT_TRUE(ring.try_pop(v));
    EXPECT_EQ(v, 2);
    EXPECT_TRUE(ring.try_pop(v));
    EXPECT_EQ(v, 3);
    EXPECT_FALSE(ring.try_pop(v));
    EXPECT_EQ(ring.dropped(), 0u);
}

TEST(SpscRingBuffer, FullBufferDropsAndCounts) {
    SpscRingBuffer<int, 4> ring;
    EXPECT_TRUE(ring.try_push(10));
    EXPECT_TRUE(ring.try_push(20));
    EXPECT_TRUE(ring.try_push(30));
    EXPECT_TRUE(ring.try_push(40));
    EXPECT_FALSE(ring.try_push(50));
    EXPECT_EQ(ring.dropped(), 1u);
    EXPECT_FALSE(ring.try_push(60));
    EXPECT_EQ(ring.dropped(), 2u);

    int v = 0;
    EXPECT_TRUE(ring.try_pop(v));
    EXPECT_EQ(v, 10);
    EXPECT_TRUE(ring.try_push(70));
    EXPECT_EQ(ring.dropped(), 2u);

    EXPECT_TRUE(ring.try_pop(v));
    EXPECT_EQ(v, 20);
    EXPECT_TRUE(ring.try_pop(v));
    EXPECT_EQ(v, 30);
    EXPECT_TRUE(ring.try_pop(v));
    EXPECT_EQ(v, 40);
    EXPECT_TRUE(ring.try_pop(v));
    EXPECT_EQ(v, 70);
    EXPECT_FALSE(ring.try_pop(v));
}

TEST(SpscRingBuffer, TwoThreadSmokeDrainsAll) {
    SpscRingBuffer<Event, 1024> ring;
    constexpr int kCount = 20000;
    std::atomic<int> seen{0};
    std::vector<std::int64_t> prices(static_cast<std::size_t>(kCount), -1);

    std::thread producer([&] {
        for (int i = 0; i < kCount; ++i) {
            Event e{};
            e.type = EventType::LevelUpdate;
            e.side = Side::Buy;
            e.price_ticks = i;
            e.size_ticks = 1;
            while (!ring.try_push(e)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        Event e{};
        int n = 0;
        while (n < kCount) {
            if (!ring.try_pop(e)) {
                std::this_thread::yield();
                continue;
            }
            EXPECT_EQ(e.type, EventType::LevelUpdate);
            prices[static_cast<std::size_t>(n)] = e.price_ticks;
            ++n;
        }
        seen.store(n, std::memory_order_relaxed);
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(seen.load(), kCount);
    Event leftover{};
    EXPECT_FALSE(ring.try_pop(leftover));
    for (int i = 0; i < kCount; ++i) {
        EXPECT_EQ(prices[static_cast<std::size_t>(i)], i);
    }
}
