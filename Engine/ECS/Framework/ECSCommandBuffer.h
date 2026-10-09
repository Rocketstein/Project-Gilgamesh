#pragma once

#include <memory>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "ArchetypeResource.h"
#include "ECSCommand.h"
#include "MemoryArena.h"

class World;

// Owns deferred ECS payloads and the metadata needed to execute them.
class ECSCommandBuffer final
{
public:
	explicit ECSCommandBuffer(World& world);
	// Destroy recorded payloads before releasing their arena storage.
	~ECSCommandBuffer() noexcept;

	ECSCommandBuffer(const ECSCommandBuffer&) = delete;
	ECSCommandBuffer& operator=(const ECSCommandBuffer&) = delete;
	ECSCommandBuffer(ECSCommandBuffer&&) = delete;
	ECSCommandBuffer& operator=(ECSCommandBuffer&&) = delete;

	void Flush();

	void Clear();

	void AddEntity(Entity entity)
	{

	}

	void RemoveEntity(Entity entity)
	{

	}

	// Record a preconstructed component for later attachment.
	template <typename Component>
	void AddComponent(Entity entity, Component component)
	{
		using Payload = ECSAddComponentCommandPayload<Component>;

		// Keep the concrete payload at a stable, correctly aligned address.
		void* writePtr = commandBuffer_.Allocate(
			sizeof(Payload), alignof(Payload));

		if (!writePtr)
		{
			throw std::bad_alloc{};
		}

		// Construct the payload before publishing its metadata.
		auto* payload = new (writePtr) Payload{
			entity, std::move(component)
		};

		// Registration failure must not leave an untracked live payload.
		try
		{
			commands_.push_back({ &Payload::Execute, &Payload::Destroy, payload });
		}
		catch (...)
		{
			Payload::Destroy(payload);
			throw;
		}
	}

	// Store argument values and defer component construction until playback.
	template <typename Component, typename... Args>
	void EmplaceComponent(Entity entity, Args&&... args)
	{
		using Payload = ECSEmplaceComponentCommandPayload<
			Component, std::decay_t<Args>...>;

		// Keep the concrete payload at a stable, correctly aligned address.
		void* writePtr = commandBuffer_.Allocate(
			sizeof(Payload), alignof(Payload));

		if (!writePtr)
		{
			throw std::bad_alloc{};
		}

		// Construct the payload before publishing its metadata.
		auto* payload = new (writePtr) Payload{
			entity, std::tuple<std::decay_t<Args>...>{std::forward<Args>(args)...}
		};

		// Registration failure must not leave an untracked live payload.
		try
		{
			commands_.push_back({ &Payload::Execute, &Payload::Destroy, payload });
		}
		catch (...)
		{
			Payload::Destroy(payload);
			throw;
		}
	}

	template <typename Component>
	void RemoveComponent(Entity entity, Component component)
	{

	}

private:
	using DispatchFn = void(*)(void* payload, EntityRegistry& registry, World& world);
	using DestroyFn  = void(*)(void* commandData) noexcept;

	// Describes a concrete payload without owning its arena storage.
	struct ECSCommandHeader
	{
		DispatchFn dispatch;
		DestroyFn destroy;
		void* payload;
	};

private:
	std::vector<ECSCommandHeader> commands_; // Metadata in recording order.
	MemoryArena commandBuffer_;				 // Stable storage for the owned payload objects.
	World& world_;							 // The world to which the commands will be applied.
};
