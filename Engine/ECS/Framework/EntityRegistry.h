#pragma once

#include <cassert>
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

	// =============================================================================
	// Entity Lifecycle
	// =============================================================================

	[[nodiscard]] Entity Create();

	[[nodiscard]] bool Destroy(Entity entity);

	[[nodiscard]] bool IsValid(Entity entity) const noexcept;

	// =============================================================================
	// Component Operations
	// =============================================================================
	// Construct T in a new archetype row and migrate the entity's existing values.
	// Component constructors must not structurally mutate this registry.
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
		T* value = nullptr;
		MoveEntityToArchetype(
			entity,
			source,
			target,
			[&](Chunk::PendingRow& pending)
			{
				// Construct T before moving old values: user arguments may throw.
				value = &pending.Construct<T>(std::forward<Args>(args)...);
			});
		return *value;
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

		const EntitySlot source = slots_[entity.index_];
		if (source.chunk == nullptr)
		{
			throw std::logic_error("Entity has a component signature but no chunk row");
		}

		// Keep only the component types that will exist in the destination row.
		ArchetypeSignature signature = source.archetype->signature;
		std::erase(signature, ComponentTypeId{ typeid(T) });

		if (signature.empty())
		{
			// A componentless entity has no archetype row in this registry.
			if (const auto displaced = source.archetype->EraseRow(*source.chunk, source.row))
			{
				slots_[displaced->index_].row = source.row;
			}
			EntitySlot& slot = slots_[entity.index_];
			slot.archetype = nullptr;
			slot.chunk = nullptr;
			slot.row = 0;
			return;
		}

		Archetype& target = archetypeManager_.FindOrCreate(signature);
		MoveEntityToArchetype(entity, source, target, [](Chunk::PendingRow&) {});
	}

	template <class... Components, class Fn>
	void ForEach(Fn&& fn)
	{
		static_assert(sizeof...(Components) > 0,
			"ForEach() requires at least one component type");

		auto& cache = archetypeManager_.GetQueryCache<Components...>();
		auto& archetypes = cache.matches;
		for (auto* archetype : archetypes)
		{
			// Iterate chunks
			for (auto& chunkPtr : archetype->GetChunks())
			{
				Chunk& chunk = *chunkPtr;
				const std::uint32_t count = chunk.Count();
				for (std::uint32_t row = 0; row < count; ++row)
				{
					fn(chunk.Get<Components>(row)...);
				}
			}
		}
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

	// Populate new columns, move shared columns, then publish the new row location.
	// The target must differ from the source; populate only constructs new values.
	template <typename Populate>
	void MoveEntityToArchetype(
		Entity entity,
		EntitySlot source,
		Archetype& target,
		Populate&& populate)
	{
		assert(source.archetype != &target);
		const auto [destination, newRow] = target.InsertRow(
			entity,
			[&](Chunk::PendingRow& pending)
			{
				// Construct new values before moving any source component.
				std::forward<Populate>(populate)(pending);

				// The common types survive; types absent from target are removed.
				if (source.chunk != nullptr)
				{
					for (ComponentTypeId type : source.archetype->signature)
					{
						if (target.HasComponent(type))
						{
							pending.MoveFrom(type, *source.chunk, source.row);
						}
					}
				}
			});

		// Erase only after the destination is complete and repair a source swap.
		if (source.chunk != nullptr)
		{
			if (const auto displaced = source.archetype->EraseRow(*source.chunk, source.row))
			{
				slots_[displaced->index_].row = source.row;
			}
		}

		// Publish the destination location after retiring the source row.
		EntitySlot& slot = slots_[entity.index_];
		slot.archetype = &target;
		slot.chunk = destination;
		slot.row = newRow;
	}

private:
	std::vector<std::uint32_t> freeIndices_;
	std::vector<EntitySlot> slots_;
	// Own the future component storage behind the registry's entity API.
	ArchetypeManager archetypeManager_;
	std::uint64_t ownerId_ = 0;
};
