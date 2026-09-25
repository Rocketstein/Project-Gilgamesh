#pragma once

#include "Engine/ECS/Entity/EntityRegistry.h"

class Scene;

class World final
{
public:
	World() = default;


	[[nodiscard]]
	Entity CreateEntity();

	[[nodiscard]]
	bool DestroyEntity(Entity entity);

	[[nodiscard]]
	bool IsAlive(Entity entity) const noexcept;

private:
	EntityRegistry entityRegistry_;

};
