#include <array>
#include <cstddef>
#include <stdexcept>

#include "Engine/ECS/Framework/ArchetypeManager.h"
#include "Engine/ECS/Framework/EntityRegistry.h"
#include "Engine/ECS/Framework/World.h"
#include "Tests/TestFramework.h"

namespace
{
// Exercises an allocation alignment larger than the Entity column requires.
struct alignas(64) OverAlignedComponent
{
	std::byte value;
};
}

GILGAMESH_TEST("Engine.ECS.ArchetypeManager", "ReusesCanonicalSignature")
{
	ArchetypeManager manager;
	manager.RegisterComponent<int>();
	manager.RegisterComponent<float>();

	// Duplicate and reordered type IDs describe the same component set.
	const std::array<ComponentTypeId, 3> withDuplicate{
		ComponentTypeId{ typeid(int) },
		ComponentTypeId{ typeid(float) },
		ComponentTypeId{ typeid(int) }
	};
	const std::array<ComponentTypeId, 2> reversed{
		ComponentTypeId{ typeid(float) },
		ComponentTypeId{ typeid(int) }
	};

	// Equivalent type sets must resolve to the same stored archetype.
	Archetype& archetype = manager.FindOrCreate(withDuplicate);
	Archetype& reused = manager.FindOrCreate(reversed);
	const ArchetypeManager& readOnlyManager = manager;
	const Archetype* found = readOnlyManager.Find(reversed);
	GILGAMESH_CHECK_MESSAGE(&archetype == &reused && found == &archetype,
		"equivalent signatures must return the same stored archetype");
	GILGAMESH_CHECK_MESSAGE(archetype.signature.size() == 2 &&
		archetype.HasComponent(ComponentTypeId{ typeid(int) }) &&
		archetype.HasComponent(ComponentTypeId{ typeid(float) }),
		"the stored signature must be canonical and contain both types");

	// The archetype must receive the registered storage requirements for both types.
	const auto intInfo = archetype.componentTypes.find(ComponentTypeId{ typeid(int) });
	const auto floatInfo = archetype.componentTypes.find(ComponentTypeId{ typeid(float) });
	GILGAMESH_CHECK_MESSAGE(archetype.componentTypes.size() == 2 &&
		intInfo != archetype.componentTypes.end() &&
		floatInfo != archetype.componentTypes.end(),
		"the archetype must contain metadata for exactly its component types");
	if (intInfo != archetype.componentTypes.end() && floatInfo != archetype.componentTypes.end())
	{
		GILGAMESH_CHECK_MESSAGE(intInfo->second.size == sizeof(int) &&
			intInfo->second.alignment == alignof(int) &&
			floatInfo->second.size == sizeof(float) &&
			floatInfo->second.alignment == alignof(float),
			"component metadata must preserve each type's size and alignment");
	}
}

GILGAMESH_TEST("Engine.ECS.ArchetypeManager", "RequiresRegisteredComponents")
{
	ArchetypeManager manager;
	manager.RegisterComponent<int>();
	const std::array<ComponentTypeId, 2> signature{
		ComponentTypeId{ typeid(int) },
		ComponentTypeId{ typeid(float) }
	};

	// A missing type must fail before an incomplete archetype is inserted.
	bool rejected = false;
	try
	{
		(void)manager.FindOrCreate(signature);
	}
	catch (const std::logic_error&)
	{
		rejected = true;
	}
	GILGAMESH_CHECK_MESSAGE(rejected,
		"archetype creation must reject an unregistered component type");
	GILGAMESH_CHECK_MESSAGE(manager.Find(signature) == nullptr,
		"failed creation must not insert an incomplete archetype");

	// Registration after the failed request must allow the same signature.
	manager.RegisterComponent<float>();
	Archetype& archetype = manager.FindOrCreate(signature);
	GILGAMESH_CHECK_MESSAGE(manager.Find(signature) == &archetype &&
		archetype.componentTypes.size() == 2,
		"registration must allow the complete archetype to be created");
}

GILGAMESH_TEST("Engine.ECS.ArchetypeLayout", "EmptySignatureUsesEntityColumn")
{
	ArchetypeManager manager;
	const std::array<ComponentTypeId, 0> signature{};
	const ChunkLayout& layout = manager.FindOrCreate(signature).layout;

	// With no component columns, every byte is available to the Entity column.
	GILGAMESH_CHECK_MESSAGE(layout.capacity == CHUNK_SIZE / sizeof(Entity),
		"an empty archetype must use the entire chunk for Entity values");
	GILGAMESH_CHECK_MESSAGE(layout.entityOffset == 0 &&
		layout.allocationAlignment == alignof(Entity) &&
		layout.componentColumns.empty(),
		"an empty archetype must describe only an aligned Entity column");
}

