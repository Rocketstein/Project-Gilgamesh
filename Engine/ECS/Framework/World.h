#pragma once

#include <string>

#include "Engine/ECS/Framework/EntityRegistry.h"
#include "Engine/ECS/Framework/Scene.h"

class World final
{
public:
	World() = default;

	[[nodiscard]] SceneHandle CreateScene(std::string name = "");
	bool DestroyScene(SceneHandle handle);
	[[nodiscard]] const Scene* TryGetScene(SceneHandle handle) const noexcept;
	[[nodiscard]] bool IsSceneHandleValid(SceneHandle handle) const noexcept;

	[[nodiscard]]
	Entity CreateEntity();

	[[nodiscard]]
	Entity CreateEntity(SceneHandle handle);

	void DestroyEntity(Entity entity);

	[[nodiscard]]
	bool IsAlive(Entity entity) const noexcept;

	template <typename Component, typename... Args>
	Component& Emplace(Entity entity, Args&&... args)
	{
		return entityRegistry_.Emplace<Component>(entity, std::forward<Args>(args)...);
	}

	template <typename Component>
	[[nodiscard]] bool Has(Entity entity) const
	{
		return entityRegistry_.Has<Component>(entity);
	}

	template <typename Component>
	[[nodiscard]] Component& Get(Entity entity)
	{
		return entityRegistry_.Get<Component>(entity);
	}

	template <typename Component>
	[[nodiscard]] const Component& Get(Entity entity) const
	{
		return entityRegistry_.Get<Component>(entity);
	}

	template <typename... Components, typename Fn>
	void ForEach(Fn&& fn)
	{
		entityRegistry_.ForEach<Components...>(std::forward<Fn>(fn));
	}


private:
	EntityRegistry entityRegistry_;
	// Track scene ownership without adding scene concepts to ECS storage.
	std::map<Entity, SceneHandle> entityScenes_;

	// =============================================================================
	// Scene Management
	// =============================================================================
	struct SceneItem
	{
		std::unique_ptr<Scene> scene;
		std::uint32_t generation = 0;
	};
	std::vector<SceneItem> scenes_;
	//std::vector<std::uint32_t> activeScenes_;
	std::vector<std::uint32_t> freeSceneIndices_;
};
