#include "EntityRegistry.h"

#include <atomic>
#include <limits>
#include <stdexcept>

namespace
{
	// Give each registry a distinct identity so handles cannot cross worlds.
	// Deferred world ID usage until implementing world manager
	std::uint64_t AcquireEntityRegistryOwnerId() noexcept
	{
		static std::atomic<std::uint64_t> nextOwnerId{ 1 };
		return nextOwnerId.fetch_add(1, std::memory_order_relaxed);
	}
} // Anonymous namespace

EntityRegistry::EntityRegistry() noexcept
	: ownerId_(AcquireEntityRegistryOwnerId())
{
}

Entity EntityRegistry::Create()
{
	// Reuse an index whose generation was advanced during destruction.
	if (!freeIndices_.empty())
	{
		const std::uint32_t index = freeIndices_.back();
		freeIndices_.pop_back();
		slots_[index].state = EntityState::Alive;
		return Entity(index, slots_[index].generation, ownerId_);
	}

	// The maximum index is reserved for the null entity handle.
	if (slots_.size() >= Entity::InvalidIndex)
	{
		throw std::length_error("EntityRegistry handle capacity exceeded");
	}

	const auto index = static_cast<std::uint32_t>(slots_.size());
	slots_.emplace_back();
	slots_.back().state = EntityState::Alive;
	return Entity(index, slots_.back().generation, ownerId_);
}

Entity EntityRegistry::Reserve()
{
	// Reuse an index whose generation was advanced during destruction.
	if (!freeIndices_.empty())
	{
		const std::uint32_t index = freeIndices_.back();
		freeIndices_.pop_back();
		slots_[index].state = EntityState::Reserved;
		return Entity(index, slots_[index].generation, ownerId_);
	}
	// The maximum index is reserved for the null entity handle.
	if (slots_.size() >= Entity::InvalidIndex)
	{
		throw std::length_error("EntityRegistry handle capacity exceeded");
	}
	const auto index = static_cast<std::uint32_t>(slots_.size());
	slots_.emplace_back();
	slots_.back().state = EntityState::Reserved;
	return Entity(index, slots_.back().generation, ownerId_);
}

void EntityRegistry::Commit(Entity entity)
{
	if (GetEntityState(entity) != EntityState::Reserved) return;

	slots_[entity.index_].state = EntityState::Alive;
}

void EntityRegistry::CancelEntityReservation(Entity entity)
{
	if (GetEntityState(entity) != EntityState::Reserved) return;

	slots_[entity.index_].state = EntityState::Canceled;
}

bool EntityRegistry::Destroy(Entity entity)
{
	if (GetEntityState(entity) != EntityState::Alive) return false;

	// Component destructors may create handles, so do not retain a slot reference.
	const EntitySlot source = slots_[entity.index_];
	const bool canReuse =
		source.generation < std::numeric_limits<std::uint32_t>::max() - 1;

	// Allocate free-list space before changing the entity's lifetime.
	if (canReuse)
	{
		freeIndices_.reserve(freeIndices_.size() + 1);
	}

	// Erase the component row and repair the slot of a swapped-in entity.
	if (source.chunk != nullptr)
	{
		if (const auto displaced = source.archetype->EraseRow(*source.chunk, source.row))
		{
			slots_[displaced->index_].row = source.row;
		}
	}

	// Clear the location before the slot can be reused.
	EntitySlot& slot = slots_[entity.index_];
	slot.archetype = nullptr;
	slot.chunk = nullptr;
	slot.row = 0;
	slot.state = EntityState::Free;

	// Invalidate the old handle and never recycle an exhausted slot.
	++slot.generation;
	if (canReuse)
	{
		freeIndices_.push_back(entity.index_);
	}
	return true;
}

EntityState EntityRegistry::GetEntityState(Entity entity) const noexcept
{
	if (entity.ownerId_ != ownerId_ || entity.index_ >= slots_.size())
	{
		return EntityState::Invalid;
	}

	// Only Create issues handles, so a freed slot's next generation has no
	// matching handle until that slot is reused.
	const std::uint32_t generation = slots_[entity.index_].generation;

	if (generation == std::numeric_limits<std::uint32_t>::max() || generation != entity.generation_)
	{
		return EntityState::Invalid;
	}

	return slots_[entity.index_].state;
}