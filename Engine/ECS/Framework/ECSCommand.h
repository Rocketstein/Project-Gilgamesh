#pragma once

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "EntityRegistry.h"
#include "Engine/ECS/Entity/Entity.h"
#include "Engine/ECS/Framework/Scene.h"

class World;
class ECSCommandBuffer;

// Scene changes share entity command ordering and playback boundaries.
enum class ECSSceneCommandType
{
	Create,
	SetActive,
	ClearActive,
	Destroy
};

// Keeps scene identity intact until a deferred change can be validated again.
struct ECSSceneCommandPayload
{
public:
	// Retain the scene identity and take ownership of its deferred name.
	explicit ECSSceneCommandPayload(ECSSceneCommandType type, SceneHandle scene, std::string&& name)
		: type(type), scene(scene), name(std::move(name))
	{
	}

private:
	friend class ECSCommandBuffer;
	// Playback callbacks are internal to the owning command buffer.
	static void Execute(void* commandData, EntityRegistry& registry, World& world);
	static void Destroy(void* commandData, World& world) noexcept;

private:
	ECSSceneCommandType type; // Scene operation recorded by the caller.
	SceneHandle scene; // Full identity, including generation and owning world.
	std::string name; // Owns a deferred scene's name until creation is published.
};

// Keeps a world-owned entity reserved until its creation is played back.
struct ECSAddWorldEntityCommandPayload
{
public:
	// Retain the reserved entity identity without publishing it yet.
	explicit ECSAddWorldEntityCommandPayload(Entity entity) noexcept : entity(entity) {}

private:
	friend class ECSCommandBuffer;
	// Playback callbacks are internal to the owning command buffer.
	static void Execute(void* commandData, EntityRegistry& registry, World& world);
	static void Destroy(void* commandData, World& world) noexcept;

private:
	Entity entity; // Reservation to commit or cancel when the payload is discarded.
};


// Couples a reserved entity with its intended scene membership.
struct ECSAddEntityCommandPayload
{
public:
	// Retain both identities until their FIFO creation prerequisites are applied.
	explicit ECSAddEntityCommandPayload(Entity entity, SceneHandle handle) noexcept
		: entity(entity), handle(handle)
	{
	}

private:
	friend class ECSCommandBuffer;
	// Playback callbacks are internal to the owning command buffer.
	static void Execute(void* commandData, EntityRegistry& registry, World& world);
	static void Destroy(void* commandData, World& world) noexcept;

private:
	Entity entity; // Reservation to commit or cancel when the payload is discarded.
	SceneHandle handle; // Full scene identity checked again during playback.
};

// Defers entity destruction until the world reaches a structural boundary.
struct ECSRemoveEntityCommandPayload
{
public:
	// Retain the identity to remove without touching its current storage.
	explicit ECSRemoveEntityCommandPayload(Entity entity) noexcept : entity(entity) {}

private:
	friend class ECSCommandBuffer;
	// Playback callbacks are internal to the owning command buffer.
	static void Execute(void* commandData, EntityRegistry& registry, World& world);
	static void Destroy(void* commandData, World& world) noexcept;

private:
	Entity entity; // Entity identity validated again during playback.
};

// Owns a preconstructed component until it can be attached during playback.
template <typename Component>
struct ECSAddComponentCommandPayload
{
public:
	// Move the component directly into its stable arena-owned payload.
	explicit ECSAddComponentCommandPayload(Entity entity, Component&& component)
		: entity(entity), component(std::move(component))
	{
	}

private:
	friend class ECSCommandBuffer;

	// Forward execution to the payload at its actual member address.
	static void Execute(void* commandData, EntityRegistry& registry, World& world)
	{
		auto* self = static_cast<ECSAddComponentCommandPayload*>(commandData);
		registry.Emplace<Component>(self->entity, std::move(self->component));
	}

	// Destroy the payload while leaving storage owned by the arena.
	static void Destroy(void* commandData, World& world) noexcept
	{
		std::destroy_at(static_cast<ECSAddComponentCommandPayload*>(commandData));
	}

private:
	Entity entity; // Entity that will receive the component.
	Component component; // Owns the value until attachment or discarded playback.
};

// Owns constructor arguments until the component is created during playback.
template <typename Component, typename... Args>
struct ECSEmplaceComponentCommandPayload
{
public:
	// Move captured argument values without constructing the component yet.
	explicit ECSEmplaceComponentCommandPayload(Entity entity, std::tuple<Args...>&& args)
		: entity(entity), args(std::move(args))
	{
	}

private:
	friend class ECSCommandBuffer;

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
	static void Destroy(void* commandData, World&) noexcept
	{
		std::destroy_at(static_cast<ECSEmplaceComponentCommandPayload*>(commandData));
	}

private:
	Entity entity; // Entity that will receive the constructed component.
	std::tuple<Args...> args; // Stores the constructor arguments by value.
};

// Defers removal of one component type until structural playback.
template <typename Component>
struct ECSRemoveComponentCommandPayload
{
public:
	// Retain the identity while leaving component storage unchanged.
	explicit ECSRemoveComponentCommandPayload(Entity entity) noexcept : entity(entity) {}

private:
	friend class ECSCommandBuffer;

	// Remove the component only if the entity still owns it during playback.
	static void Execute(void* commandData, EntityRegistry& registry, World&)
	{
		auto* self = static_cast<ECSRemoveComponentCommandPayload*>(commandData);
		registry.Remove<Component>(self->entity);
	}

	// End the payload lifetime while retaining the arena's allocation.
	static void Destroy(void* commandData, World&) noexcept
	{
		std::destroy_at(static_cast<ECSRemoveComponentCommandPayload*>(commandData));
	}

private:
	Entity entity; // Entity whose component will be removed.
};

template <typename Component>
struct ECSSetComponentValueCommandPayload
{


};

