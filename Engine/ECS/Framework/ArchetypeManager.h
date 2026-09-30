#pragma once

#include <map>
#include <span>
#include <typeindex>
#include <vector>

#include "Archetype.h"
#include "Engine/ECS/Entity/Entity.h"

// Owns one archetype for each distinct set of component types.
class ArchetypeManager final
{
public:
	ArchetypeManager() = default;
	ArchetypeManager(const ArchetypeManager&) = delete;
	ArchetypeManager& operator=(const ArchetypeManager&) = delete;
	ArchetypeManager(ArchetypeManager&&) = delete;
	ArchetypeManager& operator=(ArchetypeManager&&) = delete;

	// Normalize the type set and return its existing archetype, or create it once.
	// The order of input types and duplicate entries do not affect identity.
	Archetype& FindOrCreate(std::span<const ComponentTypeId> componentTypes);

	// Return nullptr when this set of types has no archetype yet.
	[[nodiscard]] Archetype* Find(std::span<const ComponentTypeId> componentTypes);
	[[nodiscard]] const Archetype* Find(std::span<const ComponentTypeId> componentTypes) const;

	// Erase an entity's row and repair the location of any entity moved into it.
	// The registry calls this before invalidating a handle, once row storage exists.
	void EraseEntity(Entity entity);

private:
	// Sort and remove duplicate types before looking up a signature.
	[[nodiscard]] static ArchetypeSignature CanonicalizeSignature(
		std::span<const ComponentTypeId> componentTypes);

	// Map keys compare complete signatures; map nodes keep archetype addresses stable.
	std::map<ArchetypeSignature, Archetype> archetypes_;
};
