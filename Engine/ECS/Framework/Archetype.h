#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Chunk.h"
#include "ArchetypeResource.h"

// Groups chunks whose entities have the same component signature.
struct Archetype
{
	// Keep the signature and its component metadata together for every chunk.
	Archetype(ArchetypeSignature inSignature, ComponentTypes inComponentTypes);
	Archetype(const Archetype&) = delete;
	Archetype& operator=(const Archetype&) = delete;
	Archetype(Archetype&&) = delete;
	Archetype& operator=(Archetype&&) = delete;

	// Check whether this signature contains the requested component type.
	bool HasComponent(ComponentTypeId typeId) const;

	std::vector<std::unique_ptr<Chunk>>& GetChunks() { return chunks; }

	// Open a row in an available chunk, populate every column, then publish it.
	// The populate callback must not structurally mutate this archetype.
	template <typename Populate>
	[[nodiscard]] std::pair<Chunk*, std::uint32_t> InsertRow(
		Entity entity, Populate&& populate)
	{
		Chunk* chunk = FindOrCreateUsableChunk();
		auto pending = chunk->BeginRow(entity);

		// A failed initializer leaves the row uncommitted for PendingRow to clean up.
		std::forward<Populate>(populate)(pending);
		if (!pending.IsComplete())
		{
			throw std::logic_error("Archetype insertion left component columns uninitialized");
		}

		// Commit and availability changes cannot fail after a complete row exists.
		const std::uint32_t row = pending.Commit();
		if (chunk->IsFull())
		{
			assert(!availableChunks.empty() && availableChunks.back() == chunk);
			availableChunks.pop_back();
		}
		return { chunk, row };
	}

	// Erase a known row in one of this archetype's chunks and report a swap.
	std::optional<Entity> EraseRow(Chunk& chunk, std::uint32_t row);

	// The exact set of component types shared by every entity in this archetype.
	const ArchetypeSignature signature;
	// Size and alignment metadata for each component type in the signature.
	const ComponentTypes componentTypes;
	// Layout metadata is shared by every chunk of this archetype.
	const ChunkLayout layout;

private:
	// Reuse a nonfull chunk or allocate one for this archetype.
	Chunk* FindOrCreateUsableChunk();

	// Nonfull chunks are selected from the back for constant-time insertion.
	std::vector<Chunk*> availableChunks;
	// Chunks must be destroyed before the metadata they reference.
	std::vector<std::unique_ptr<Chunk>> chunks;
};
