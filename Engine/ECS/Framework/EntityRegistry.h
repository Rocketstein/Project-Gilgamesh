#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <typeinfo>
#include <utility>
#include <vector>

#include "ArchetypeManager.h"
#include "EntitySlot.h"
#include "QueryTerm.h"
#include "Engine/ECS/Entity/Entity.h"

// Issues entity handles and owns the generations used to validate them.
class EntityRegistry final
{
public:
	// Bind issued handles to a world while retaining a distinct registry identity.
	explicit EntityRegistry(std::uint32_t owningWorldID = 0) noexcept;
	EntityRegistry(const EntityRegistry&) = delete;
	EntityRegistry& operator=(const EntityRegistry&) = delete;
	EntityRegistry(EntityRegistry&&) = delete;
	EntityRegistry& operator=(EntityRegistry&&) = delete;

	// =============================================================================
	// Entity Lifecycle
	// =============================================================================

	[[nodiscard]] Entity Create();

	[[nodiscard]] Entity Reserve();

	// Cancel a reservation and recycle its slot without allocating.
	void CancelEntityReservation(Entity entity) noexcept;

	void Commit(Entity entity);

	bool Destroy(Entity entity);

	[[nodiscard]] EntityState GetEntityState(Entity entity) const noexcept;

	// =============================================================================
	// Component Operations
	// =============================================================================
	// Construct T in a new archetype row and migrate the entity's existing values.
	// Component constructors must not structurally mutate this registry.
	template <typename T, typename... Args>
	T& Emplace(Entity entity, Args&&... args)
	{
		if (GetEntityState(entity) != EntityState::Alive || Has<T>(entity))
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
		if (GetEntityState(entity) != EntityState::Alive)
		{
			return false;
		}

		// An entity without a row has no components to query.
		const EntitySlot& slot = slots_[entity.index_];
		return slot.archetype != nullptr &&
			slot.archetype->HasComponent(ComponentTypeId{ typeid(T) });
	}

	// Mutable T publishes a possible write; const T is read-only. Reacquire mutable
	// references for each write phase instead of retaining them across system evaluation.
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
		if constexpr (std::is_const_v<T>)
		{
			return std::as_const(*slot.chunk).Get<T>(slot.row);
		}
		else
		{
			// Resolve successfully before publishing the mutable borrow.
			T& value = slot.chunk->Get<T>(slot.row);
			slot.chunk->MarkChanged(ComponentTypeId{ typeid(T) }, NextChangeVersion());
			return value;
		}
	}

	template <typename T>
	const T& Get(Entity entity) const
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
		return std::as_const(*slot.chunk).Get<T>(slot.row);
	}

	// Current write stamp; consumers retain their own observation point.
	[[nodiscard]] std::uint64_t GetChangeVersion() const noexcept { return changeVersion_; }

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
			StampStructuralChange(source.chunk, nullptr);
			return;
		}

		Archetype& target = archetypeManager_.FindOrCreate(signature);
		MoveEntityToArchetype(entity, source, target, [](Chunk::PendingRow&) {});
	}

	// Copy live committed handles; no types includes componentless entities, otherwise require all types.
	// The snapshot has no ordering guarantee; revalidate retained handles after structural changes.
	template <typename... Components>
	[[nodiscard]] std::vector<Entity> QueryEntities()
	{
		std::vector<Entity> entities;
		if constexpr (sizeof...(Components) == 0)
		{
			// Componentless entities have no archetype row, so enumerate live slots directly.
			for (std::size_t index = 0; index < slots_.size(); ++index)
			{
				if (slots_[index].state != EntityState::Alive) continue;
				entities.push_back(Entity(
					static_cast<std::uint32_t>(index),
					slots_[index].generation,
					ownerId_,
					owningWorldID_));
			}
		}
		else
		{
			// The cache selects archetypes; current chunk rows supply matching entity identities.
			const auto& cache = archetypeManager_.GetQueryCache<Components...>();
			for (auto* archetype : cache.matches)
			{
				for (const auto& chunkPtr : archetype->GetChunks())
				{
					const Chunk& chunk = *chunkPtr;
					for (std::uint32_t row = 0; row < chunk.Count(); ++row)
					{
						entities.push_back(chunk.GetEntity(row));
					}
				}
			}
		}
		return entities;
	}

	// Visit all matching rows. Plain T/const T remain read-write/read-only queries.
	// Structural changes invalidate rows; writable references are scoped to this pass.
	template <typename... Terms, typename Fn>
		requires (QueryDetail::UniqueTerms<Terms...> && !QueryDetail::hasChangedTerms<Terms...>)
	void ForEachComponent(Fn&& fn)
	{
		ForEachComponentImpl<Terms...>(0, std::forward<Fn>(fn));
	}

	// Match all component types, then select chunks where any Changed term is newer.
	// The caller owns its cursor and advances it only after successfully consuming inputs.
	template <typename... Terms, typename Fn>
		requires (QueryDetail::UniqueTerms<Terms...> && QueryDetail::hasChangedTerms<Terms...>)
	void ForEachComponent(std::uint64_t sinceVersion, Fn&& fn)
	{
		ForEachComponentImpl<Terms...>(sinceVersion, std::forward<Fn>(fn));
	}

