#pragma once
#include <atomic>
#include <array>
#include <optional>
#include <cstddef>

template<typename T, size_t N>
class RingBuffer {
    static_assert(N > 0, "Buffer size must be > 0");

public:
    bool push(const T& item) {
        const size_t cur_head = head_.load(std::memory_order_relaxed);
        const size_t next_head = (cur_head + 1) % N;

        if (next_head == tail_.load(std::memory_order_acquire))
            return false; // full

        buffer_[cur_head] = item;
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() {
        const size_t cur_tail = tail_.load(std::memory_order_relaxed);

        if (cur_tail == head_.load(std::memory_order_acquire))
            return std::nullopt; // empty

        T item = buffer_[cur_tail];
        tail_.store((cur_tail + 1) % N, std::memory_order_release);
        return item;
    }

private:
    std::array<T, N> buffer_;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
};
