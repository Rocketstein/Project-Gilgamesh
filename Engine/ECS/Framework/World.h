#pragma once

#include <string>

#include "Engine/ECS/Framework/EntityRegistry.h"
#include "Engine/ECS/Framework/Scene.h"

class World final
{
public:
	World() = default;

	//SceneHandle CreateScene(std::string name);
	//bool DestroyScene(SceneHandle scene);
	//const Scene* TryGetScene(SceneHandle scene) const noexcept;

	[[nodiscard]]
	Entity CreateEntity();

	[[nodiscard]]
	bool DestroyEntity(Entity entity);

	[[nodiscard]]
	bool IsAlive(Entity entity) const noexcept;

private:
	EntityRegistry entityRegistry_;
	//std::unordered_map<std::string, Scene> scenes_;
	//std::vector<Scene*> activeScenes_;
};
