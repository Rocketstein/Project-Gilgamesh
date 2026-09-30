#include "ArchetypeManager.h"

#include <algorithm>

Archetype& ArchetypeManager::FindOrCreate(std::span<const ComponentTypeId> componentTypes)
{
	// Canonicalize component groupings
	Signature signature = CanonicalizeSignature(componentTypes);

	// Check if the component grouping already exists; if true, return.
	if (archetypes_.count(signature))
		return archetypes_[signature];

	// Register new component grouping as an archetype
	
	// Create Archetype
	Archetype archetype = {};
	archetypes_[signature] = archetype;
	return archetype;
}

Archetype* ArchetypeManager::Find(std::span<const ComponentTypeId> componentTypes)
{
	// Canonicalize component groupings
	Signature signature = CanonicalizeSignature(componentTypes);

	// Check if the component grouping already exists; if true, return.
	if (archetypes_.count(signature))
		return &archetypes_[signature];

	return nullptr;
}

const Archetype* ArchetypeManager::Find(std::span<const ComponentTypeId> componentTypes) const
{
	const Archetype* archetype = Find(componentTypes);
	return archetype;
}

void ArchetypeManager::EraseEntity(Entity entity)
{

}

ArchetypeManager::Signature ArchetypeManager::CanonicalizeSignature(
	std::span<const ComponentTypeId> componentTypes)
{
	Signature signature(componentTypes.begin(), componentTypes.end());

	std::sort(signature.begin(), signature.end());
	signature.erase(
		std::unique(signature.begin(), signature.end()),
		signature.end());

	return signature;
}