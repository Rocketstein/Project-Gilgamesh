#pragma once

#include <memory>
#include <new>
#include <utility>
#include <vector>

#include "ArchetypeResource.h"
#include "ECSCommand.h"
#include "MemoryArena.h"

class ECSCommandBuffer final
{
public:
	ECSCommandBuffer();

	// flush...
	// sort...

	void AddEntity(Entity entity)
	{

	}

	void RemoveEntity(Entity entity)
	{

	}

	// Premade component attachment
	template <typename Component>
	void AddComponent(Entity entity, Component component)
	{
		using Payload = ECSAddComponentCommandPayload<Component>;
		using Command = ECSCommand<Payload>;

		// Allocate storage aligned for both metadata and payload.
		void* writePtr = commandBuffer_.Allocate(
			sizeof(Command), alignof(Command));

		if (!writePtr) return;

		// Construct the complete record directly in arena storage.
		auto* command = new (writePtr) Command{
			{ &Payload::Execute, &Payload::Destroy, sizeof(Command) },
			{ entity, std::move(component) }
		};
	}

	// Deferred component construction
	template <typename Component, typename... Args>
	void EmplaceComponent(Entity entity, Args&&... args)
	{

	}

	template <typename Component>
	void RemoveComponent(Entity entity, Component component)
	{

	}

private:
	using DispatchFn = void(*)(void* commandData, EntityRegistry& registry);
	using DestroyFn = void(*)(void* commandData) noexcept;

	struct ECSCommandHeader
	{
		DispatchFn dispatch;
		DestroyFn destroy;
		uint32_t size;
	};

	template <typename CommandPayload>
	struct ECSCommand
	{
		ECSCommandHeader header;
		CommandPayload commandPayload;
	};

	MemoryArena commandBuffer_;
};