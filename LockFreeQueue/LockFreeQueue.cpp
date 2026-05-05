#include "LockFreeQueue.hpp"

template<typename T, std::size_t N>
bool LockFreeQueue<T, N>::push(const T& item) 
{
	const auto h = head_.load(std::memory_order_relaxed);
	const auto next_h = next(h);
	if (next_h == tail_.load(std::memory_order_acquire)) 
	{
		return false;  // full
	}
	buffer_[h] = item;
	head_.store(next_h, std::memory_order_release);
	return true;
}

template<typename T, std::size_t N>
bool LockFreeQueue<T, N>::push(T&& item) 
{
	const auto h = head_.load(std::memory_order_relaxed);
	const auto next_h = next(h);
	if (next_h == tail_.load(std::memory_order_acquire)) 
	{
		return false;  // full
	}
	buffer_[h] = std::move(item);
	head_.store(next_h, std::memory_order_release);
	return true;
}

template<typename T, std::size_t N>
bool LockFreeQueue<T, N>::pop(T& item) 
{
	const auto t = tail_.load(std::memory_order_relaxed);
	if (t == head_.load(std::memory_order_acquire)) 
	{
		return false;  // empty
	}
	item = std::move(buffer_[t]);
	tail_.store(next(t), std::memory_order_release);
	return true;
}

template<typename T, std::size_t N>
bool LockFreeQueue<T, N>::empty() const noexcept 
{
	return head_.load(std::memory_order_acquire) == 
			tail_.load(std::memory_order_acquire);
}