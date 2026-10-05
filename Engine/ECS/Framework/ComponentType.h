#pragma once

#include <cstddef>
#include <typeindex>
#include <unordered_map>
#include <vector>

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
