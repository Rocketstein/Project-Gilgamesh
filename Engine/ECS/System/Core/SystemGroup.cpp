#include "SystemGroup.h"

void SystemGroup::OnCreate()
{
	for (auto& sys : systems_) sys->OnCreate();
}

void SystemGroup::OnDestroy(World& world)
{
	for (auto& sys : systems_) sys->OnDestroy(world);
}

void SystemGroup::Update(World& world, float deltaTime)
{
	for (auto& system : systems_)
	{
		if (system->IsEnabled())
		{
			system->Update(world, deltaTime);
		}
	}
}

void SystemGroup::AddSystem(ISystem* system)
{
	systems_.push_back(system);
}

void SystemGroup::RemoveSystem(ISystem* system)
{

}
