#pragma once

#include <cstddef>
#include <memory>
#include <vector>

// Owns fixed-size pages whose allocation addresses remain stable.
class MemoryArena final
{
public:
	MemoryArena(std::size_t pageSize);
	// Allocate raw storage with a nonzero power-of-two alignment.
	void* Allocate(std::size_t size, std::size_t alignment);

	// Rewind pages after the caller has destroyed their objects.
	void Reset();

private:
	void AllocateNewPage();

private:
	struct Page
	{
		std::unique_ptr<std::byte[]> data;	// Fixed Size
		std::size_t capacity = 0; // Total bytes in this page.
		std::size_t offset   = 0; // Next available byte within the page.
	};

	std::vector<Page> pages_;
	std::size_t pageIndex_ = 0; // Current allocation page, rewound by Reset.
	std::size_t pageSize_ = 0; // Bytes allocated for each new page.
};
