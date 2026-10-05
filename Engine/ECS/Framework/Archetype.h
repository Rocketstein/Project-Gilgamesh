#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "Chunk.h"
#include "ComponentType.h"

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

	// Reuse a nonfull chunk or allocate one for this archetype.
	Chunk* FindOrCreateUsableChunk();

	// Erase a known row in one of this archetype's chunks and report a swap.
	[[nodiscard]] std::optional<Entity> EraseRow(Chunk& chunk, std::uint32_t row);

	// The exact set of component types shared by every entity in this archetype.
	const ArchetypeSignature signature;
	// Size and alignment metadata for each component type in the signature.
	const ComponentTypes componentTypes;
	// Layout metadata is shared by every chunk of this archetype.
	const ChunkLayout layout;

	// Nonfull chunks are selected from the back for constant-time insertion.
	std::vector<Chunk*> availableChunks;
	// Chunks must be destroyed before the metadata they reference.
	std::vector<std::unique_ptr<Chunk>> chunks;
};
