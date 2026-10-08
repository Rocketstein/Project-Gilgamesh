#include "MemoryArena.h"

#include <bit>
#include <stdexcept>

MemoryArena::MemoryArena(std::size_t pageSize)
	: pageSize_(pageSize)
{
	// Reject empty pages before allocating storage.
	if (pageSize_ == 0)
	{
		throw std::invalid_argument("MemoryArena page size must be greater than zero");
	}

	AllocateNewPage();
}

void* MemoryArena::Allocate(std::size_t size, std::size_t alignment)
{
	// Enforce std::align's precondition before changing any page state.
	if (!std::has_single_bit(alignment))
	{
		throw std::invalid_argument("MemoryArena alignment must be a nonzero power of two");
	}

	Page* currentPage = &pages_.back();

	void* currentAddress = currentPage->data.get() + currentPage->offset;
	std::size_t remainingSize = currentPage->capacity - currentPage->offset;

	// Allocate new page
	if (!std::align(alignment, size, currentAddress, remainingSize))
	{
		AllocateNewPage();
		currentPage = &pages_.back();
		currentAddress = currentPage->data.get();
		remainingSize = currentPage->capacity;

		if (!std::align(alignment, size, currentAddress, remainingSize))
		{
			// Failed to allocate new page
			return nullptr;
		}
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