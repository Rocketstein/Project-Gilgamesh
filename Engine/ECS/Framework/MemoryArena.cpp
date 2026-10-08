#include "MemoryArena.h"

MemoryArena::MemoryArena(std::uint64_t pageSize)
	: pageSize_(pageSize)
{
	AllocateNewPage();
}

void* MemoryArena::Allocate(std::uint64_t size, std::uint64_t alignment)
{
	Page* currentPage = &pages_.back();

	void* currentAddress = currentPage->data.get() + currentPage->offset;
	uint64_t remainingSize = currentPage->capacity - currentPage->offset;

	if (!std::align(alignment, size, currentAddress, remainingSize))
	{
		AllocateNewPage();
		currentPage = &pages_.back();
		currentAddress = currentPage->data.get();
		remainingSize = currentPage->capacity;

		std::align(alignment, size, currentAddress, remainingSize);
	}

	currentPage->offset = currentPage->capacity - remainingSize + size;
	return currentAddress;
}

void MemoryArena::AllocateNewPage()
{
	pages_.push_back(
		{
			std::make_unique<std::byte[]>(pageSize_),
			pageSize_,
			0
		}
	);
}