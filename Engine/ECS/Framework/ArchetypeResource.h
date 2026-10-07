#pragma once

#include <cstddef>
#include <typeindex>
#include <unordered_map>
#include <vector>

struct Archetype;

// Remember matching archetypes and progress through the creation list.
struct QueryCache
{
	// Existing archetypes selected by this component combination.
	std::vector<Archetype*> matches;

	// Number of created archetypes already checked against the combination.
	std::size_t checkedArchetypeCount = 0;
};

// Describes a component's storage requirements and object lifetime operations.
struct ComponentTypeInfo
{
	// Bytes occupied by one component value.
	std::size_t size;
	// Alignment required by one component value.
	std::size_t alignment;
	// Move-construct a value in uninitialized storage without throwing.
	void (*moveConstruct)(void* destination, void* source) noexcept;
	// End the lifetime of a constructed value without throwing.
	void (*destroy)(void* address) noexcept;
};

using ComponentTypeId	 = std::type_index;
using ArchetypeSignature = std::vector<ComponentTypeId>;
using ComponentTypes	 = std::unordered_map<ComponentTypeId, ComponentTypeInfo>;
