#pragma once

#include <cstdint>
#include <stdexcept>
#include <typeinfo>
#include <utility>
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

	[[nodiscard]] Entity Create();

	[[nodiscard]] bool Destroy(Entity entity);

	[[nodiscard]] bool IsValid(Entity entity) const noexcept;

	template <typename T, typename... Args>
	T& Emplace(Entity entity, Args&&... args)
	{
		if (!IsValid(entity) || Has<T>(entity))
		{
			throw std::logic_error("Cannot emplace T on this entity");
		}

		archetypeManager_.RegisterComponent<T>();

		// Keep the old location until migration finishes.
		const EntitySlot source = slots_[entity.index_];
		ArchetypeSignature signature =
			source.archetype ? source.archetype->signature : ArchetypeSignature{};
		signature.emplace_back(typeid(T));

		Archetype& target = archetypeManager_.FindOrCreate(signature);
		Chunk* destination = target.FindOrCreateUsableChunk();
		auto pending = destination->BeginRow(entity);               // Planned API

		// Construct T first: its constructor may throw while the source is intact.
		T& value = pending.Construct<T>(std::forward<Args>(args)...);

		// Construct every existing component in the destination row.
		if (source.chunk != nullptr)
		{
			for (ComponentTypeId type : source.archetype->signature)
			{
				const auto& info = source.archetype->componentTypes.at(type);
				info.moveConstruct(
					pending.ComponentAddress(type),
					source.chunk->ComponentAddress(type, source.row));
				pending.MarkConstructed(type);
			}
		}

		// Publish the complete row, then remove the old one.
		const std::uint32_t newRow = pending.Commit();
		if (source.chunk != nullptr)
		{
			if (const auto displaced = source.archetype->EraseRow(*source.chunk, source.row))
			{
				slots_[displaced->index_].row = source.row;
			}
		}

		EntitySlot& slot = slots_[entity.index_];
		slot.archetype = &target;
		slot.chunk = destination;
		slot.row = newRow;
		return value;
	}

	template <typename T>
	bool Has(Entity entity) const
	{
		// A stale or foreign handle cannot refer to a component row.
		if (!IsValid(entity))
		{
			return false;
		}

		// An entity without a row has no components to query.
		const EntitySlot& slot = slots_[entity.index_];
		return slot.archetype != nullptr &&
			slot.archetype->HasComponent(ComponentTypeId{ typeid(T) });
	}

	// Return T from the entity's chunk row, or reject a missing component.
	template <typename T>
	T& Get(Entity entity)
	{
		// A reference can only be returned for a live entity that owns T.
		if (!Has<T>(entity))
		{
			throw std::out_of_range("Entity does not have the requested component");
		}

		// The slot supplies the chunk and the shared row index for all columns.
		const EntitySlot& slot = slots_[entity.index_];
		if (slot.chunk == nullptr)
		{
			throw std::logic_error("Entity has an archetype but no chunk row");
		}
		return slot.chunk->Get<T>(slot.row);
	}

	template <typename T>
	void Remove(Entity entity)
	{
		if (!Has<T>(entity))
		{
			return;
		}

		ArchetypeSignature signature;
		if (slots_[entity.index_].archetype != nullptr)
		{
			signature = slots_[entity.index_].archetype->signature;
			archetypeManager_.EraseEntity(entity);
		}
		signature.push_back(ComponentTypeId{ typeid(T) });

		Archetype& archetype = archetypeManager_.FindOrCreate(signature);
		slots_[entity.index_].archetype = &archetype;
	}

	template <class... Components, class Fn>
	void ForEach(Fn&& fn)
	{

	}


private:
	// Tracks one handle generation and its optional archetype row location.
	struct EntitySlot
	{
		// The generation advances when an entity is destroyed, before reuse.
		std::uint32_t generation = 0;
		// The archetype owning this entity's row.
		Archetype* archetype = nullptr;
		// The chunk containing the row.
		Chunk* chunk = nullptr;
		// The shared index into the chunk's Entity and component columns.
		std::uint32_t row = 0;
	};

	std::vector<std::uint32_t> freeIndices_;
	std::vector<EntitySlot> slots_;
	// Own the future component storage behind the registry's entity API.
	ArchetypeManager archetypeManager_;
	std::uint64_t ownerId_ = 0;
};
