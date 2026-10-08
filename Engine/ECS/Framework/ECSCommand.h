#pragma once

#include "Engine/ECS/Entity/Entity.h"
#include "EntityRegistry.h"

template <typename Component>
struct ECSAddEntityCommandPayload
{


};

template <typename Component>
struct ECSRemoveEntityCommandPayload
{


};

template <typename Component>
struct ECSAddComponentCommandPayload
{
	Entity entity;
	Component component;

	static void Execute(void* commandData, EntityRegistry& registry)
	{
		auto* self = static_cast<ECSAddComponentCommandPayload*>(commandData);
		registry.Emplace<Component>(self->entity, std::move(self->component));
	}
};

template <typename Component, typename... Args>
struct ECSEmplaceComponentCommandPayload
{
	Entity entity;
	Component component;
	std::tuple<Args...> args;
};


template <typename Component>
struct ECSRemoveComponentCommandPayload
{


};

template <typename Component>
struct ECSSetComponentValueCommandPayload
{


};