private:
	bool CanReuse(Entity entity) const noexcept;

	// Writes and committed structural edits share one monotonic registry-local clock.
	[[nodiscard]] std::uint64_t NextChangeVersion() noexcept;

	// Source and destination may be null for first insertion or final removal respectively.
	void StampStructuralChange(Chunk* source, Chunk* destination) noexcept;

	// Unwatched terms still require component presence but never affect chunk selection.
	template <typename Term>
	[[nodiscard]] static bool HasQueryChange(const Chunk& chunk, std::uint64_t sinceVersion)
	{
		if constexpr (QueryDetail::TermTraits<Term>::changed)
		{
			using Component = typename QueryDetail::TermTraits<Term>::ComponentType;
			return chunk.GetChangeVersion(ComponentTypeId{ typeid(Component) }) > sinceVersion;
		}
		else
		{
			return false;
		}
	}

	// Stamp each writable column once, before callbacks can partially modify its rows.
	template <typename Term>
	static void MarkQueryWrite(Chunk& chunk, std::uint64_t version)
	{
		if constexpr (QueryDetail::TermTraits<Term>::writable)
		{
			using Component = typename QueryDetail::TermTraits<Term>::ComponentType;
			chunk.MarkChanged(ComponentTypeId{ typeid(Component) }, version);
		}
	}

	// Access flags determine reference constness, independently of the callback signature.
	template <typename Term>
	static typename QueryDetail::TermTraits<Term>::Reference GetQueryComponent(
		Chunk& chunk, std::uint32_t row)
	{
		using Component = typename QueryDetail::TermTraits<Term>::ComponentType;
		if constexpr (QueryDetail::TermTraits<Term>::writable)
		{
			return chunk.Get<Component>(row);
		}
		else
		{
			return std::as_const(chunk).Get<Component>(row);
		}
	}

	// Filter before marking writes so read-write Changed terms cannot select themselves.
	template <typename... Terms, typename Fn>
	void ForEachComponentImpl(std::uint64_t sinceVersion, Fn&& fn)
	{
		auto& cache = archetypeManager_.GetQueryCache<
			typename QueryDetail::TermTraits<Terms>::ComponentType...>();
		for (Archetype* archetype : cache.matches)
		{
			for (const auto& chunkPtr : archetype->GetChunks())
			{
				Chunk& chunk = *chunkPtr;
				const std::uint32_t count = chunk.Count();
				if (count == 0) continue;

				// Watched columns combine with OR; unwatched outputs do not select chunks.
				if constexpr (QueryDetail::hasChangedTerms<Terms...>)
				{
					if (!(HasQueryChange<Terms>(chunk, sinceVersion) || ...)) continue;
				}

				// Read-only queries leave both the registry clock and column versions untouched.
				if constexpr ((QueryDetail::TermTraits<Terms>::writable || ...))
				{
					const std::uint64_t version = NextChangeVersion();
					(MarkQueryWrite<Terms>(chunk, version), ...);
				}

				// Raw storage access avoids publishing a separate write for each row.
				for (std::uint32_t row = 0; row < count; ++row)
				{
					fn(GetQueryComponent<Terms>(chunk, row)...);
				}
			}
		}
	}

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
		StampStructuralChange(source.chunk, destination);
	}

private:
	// Capacity covers all issued slots so cancellation never allocates.
	std::vector<std::uint32_t> freeIndices_;
	std::vector<EntitySlot> slots_;
	// Own the future component storage behind the registry's entity API.
	ArchetypeManager archetypeManager_;
	std::uint64_t changeVersion_ = 0; // Last write stamp issued; zero is the initial observer cursor.
	std::uint64_t ownerId_ = 0;
	std::uint32_t owningWorldID_ = 0; // World identity copied into every issued entity.
};
