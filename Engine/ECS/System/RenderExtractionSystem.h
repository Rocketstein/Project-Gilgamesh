#pragma once

#include "Engine/ECS/System/Core/ISystem.h"

class RenderExtractionSystem : ISystem
{
public:
	void Update(World& world, float deltaTime) override;

private:

};