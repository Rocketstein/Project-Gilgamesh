#pragma once

#include <typeindex>
#include <unordered_map>
#include <vector>

#include "Entity.h"

// Component Storage Interface
class IComponentStorage
{
public:
	virtual ~IComponentStorage() = default;
	virtual void Remove(Entity entity) = 0;
	virtual bool Has(Entity entity) = 0;
};

// Component Storage By Type
template <typename T>
class ComponentStorage : IComponentStorage
{
public:
	void Remove(Entity entity) override;
	bool Has(Entity entity) override;

private:
	// Sparse set storage goes here
};

class EntityRegistry final
{
public:
	EntityRegistry() = default;

	// =========================================
	// Entity Lifecycle
	// =========================================
	Entity Create();

	void Destroy(Entity entity);

	bool IsValid(Entity entity);

	// =========================================
	// Component Management
	// =========================================



private:
	std::vector<Entity> entities_;
	std::unordered_map<std::type_index, IComponentStorage> storages_;

};