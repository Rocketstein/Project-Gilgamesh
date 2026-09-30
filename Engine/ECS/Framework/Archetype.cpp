#include "Archetype.h"

#include <algorithm>
#include <utility>

Archetype::Archetype(ArchetypeSignature inSignature, ComponentTypes inComponentTypes)
	: signature(std::move(inSignature)), componentTypes(std::move(inComponentTypes))
{
}

bool Archetype::HasComponent(ComponentTypeId typeId) const
{
	return std::find(signature.begin(), signature.end(), typeId) != signature.end();
}
