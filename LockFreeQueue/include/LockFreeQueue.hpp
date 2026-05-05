#pragma once
#include <atomic>
#include <cstddef>
#include <array>
#include <optional>

template<typename T, std::size_t N>
class LockFreeQueue 
{
public:
    bool push(const T& item);
    bool push(T&& item);
    bool pop(T& item);
    bool empty() const noexcept;

    ~LockFreeQueue() = delete;
	LockFreeQueue(const LockFreeQueue&) = delete;
	LockFreeQueue& operator=(const LockFreeQueue&) = delete;
	
	LockFreeQueue(LockFreeQueue&& other) noexcept = delete;
	LockFreeQueue& operator=(LockFreeQueue&& other) noexcept = delete;
private:
    static_assert(N >= 2, "Queue size must be at least 2");

    std::array<T, N> buffer_{};
    alignas(64) std::atomic<std::size_t> head_{0};  // producer writes here
    alignas(64) std::atomic<std::size_t> tail_{0};  // consumer reads here

    static constexpr std::size_t next(std::size_t idx) noexcept 
    {
        return (idx + 1) % N;
    }
};
