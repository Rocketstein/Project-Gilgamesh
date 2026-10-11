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

	// Reject oversized requests before advancing through retained pages.
	if (size > pageSize_)
	{
		return nullptr;
	}

	// Reuse retained pages before allocating additional storage.
	while (pageIndex_ < pages_.size())
	{
		Page& page = pages_[pageIndex_];
		void* currentAddress = page.data.get() + page.offset;
		std::size_t remainingSize = page.capacity - page.offset;

		if (std::align(alignment, size, currentAddress, remainingSize))
		{
			page.offset = page.capacity - remainingSize + size;
			return currentAddress;
		}

		++pageIndex_;
	}

	// Add only one page and stop if the request still cannot be aligned.
	AllocateNewPage();
	pageIndex_ = pages_.size() - 1;
	Page& page = pages_[pageIndex_];
	void* currentAddress = page.data.get();
	std::size_t remainingSize = page.capacity;

	if (!std::align(alignment, size, currentAddress, remainingSize))
	{
		return nullptr;
	}

	page.offset = page.capacity - remainingSize + size;
	return currentAddress;
}

void MemoryArena::Reset()
{
	for (auto& page : pages_)
	{
		page.offset = 0;
	}

	pageIndex_ = 0;
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
