#include "World.h"

#include "IDGenerator.h"

World::World() : worldID_(IDGenerator<WorldTag>::Next())
{

}

SceneHandle World::CreateScene(std::string name)
{
	// Reserve the null index and narrow only after checking the slot capacity.
	const bool reuseSlot = !freeSceneIndices_.empty();
	if (!reuseSlot && scenes_.size() >= std::numeric_limits<std::uint32_t>::max())
	{
		throw std::length_error("World scene handle capacity exceeded");
	}
	const std::uint32_t id = reuseSlot
		? freeSceneIndices_.back()
		: static_cast<std::uint32_t>(scenes_.size());

	// Construct the scene before changing either the slots or the free list.
	std::string sceneName = name.empty() ? "Scene_" + std::to_string(id) : std::move(name);
	auto scene = std::make_unique<Scene>(std::move(sceneName));
	if (reuseSlot)
	{
		scenes_[id].scene = std::move(scene);
		freeSceneIndices_.pop_back();
	}
	else
	{
		scenes_.push_back(SceneItem{ std::move(scene), 0 });
	}

	return SceneHandle{ id, scenes_[id].generation };
}

bool World::DestroyScene(SceneHandle handle)
{
	if (!IsSceneHandleValid(handle)) return false;
	Scene* scene = scenes_[handle.id].scene.get();

	// Allocate recycling space before destroying any scene content.
	const bool canReuse = scenes_[handle.id].generation <
		std::numeric_limits<std::uint32_t>::max() - 1;
	if (canReuse)
	{
		freeSceneIndices_.reserve(freeSceneIndices_.size() + 1);
	}

	// DestroyEntity erases membership, so drain rather than iterate the vector.
	while (!scene->entities_.empty())
	{
		DestroyEntity(scene->entities_.back());
	}

	// Reacquire the slot after component destruction and retire exhausted slots.
	SceneItem& item = scenes_[handle.id];
	item.scene.reset();
	++item.generation;
	if (canReuse)
	{
		freeSceneIndices_.push_back(handle.id);
	}
	return true;
}

const Scene* World::TryGetScene(SceneHandle handle) const noexcept
{
	if (!IsSceneHandleValid(handle)) return nullptr;
	return scenes_[handle.id].scene.get();
}

bool World::IsSceneHandleValid(SceneHandle handle) const noexcept
{
	// Reject null or out-of-range handles before accessing the slot.
	if (handle.IsNull())
	{
		return false;
	}

	if (handle.id >= scenes_.size())
	{
		return false;
	}

	// A recycled slot has no scene until it is reused.
	if (scenes_[handle.id].scene == nullptr)
	{
		return false;
	}

	// A matching index alone does not identify the same scene after slot reuse.
	if (scenes_[handle.id].generation != handle.generation)
	{
		return false;
	}

	return true;
}

Entity World::CreateEntity()
{
	return entityRegistry_.Create();
}

Entity World::CreateEntity(SceneHandle handle)
{
	if (!IsSceneHandleValid(handle)) return {};

	Scene* scene = scenes_[handle.id].scene.get();
	const Entity entity = entityRegistry_.Create();
	// Publish both membership directions, retiring the entity if tracking fails.
	try
	{
		entityScenes_.emplace(entity, handle);
		scene->entities_.push_back(entity);
	}
	catch (...)
	{
		entityScenes_.erase(entity);
		entityRegistry_.Destroy(entity);
		throw;
	}
	return entity;
}

Entity World::ReserveEntity()
{
	return entityRegistry_.Reserve();
}

void World::CancelEntityReservation(Entity entity) noexcept
{
	if (entity.IsNull()) return;
	entityRegistry_.CancelEntityReservation(entity);
}

// Activate a reserved entity after registering its scene membership.
void World::CommitEntity(Entity entity, SceneHandle handle)
{
	// Only a pending reservation can acquire scene membership.
	if (entityRegistry_.GetEntityState(entity) != EntityState::Reserved) return;
	if (!IsSceneHandleValid(handle))
	{
		CancelEntityReservation(entity);
		return;
	}

	// Publish both membership directions before making the entity alive.
	Scene* scene = scenes_[handle.id].scene.get();
	try
	{
		entityScenes_.emplace(entity, handle);
		scene->entities_.push_back(entity);
	}
	catch (...)
	{
		// Roll back tracking and cancel the reservation if allocation fails.
		entityScenes_.erase(entity);
		CancelEntityReservation(entity);
		throw;
	}

	entityRegistry_.Commit(entity);
}

void World::DestroyEntity(Entity entity)
{
	// Copy the owner before component destruction can create other entities.
	const auto membership = entityScenes_.find(entity);
	const SceneHandle owner = membership != entityScenes_.end()
		? membership->second : SceneHandle{};
	if (!entityRegistry_.Destroy(entity)) return;

	// World-owned entities have no scene, while scene members leave both indices.
	if (IsSceneHandleValid(owner))
	{
		std::erase(scenes_[owner.id].scene->entities_, entity);
	}
	entityScenes_.erase(entity);
}

bool World::IsAlive(Entity entity) const noexcept
{
	return entityRegistry_.GetEntityState(entity) == EntityState::Alive;
}
