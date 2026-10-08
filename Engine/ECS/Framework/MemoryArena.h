#pragma once

#include <cstdint>
#include <memory>
#include <vector>

class MemoryArena final
{
public:
	MemoryArena(std::uint64_t pageSize);
	void* Allocate(std::uint64_t size, std::uint64_t alignment);

private:
	void AllocateNewPage();

private:
	struct Page
	{
		std::unique_ptr<std::byte[]> data;	// Fixed Size
		std::uint64_t capacity = 0;
		std::uint64_t offset   = 0;
	};

	std::vector<Page> pages_;
	std::uint64_t pageSize_ = 0;
};