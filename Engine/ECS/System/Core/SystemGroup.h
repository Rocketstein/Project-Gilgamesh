#pragma once

#include "ISystem.h"

#include <memory>
#include <vector>

// TODO: Extend to metadata driven management using reflection

class SystemGroup : public ISystem
{
public:
	void OnCreate() override;
	void OnDestroy(World& world) override;

	void Update(World& world, float deltaTime) override;

	void AddSystem(ISystem* system);
	void RemoveSystem(ISystem* system);
	// No sorting yet; transition to reflection should come first

private:
	std::vector<std::shared_ptr<ISystem>> systems_;

};