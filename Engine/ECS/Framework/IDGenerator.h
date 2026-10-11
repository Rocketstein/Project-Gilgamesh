#pragma once

#include <atomic>
#include <cstdint>

struct WorldTag {};

template <typename Tag>
class IDGenerator final
{
public:
	static uint32_t Next()
	{
		return nextID.fetch_add(1, std::memory_order_relaxed);
	}

private:
	inline static std::atomic<uint32_t> nextID{ 0 };
};