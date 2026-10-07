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

	[[nodiscard]]
	bool DestroyEntity(Entity entity);

	[[nodiscard]]
	bool IsAlive(Entity entity) const noexcept;

private:
	EntityRegistry entityRegistry_;

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
