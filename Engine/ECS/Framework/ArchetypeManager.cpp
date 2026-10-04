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
	if (const auto existing = archetypes_.find(signature); existing != archetypes_.end())
		return existing->second;

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

	// The first argument is the map key; later arguments construct Archetype in place.
	const auto result = archetypes_.try_emplace(
		signature, signature, std::move(componentMetadata));
	return result.first->second;
}

Archetype* ArchetypeManager::Find(std::span<const ComponentTypeId> componentTypes)
{
	// Use the same canonical key as FindOrCreate without requiring a default constructor.
	ArchetypeSignature signature = CanonicalizeSignature(componentTypes);
	const auto it = archetypes_.find(signature);
	return it != archetypes_.end() ? &it->second : nullptr;
}

const Archetype* ArchetypeManager::Find(std::span<const ComponentTypeId> componentTypes) const
{
	// Look up the stored archetype without calling this const overload again.
	ArchetypeSignature signature = CanonicalizeSignature(componentTypes);
	const auto it = archetypes_.find(signature);
	return it != archetypes_.end() ? &it->second : nullptr;
}

void ArchetypeManager::EraseEntity(Entity entity)
{

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
