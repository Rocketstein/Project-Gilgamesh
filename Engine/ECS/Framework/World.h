#pragma once

#include "Engine/ECS/Entity/EntityRegistry.h"

class Scene;

class World final
{
public:
	World() = default;



private:
	EntityRegistry entityRegistery_;

};