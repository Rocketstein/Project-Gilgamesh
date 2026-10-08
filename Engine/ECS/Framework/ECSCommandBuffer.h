#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "ArchetypeResource.h"
#include "ECSCommand.h"

class ECSCommandBuffer final
{
public:
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
		using CommandType = ECSAddComponentCommand<Component>;

		uint32_t headerSize = sizeof(ECSCommandHeader);
		uint32_t commandSize = sizeof(CommandType);
		size_t oldSize = commandBuffer_.size();

		commandBuffer_.resize(oldSize + headerSize + commandSize);
		uint8_t* writePtr = commandBuffer_.data() + oldSize;

		auto* header = reinterpret_cast<ECSCommandHeader*>(writePtr);
		header->dispatch = //execute;
		header->size = commandSize;

		new (writePtr + headerSize) CommandType{ entity, std::move(component) };
	}

	// Deferred component construction
	template <typename Component, typename... Args>
	void AddComponent(Entity entity, Args&&... args)
	{

	}

	template <typename Component>
	void RemoveComponent(Entity entity, Component component)
	{

	}

private:
	using DispatchFn = void(*)(void* commandDataPtr, EntityRegistry& registry);

	template <typename Component>
	struct ECSCommandHeader
	{
		DispatchFn dispatch;
		uint32_t size;
	};

	std::vector<std::byte> commandBuffer_;


};