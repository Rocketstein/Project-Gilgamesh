#pragma once

#include <memory>
#include <new>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "ArchetypeResource.h"
#include "ECSCommand.h"
#include "MemoryArena.h"

struct SceneHandle;

class World;

// Owns deferred ECS payloads and the metadata needed to execute them.
class ECSCommandBuffer final
{
public:
	// Destroy recorded payloads before releasing their arena storage.
	~ECSCommandBuffer() noexcept;

	ECSCommandBuffer(const ECSCommandBuffer&) = delete;
	ECSCommandBuffer& operator=(const ECSCommandBuffer&) = delete;
	ECSCommandBuffer(ECSCommandBuffer&&) = delete;
	ECSCommandBuffer& operator=(ECSCommandBuffer&&) = delete;

	Entity CreateEntity();

	Entity CreateEntity(SceneHandle handle);

	void RemoveEntity(Entity entity);

	// Reserves a scene identity before publishing its owned creation payload.
	[[nodiscard]] SceneHandle CreateScene(std::string name);
	// Records scene operations and owned creation names in the entity command FIFO.
	void RecordSceneCommand(ECSSceneCommandType type, SceneHandle scene, std::string name = "");

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
			Payload::Destroy(payload, world_);
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
			Payload::Destroy(payload, world_);
			throw;
		}
	}

	template <typename Component>
	void RemoveComponent(Entity entity)
	{
		using Payload = ECSRemoveComponentCommandPayload<Component>;

		// Keep the concrete payload at a stable, correctly aligned address.
		void* writePtr = commandBuffer_.Allocate(
			sizeof(Payload), alignof(Payload));

		if (!writePtr)
		{
			throw std::bad_alloc{};
		}

		// Construct the payload before publishing its metadata.
		auto* payload = new (writePtr) Payload{ entity };

		// Registration failure must not leave an untracked live payload.
		try
		{
			commands_.push_back({ &Payload::Execute, &Payload::Destroy, payload });
		}
		catch (...)
		{
			Payload::Destroy(payload, world_);
			throw;
		}
	}

private:
	friend class World;

	// Only the owning world may create a buffer or advance command playback.
	explicit ECSCommandBuffer(World& world);
	void Flush();
	void Clear();

	// Preserve concrete command dispatch and destruction without virtual payloads.
	using DispatchFn = void(*)(void* payload, EntityRegistry& registry, World& world);
	using DestroyFn  = void(*)(void* commandData, World& world) noexcept;

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
