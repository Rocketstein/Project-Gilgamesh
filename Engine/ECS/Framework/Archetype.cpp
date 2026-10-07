#include "Archetype.h"

#include <algorithm>
#include <bit>
#include <stdexcept>
#include <utility>

#include "Engine/ECS/Entity/Entity.h"

namespace
{
	// Place one SoA column without exceeding the fixed chunk allocation.
	bool TryPlaceColumn(
		std::size_t rows,
		std::size_t elementSize,
		std::size_t alignment,
		std::size_t& cursor,
		std::size_t& columnOffset)
	{
		const std::size_t remainder = cursor % alignment;
		const std::size_t padding = remainder == 0 ? 0 : alignment - remainder;
		if (padding > CHUNK_SIZE - cursor)
			return false;
		cursor += padding;

		if (rows > (CHUNK_SIZE - cursor) / elementSize)
			return false;
		columnOffset = cursor;
		cursor += rows * elementSize;
		return true;
	}

	// Build one complete layout before any chunk can reference it.
	ChunkLayout BuildChunkLayout(
		const ArchetypeSignature& signature,
		const ComponentTypes& componentTypes)
	{
		// Element sizes give an upper bound on capacity before alignment padding.
		std::size_t bytesPerRow = sizeof(Entity);
		std::size_t allocationAlignment = alignof(Entity);
		for (const ComponentTypeId& typeId : signature)
		{
			const auto typeIt = componentTypes.find(typeId);
			if (typeIt == componentTypes.end())
				throw std::invalid_argument("Archetype signature is missing component metadata");

			const ComponentTypeInfo& info = typeIt->second;
			if (info.size == 0 || !std::has_single_bit(info.alignment) ||
				info.size % info.alignment != 0)
			{
				throw std::invalid_argument("Archetype component size or alignment is invalid");
			}
			if (info.size > CHUNK_SIZE - bytesPerRow)
				throw std::length_error("Archetype component row exceeds chunk size");

			bytesPerRow += info.size;
			allocationAlignment = std::max(allocationAlignment, info.alignment);
		}

		// Try a row count, placing Entity first and then components in signature order.
		auto tryLayout = [&](std::size_t rows, ChunkLayout* output)
			{
				std::size_t cursor = 0;
				std::size_t offset = 0;
				if (!TryPlaceColumn(rows, sizeof(Entity), alignof(Entity), cursor, offset))
					return false;
				if (output)
					output->entityOffset = offset;

				for (const ComponentTypeId& typeId : signature)
				{
					const ComponentTypeInfo& info = componentTypes.at(typeId);
					if (!TryPlaceColumn(rows, info.size, info.alignment, cursor, offset))
						return false;
					if (output)
						output->componentColumns.emplace(typeId, ChunkLayout::Column{ offset, info.size });
				}
				return true;
			};

		// The first count that fits while descending is the maximum chunk capacity.
		std::size_t capacity = CHUNK_SIZE / bytesPerRow;
		while (capacity > 0 && !tryLayout(capacity, nullptr))
			--capacity;
		if (capacity == 0)
			throw std::length_error("Archetype cannot fit one entity row in a chunk");

		// Store the offsets for the final capacity, not for an earlier candidate.
		ChunkLayout layout{};
		layout.capacity = static_cast<std::uint32_t>(capacity);
		layout.allocationAlignment = allocationAlignment;
		layout.componentColumns.reserve(signature.size());
		if (!tryLayout(capacity, &layout))
			throw std::logic_error("Archetype layout calculation changed unexpectedly");
		return layout;
	}
} // Anonymous namespace

Archetype::Archetype(ArchetypeSignature inSignature, ComponentTypes inComponentTypes)
	: signature(std::move(inSignature)),
	  componentTypes(std::move(inComponentTypes)),
	  layout(BuildChunkLayout(signature, componentTypes))
{
}

Chunk* Archetype::FindOrCreateUsableChunk()
{
	if (!availableChunks.empty())
	{
		return availableChunks.back();
	}

	// Reserve the availability entry before publishing ownership of a new chunk.
	availableChunks.reserve(chunks.size() + 1);
	std::unique_ptr<Chunk> newChunk = std::make_unique<Chunk>(layout, componentTypes);
	Chunk* result = newChunk.get();
	chunks.push_back(std::move(newChunk));
	availableChunks.push_back(result);

	return result;
}

std::optional<Entity> Archetype::EraseRow(Chunk& chunk, std::uint32_t row)
{
	// A formerly full chunk becomes available after one successful erase.
	const bool wasFull = chunk.IsFull();
	if (wasFull)
	{
		availableChunks.reserve(chunks.size());
	}

	std::optional<Entity> displaced = chunk.Erase(row);
	if (wasFull)
	{
		availableChunks.push_back(&chunk);
	}
	return displaced;
}

bool Archetype::HasComponent(ComponentTypeId typeId) const
{
	return std::find(signature.begin(), signature.end(), typeId) != signature.end();
}
