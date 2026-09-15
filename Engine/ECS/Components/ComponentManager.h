#pragma once
#include <unordered_map>

class ComponentManager final
{
public:
	ComponentManager() = default;

	// Registers a component type with the manager, where a hash key is generated from the component type's name.
	void RegisterComponent(void* componentStorage);

private:
	std::unordered_map<size_t, void*> componentStorage_;

};