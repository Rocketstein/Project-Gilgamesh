#include "World.h"

SceneHandle World::CreateScene(std::string name)
{
	SceneHandle handle = {};
	std::uint32_t id = !freeSceneIndices_.empty() ? freeSceneIndices_.back() : scenes_.size();
	if (id >= scenes_.size())
	{
		scenes_.reserve(id);
	}

	std::string sceneName = name.empty() ? "Scene_" + std::to_string(id) : std::move(name);
	auto scene = std::make_unique<Scene>(std::move(sceneName));
	scenes_[id] = SceneItem{ std::move(scene), scenes_[id].generation };

	handle.id = id;
	handle.generation = scenes_[id].generation;

	freeSceneIndices_.pop_back();
	return handle;
}

bool World::DestroyScene(SceneHandle handle)
{
	if (!IsSceneHandleValid(handle)) return false;
	SceneItem& item = scenes_[handle.id];
	Scene* scenePtr = item.scene.get();

	// Remove scene specefic entities
	for (Entity entity : scenePtr->entities_)
	{
		entityRegistry_.Destroy(entity);
	}

	item.generation++;
	freeSceneIndices_.push_back(handle.id);
	return true;
}

const Scene* World::TryGetScene(SceneHandle handle) const noexcept
{
	if (!IsSceneHandleValid(handle)) return nullptr;
	return scenes_[handle.id].scene.get();
}

bool World::IsSceneHandleValid(SceneHandle handle) const noexcept
{
	if (handle.IsNull())
	{
		return false;
	}

	if (handle.id >= scenes_.size())
	{
		return false;
	}

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
	if (!IsSceneHandleValid(handle))
	{
		Entity entity;
		return entity;
	}

	Scene* scene = scenes_[handle.id].scene.get();
	Entity entity = entityRegistry_.Create();
	scene->entities_.push_back(entity);
	return entity;
}

bool World::DestroyEntity(Entity entity)
{
	return entityRegistry_.Destroy(entity);
}

bool World::IsAlive(Entity entity) const noexcept
{
	return entityRegistry_.IsValid(entity);
}
