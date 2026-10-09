#pragma once

#include <memory>
#include <tuple>
#include <utility>

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

	// Forward execution to the payload at its actual member address.
	static void Execute(void* commandData, EntityRegistry& registry)
	{
		auto* self = static_cast<ECSAddComponentCommandPayload*>(commandData);
		registry.Emplace<Component>(self->entity, std::move(self->component));
	}

	// Destroy the payload while leaving storage owned by the arena.
	static void Destroy(void* commandData) noexcept
	{
		std::destroy_at(static_cast<ECSAddComponentCommandPayload*>(commandData));
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

