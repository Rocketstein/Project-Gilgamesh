#pragma once

#include <memory>
#include <tuple>
#include <utility>

#include "Engine/ECS/Entity/Entity.h"
#include "EntityRegistry.h"

class World;

struct ECSAddEntityCommandPayload
{


};

struct ECSRemoveEntityCommandPayload
{


};

template <typename Component>
struct ECSAddComponentCommandPayload
{
	Entity entity;
	Component component;

	// Forward execution to the payload at its actual member address.
	static void Execute(void* commandData, EntityRegistry& registry, World& world)
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

// Owns constructor arguments until the component is created during playback.
template <typename Component, typename... Args>
struct ECSEmplaceComponentCommandPayload
{
public:
	// Expand the stored arguments and consume their values during construction.
	static void Execute(void* commandData, EntityRegistry& registry, World&)
	{
		auto* self = static_cast<ECSEmplaceComponentCommandPayload*>(commandData);
		std::apply(
			[self, &registry](auto&&... values)
			{
				registry.Emplace<Component>(
					self->entity, std::forward<decltype(values)>(values)...);
			},
			std::move(self->args));
	}

	// Destroy the arguments while leaving storage owned by the arena.
	static void Destroy(void* commandData) noexcept
	{
		std::destroy_at(static_cast<ECSEmplaceComponentCommandPayload*>(commandData));
	}

public:
	Entity entity;
	std::tuple<Args...> args; // Stores the constructor arguments by value.
};


template <typename Component>
struct ECSRemoveComponentCommandPayload
{


};

template <typename Component>
struct ECSSetComponentValueCommandPayload
{


};

