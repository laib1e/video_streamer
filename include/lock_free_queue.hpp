#pragma once
#include <atomic>
#include <cstddef>
#include <array>
#include <optional>

template<typename T, std::size_t N>
class LockFreeQueue {
    static_assert(N >= 2, "Queue size must be at least 2");

    std::array<T, N> buffer_{};
    alignas(64) std::atomic<std::size_t> head_{0};  // producer writes here
    alignas(64) std::atomic<std::size_t> tail_{0};  // consumer reads here

    static constexpr std::size_t next(std::size_t idx) noexcept {
        return (idx + 1) % N;
    }

public:
    bool push(const T& item) {
        const auto h = head_.load(std::memory_order_relaxed);
        const auto next_h = next(h);
        if (next_h == tail_.load(std::memory_order_acquire)) {
            return false;  // full
        }
        buffer_[h] = item;
        head_.store(next_h, std::memory_order_release);
        return true;
    }

    bool push(T&& item) {
        const auto h = head_.load(std::memory_order_relaxed);
        const auto next_h = next(h);
        if (next_h == tail_.load(std::memory_order_acquire)) {
            return false;  // full
        }
        buffer_[h] = std::move(item);
        head_.store(next_h, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        const auto t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire)) {
            return false;  // empty
        }
        item = std::move(buffer_[t]);
        tail_.store(next(t), std::memory_order_release);
        return true;
    }

    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == 
               tail_.load(std::memory_order_acquire);
    }
};
