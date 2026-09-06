#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace trdqueue {

template <typename T, std::size_t Capacity>
class SpscRingBuffer {
    static_assert(Capacity >= 2, "ring needs at least two slots");
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two so indexing is a mask");
    static_assert(std::is_trivially_copyable_v<T>,
                  "ring slots are memcpy'd; T must be a trivial POD");

public:
    static constexpr std::size_t kCapacity = Capacity;

    bool try_push(const T& item) {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        if (head - tail == Capacity) {
            drops_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        slots_[head & kMask] = item;
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    bool try_pop(T& out) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t head = head_.load(std::memory_order_acquire);
        if (tail == head) {
            return false;
        }
        out = slots_[tail & kMask];
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    std::uint64_t dropped() const {
        return drops_.load(std::memory_order_relaxed);
    }

    std::size_t size_approx() const {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        return head - tail;
    }

private:
    static constexpr std::size_t kMask = Capacity - 1;

    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    alignas(64) std::atomic<std::uint64_t> drops_{0};
    alignas(64) T slots_[Capacity]{};
};

}
