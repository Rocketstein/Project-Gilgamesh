#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "Chunk.h"

struct Archetype
{
	ArchetypeSignature signature;
	ComponentTypes componentTypes;

	std::vector<std::unique_ptr<Chunk>> chunks;

	template <typename T>
	void RegisterComponent()
	{
		const ComponentTypeId typeID = typeid(T);
		componentTypes.try_emplace(typeID, { sizeof(T), alignof(T) });
	}

	bool HasComponent(ComponentTypeId typeId) const {
		return std::find(signature.begin(), signature.end(), typeId) != signature.end();
	}
};