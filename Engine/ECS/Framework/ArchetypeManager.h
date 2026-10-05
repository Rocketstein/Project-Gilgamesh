#pragma once

#include <map>
#include <memory>
#include <new>
#include <span>
#include <typeindex>
#include <typeinfo>
#include <type_traits>
#include <utility>
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

	// Register storage metadata while the C++ component type is known.
	template <typename T>
	void RegisterComponent()
	{
		static_assert(std::is_nothrow_move_constructible_v<T>,
			"Components must be nothrow move constructible");
		static_assert(std::is_nothrow_destructible_v<T>,
			"Components must be nothrow destructible");

		// Preserve T's move and destruction operations for runtime archetype rows.
		const ComponentTypeId typeId{ typeid(T) };
		registeredComponentTypes_.try_emplace(
			typeId, ComponentTypeInfo{
				sizeof(T),
				alignof(T),
				[](void* destination, void* source) noexcept
				{
					std::construct_at(static_cast<T*>(destination),
						std::move(*std::launder(static_cast<T*>(source))));
				},
				[](void* address) noexcept
				{
					std::destroy_at(std::launder(static_cast<T*>(address)));
				}
			});
	}

	// Normalize the type set and return its existing archetype, or create it once.
	// The order of input types and duplicate entries do not affect identity.
	// Every type must be registered before a new archetype is created.
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
	// Registered size and alignment information is shared by all archetypes.
	ComponentTypes registeredComponentTypes_;
};
