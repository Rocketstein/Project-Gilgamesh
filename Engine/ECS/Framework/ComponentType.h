#pragma once

#include <cstddef>
#include <typeindex>
#include <unordered_map>
#include <vector>

struct ComponentTypeInfo
{
	std::size_t size;
	std::size_t alignment;
};

using ComponentTypeId	 = std::type_index;
using ArchetypeSignature = std::vector<ComponentTypeId>;
using ComponentTypes	 = std::unordered_map<ComponentTypeId, ComponentTypeInfo>;
