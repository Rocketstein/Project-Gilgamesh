#pragma once

#include <memory>
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
	virtual bool const Has(Entity entity) = 0;
};

// Component Storage By Type
template <typename T>
class ComponentStorage : public IComponentStorage
{
public:
	void Remove(Entity entity) override;
	bool Has(Entity entity) const override;

private:
	// Component storage goes here
	
};

// Issues entity handles and owns the generations used to validate them.
class EntityRegistry final
{
public:
	EntityRegistry() noexcept;
	EntityRegistry(const EntityRegistry&) = delete;
	EntityRegistry& operator=(const EntityRegistry&) = delete;
	EntityRegistry(EntityRegistry&&) = delete;
	EntityRegistry& operator=(EntityRegistry&&) = delete;

	// =========================================
	// Entity Lifecycle
	// =========================================
	[[nodiscard]] Entity Create();

	[[nodiscard]] bool Destroy(Entity entity);

	[[nodiscard]] bool IsValid(Entity entity) const noexcept;

	// =========================================
	// Component Management
	// =========================================
	template <typename T, typename... Args>
	T& Add(Entity entity, Args&&... args) {
		auto& storage = GetStorage<T>();
		storage.data[entity] = T{ std::forward<Args>(args)... };
		return storage.data[entity];
	}

	template <typename T>
	T& Get(Entity entity) {
		return GetStorage<T>().data.at(entity);
	}

	template <typename T>
	bool Has(Entity entity) {
		return GetStorage<T>().Has(entity);
	}

	template <typename T>
	void Remove(Entity entity) {
		GetStorage<T>().Remove(entity);
	}

	template<class... Components, class Fn>
	void ForEach(Fn&& fn);


private:
	struct EntitySlot
	{
		// The generation advances when an entity is destroyed, before reuse.
		std::uint32_t generation = 0;
	};

	std::vector<std::uint32_t> freeIndices_;
	std::vector<EntitySlot> slots_;
	std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> storages_;
	std::uint64_t ownerId_ = 0;

};
