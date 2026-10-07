#include "ArchetypeManager.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "Engine/Core/Logging/Logger.h"

Archetype& ArchetypeManager::FindOrCreate(std::span<const ComponentTypeId> componentTypes)
{
	// Use one key for every ordering of the same component types.
	ArchetypeSignature signature = CanonicalizeSignature(componentTypes);

	// Existing archetypes already own the metadata for their signatures.
	if (const auto existing = archetypeIndices_.find(signature); existing != archetypeIndices_.end())
		return GetArchetype(existing->second);

	// Resolve every component before inserting
	ComponentTypes componentMetadata;
	componentMetadata.reserve(signature.size());
	for (const ComponentTypeId& typeId : signature)
	{
		const auto registered = registeredComponentTypes_.find(typeId);
		if (registered == registeredComponentTypes_.end())
		{
			GILGAMESH_LOG(Core, Error,
				"Cannot create archetype: component type '{}' is not registered", typeId.name());
			throw std::logic_error("Cannot create archetype with an unregistered component type");
		}
		componentMetadata.emplace(typeId, registered->second);
	}

	// Finish construction before publishing the signature's storage index.
	auto archetype = std::make_unique<Archetype>(signature, std::move(componentMetadata));
	const std::size_t index = archetypeStorage_.size();
	const auto [entry, inserted] = archetypeIndices_.try_emplace(std::move(signature), index);
	if (!inserted)
		return GetArchetype(entry->second);

	// Roll back the index if the owning vector cannot accept the new archetype.
	try
	{
		archetypeStorage_.push_back(std::move(archetype));
	}
	catch (...)
	{
		archetypeIndices_.erase(entry);
		throw;
	}
	return GetArchetype(index);
}

Archetype* ArchetypeManager::Find(std::span<const ComponentTypeId> componentTypes)
{
	// Use the same canonical key as FindOrCreate without requiring a default constructor.
	ArchetypeSignature signature = CanonicalizeSignature(componentTypes);
	const auto it = archetypeIndices_.find(signature);
	return it != archetypeIndices_.end() ? &GetArchetype(it->second) : nullptr;
}

const Archetype* ArchetypeManager::Find(std::span<const ComponentTypeId> componentTypes) const
{
	// Look up the stored archetype without calling this const overload again.
	ArchetypeSignature signature = CanonicalizeSignature(componentTypes);
	const auto it = archetypeIndices_.find(signature);
	return it != archetypeIndices_.end() ? &GetArchetype(it->second) : nullptr;
}

Archetype& ArchetypeManager::GetArchetype(std::size_t index)
{
	// Invalid indices are rejected instead of accessing outside the vector.
	return *archetypeStorage_.at(index);
}

const Archetype& ArchetypeManager::GetArchetype(std::size_t index) const
{
	// Keep read-only lookups subject to the same bounds check.
	return *archetypeStorage_.at(index);
}

ArchetypeSignature ArchetypeManager::CanonicalizeSignature(
	std::span<const ComponentTypeId> componentTypes)
{
	ArchetypeSignature signature(componentTypes.begin(), componentTypes.end());

	std::sort(signature.begin(), signature.end());
	signature.erase(
		std::unique(signature.begin(), signature.end()),
		signature.end());

	return signature;
}

void ArchetypeManager::UpdateQueryCache(
	const ArchetypeSignature& required,
	QueryCache& cache)
{
	const std::size_t totalCount = archetypeStorage_.size();

	// The saved count is also the index of the next unchecked archetype.
	while (cache.checkedArchetypeCount < totalCount)
	{
		Archetype* archetype =
			archetypeStorage_[cache.checkedArchetypeCount].get();

		// Match archetypes containing every required component.
		if (std::includes(
			archetype->signature.begin(),
			archetype->signature.end(),
			required.begin(),
			required.end()))
		{
			cache.matches.push_back(archetype);
		}

		// Count rejected archetypes as examined too.
		++cache.checkedArchetypeCount;
	}
}