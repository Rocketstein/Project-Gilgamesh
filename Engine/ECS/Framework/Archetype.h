#pragma once

#include <memory>
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

	// The exact set of component types shared by every entity in this archetype.
	const ArchetypeSignature signature;
	// Size and alignment metadata for each component type in the signature.
	const ComponentTypes componentTypes;
	// Chunks must be destroyed before the metadata they reference.
	std::vector<std::unique_ptr<Chunk>> chunks;
};