GILGAMESH_TEST("Engine.ECS.ArchetypeLayout", "OverAlignedComponentFitsMaximumRows")
{
	ArchetypeManager manager;
	manager.RegisterComponent<OverAlignedComponent>();
	const ComponentTypeId typeId{ typeid(OverAlignedComponent) };
	const std::array<ComponentTypeId, 1> signature{ typeId };
	const ChunkLayout& layout = manager.FindOrCreate(signature).layout;

	// The allocation and component column must satisfy the component's alignment.
	const auto columnIt = layout.componentColumns.find(typeId);
	GILGAMESH_CHECK_MESSAGE(layout.capacity > 0 &&
		layout.allocationAlignment == alignof(OverAlignedComponent),
		"the layout must fit rows in storage aligned for the component");
	GILGAMESH_CHECK_MESSAGE(columnIt != layout.componentColumns.end(),
		"the layout must contain the registered component column");
	if (columnIt == layout.componentColumns.end())
		return;

	const ChunkLayout::Column& column = columnIt->second;
	GILGAMESH_CHECK_MESSAGE(layout.entityOffset == 0 &&
		column.offset % alignof(OverAlignedComponent) == 0 &&
		column.stride == sizeof(OverAlignedComponent),
		"the component column must start on an aligned address and use its object size as stride");
	GILGAMESH_CHECK_MESSAGE(layout.entityOffset + layout.capacity * sizeof(Entity) <= column.offset &&
		column.offset + layout.capacity * column.stride <= CHUNK_SIZE,
		"the Entity and component columns must not overlap or exceed the chunk");

	// One additional row must exceed the chunk after aligning the component column.
	const std::size_t nextCapacity = static_cast<std::size_t>(layout.capacity) + 1;
	const std::size_t entityBytes = nextCapacity * sizeof(Entity);
	const std::size_t alignment = alignof(OverAlignedComponent);
	const std::size_t alignedComponentOffset =
		entityBytes + (alignment - entityBytes % alignment) % alignment;
	GILGAMESH_CHECK_MESSAGE(
		alignedComponentOffset + nextCapacity * sizeof(OverAlignedComponent) > CHUNK_SIZE,
		"the layout capacity must be the maximum number of rows that fit");
}

GILGAMESH_TEST("Engine.ECS.EntityRegistry", "DestroyInvalidatesOldHandle")
{
	EntityRegistry registry;
	const Entity first = registry.Create();
	const Entity survivor = registry.Create();

	GILGAMESH_CHECK_MESSAGE(!first.IsNull() && !survivor.IsNull(),
		"created entities must have non-null handles");
	GILGAMESH_CHECK_MESSAGE(first != survivor,
		"concurrently live entities must have distinct handles");
	GILGAMESH_CHECK_MESSAGE(registry.IsValid(first) && registry.IsValid(survivor),
		"newly created entities must be alive");

	// Destruction changes the generation before the index can be reused.
	GILGAMESH_CHECK_MESSAGE(registry.Destroy(first),
		"the first destruction must succeed");
	GILGAMESH_CHECK_MESSAGE(!registry.IsValid(first),
		"destruction must immediately invalidate the old handle");
	GILGAMESH_CHECK_MESSAGE(!registry.Destroy(first),
		"a stale handle must not destroy an entity twice");

	const Entity replacement = registry.Create();
	GILGAMESH_CHECK_MESSAGE(replacement != first && registry.IsValid(replacement),
		"reusing a slot must issue a new, live handle");
	GILGAMESH_CHECK_MESSAGE(registry.IsValid(survivor),
		"reusing another slot must not disturb a live entity");
}

GILGAMESH_TEST("Engine.ECS.World", "OwnsEntityLifetime")
{
	World firstWorld;
	World secondWorld;
	const Entity first = firstWorld.CreateEntity();
	const Entity second = secondWorld.CreateEntity();

	// A world must reject handles issued by another world or the null handle.
	GILGAMESH_CHECK_MESSAGE(first != second,
		"different worlds must issue distinct handles");
	GILGAMESH_CHECK_MESSAGE(firstWorld.IsAlive(first),
		"the creating world must recognize its entity");
	GILGAMESH_CHECK_MESSAGE(!secondWorld.IsAlive(first),
		"another world must reject the entity");
	GILGAMESH_CHECK_MESSAGE(!firstWorld.IsAlive(Entity{}),
		"the null handle must not refer to a live entity");
	GILGAMESH_CHECK_MESSAGE(!secondWorld.DestroyEntity(first),
		"another world must not destroy the entity");
	GILGAMESH_CHECK_MESSAGE(firstWorld.DestroyEntity(first),
		"the creating world must be able to destroy the entity");
	GILGAMESH_CHECK_MESSAGE(!firstWorld.IsAlive(first),
		"a destroyed world entity must no longer be alive");
}
