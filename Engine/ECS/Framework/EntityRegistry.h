#pragma once

#include <cstdint>
#include <vector>

#include "Engine/ECS/Entity/Entity.h"
#include "Engine/ECS/Framework/ArchetypeManager.h"

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

	// Component operations become available when archetype storage is implemented.
	// Keep unsupported calls explicit instead of leaving templates that use
	// the removed per-type storage.
	template <typename T, typename... Args>
	T& Add(Entity entity, Args&&... args) = delete;

	template <typename T>
	T& Get(Entity entity) = delete;

	template <typename T>
	bool Has(Entity entity) const = delete;

	template <typename T>
	void Remove(Entity entity) = delete;

	template <class... Components, class Fn>
	void ForEach(Fn&& fn) = delete;


private:
	struct EntitySlot
	{
		// The generation advances when an entity is destroyed, before reuse.
		std::uint32_t generation = 0;
	};

	std::vector<std::uint32_t> freeIndices_;
	std::vector<EntitySlot> slots_;
	// Own the future component storage behind the registry's entity API.
	ArchetypeManager archetypeManager_;
	std::uint64_t ownerId_ = 0;

};
