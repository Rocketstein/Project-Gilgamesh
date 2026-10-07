#include "Scene.h"

Scene::Scene(std::string name)
{
	name_ = std::move(name);
}

bool Scene::Contains(Entity entity) const noexcept
{
	return std::find(entities_.begin(), entities_.end(), entity) != entities_.end();
}