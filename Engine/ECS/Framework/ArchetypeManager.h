#pragma once

#include <array>
#include <cstddef>
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

	template <class... Components>
	[[nodiscard]] QueryCache& GetQueryCache()
	{
		static_assert(sizeof...(Components) > 0,
			"GetQueryCache() requires at least one component type");
		const ArchetypeSignature& cache = GetQueryCombination<Components...>();
		UpdateQueryCache(cache, queryCaches_[cache]);
		return queryCaches_[cache];
	}

	// Normalize the type set and return its existing archetype, or create it once.
	// The order of input types and duplicate entries do not affect identity.
	// Every type must be registered before a new archetype is created.
	Archetype& FindOrCreate(std::span<const ComponentTypeId> componentTypes);

	// Return nullptr when this set of types has no archetype yet.
	[[nodiscard]] Archetype* Find(std::span<const ComponentTypeId> componentTypes);
	[[nodiscard]] const Archetype* Find(std::span<const ComponentTypeId> componentTypes) const;

private:
	// Resolve a stored archetype with a checked index access.
	[[nodiscard]] Archetype& GetArchetype(std::size_t index);
	[[nodiscard]] const Archetype& GetArchetype(std::size_t index) const;

	// Sort and remove duplicate types before looking up a signature.
	[[nodiscard]] static ArchetypeSignature CanonicalizeSignature(
		std::span<const ComponentTypeId> componentTypes);

	void UpdateQueryCache(
		const ArchetypeSignature& required,
		QueryCache& cache);

	// Return the normalized query key shared by this component type pack.
	template <class... Components>
	const ArchetypeSignature& GetQueryCombination()
	{
		// Build the immutable key once, on first use. Uses magic static.
		static const std::array<ComponentTypeId, sizeof...(Components)> types{
			ComponentTypeId{ typeid(Components) }...
		};

		static const ArchetypeSignature combination =
			CanonicalizeSignature(types);

		return combination;
	}

private:
	// Cache query results for each unique canonical component combination.
	std::map<ArchetypeSignature, QueryCache> queryCaches_;

	// Canonical signatures locate their archetypes in the owning storage.
	std::map<ArchetypeSignature, std::size_t> archetypeIndices_;

	// Append-only ownership preserves creation order and stable archetype addresses.
	std::vector<std::unique_ptr<Archetype>> archetypeStorage_;

	// Registered size and alignment information is shared by all archetypes.
	ComponentTypes registeredComponentTypes_;
};
