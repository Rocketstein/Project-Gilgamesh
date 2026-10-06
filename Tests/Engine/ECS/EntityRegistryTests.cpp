#include <array>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>

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

// Tracks the lifetime of a movable, noncopyable component across row changes.
struct TrackedComponent
{
	static inline int liveCount = 0;

	explicit TrackedComponent(int input)
		: value(std::make_unique<int>(input))
	{
		++liveCount;
	}

	TrackedComponent(TrackedComponent&& other) noexcept
		: value(std::move(other.value))
	{
		++liveCount;
	}

	TrackedComponent(const TrackedComponent&) = delete;
	TrackedComponent& operator=(const TrackedComponent&) = delete;

	~TrackedComponent() noexcept { --liveCount; }

	// Owned data must survive a move between archetype rows.
	std::unique_ptr<int> value;
};

// Lets an insertion fail after a preceding component was constructed.
struct ThrowingComponent
{
	explicit ThrowingComponent(bool shouldThrow)
	{
		if (shouldThrow)
			throw std::runtime_error("Component construction failed");
	}

	ThrowingComponent(ThrowingComponent&&) noexcept = default;
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

GILGAMESH_TEST("Engine.ECS.Archetype", "FailedConstructionRollsBackPendingRow")
{
	TrackedComponent::liveCount = 0;
	{
		EntityRegistry registry;
		ArchetypeManager manager;
		manager.RegisterComponent<TrackedComponent>();
		manager.RegisterComponent<ThrowingComponent>();
		const std::array<ComponentTypeId, 2> signature{
			ComponentTypeId{ typeid(TrackedComponent) },
			ComponentTypeId{ typeid(ThrowingComponent) }
		};
		Archetype& archetype = manager.FindOrCreate(signature);

		// Keep one committed row to inspect after the next constructor throws.
		const auto [chunk, firstRow] = archetype.InsertRow(
			registry.Create(), [](Chunk::PendingRow& pending)
			{
				pending.Construct<TrackedComponent>(10);
				pending.Construct<ThrowingComponent>(false);
			});
		GILGAMESH_CHECK_MESSAGE(firstRow == 0 && chunk->Count() == 1 &&
			TrackedComponent::liveCount == 1,
			"the first row must be complete before testing rollback");

		bool rejected = false;
		try
		{
			(void)archetype.InsertRow(
				registry.Create(), [](Chunk::PendingRow& pending)
				{
					pending.Construct<TrackedComponent>(20);
					pending.Construct<ThrowingComponent>(true);
				});
		}
		catch (const std::runtime_error&)
		{
			rejected = true;
		}
		GILGAMESH_CHECK_MESSAGE(rejected && chunk->Count() == 1 &&
			TrackedComponent::liveCount == 1 &&
			chunk->Get<TrackedComponent>(0).value &&
			*chunk->Get<TrackedComponent>(0).value == 10,
			"failed construction must destroy only the pending objects");

		// A later insertion must reuse the row abandoned by the exception.
		const auto [reusedChunk, reusedRow] = archetype.InsertRow(
			registry.Create(), [](Chunk::PendingRow& pending)
			{
				pending.Construct<TrackedComponent>(30);
				pending.Construct<ThrowingComponent>(false);
			});
		GILGAMESH_CHECK_MESSAGE(reusedChunk == chunk && reusedRow == 1 &&
			chunk->Count() == 2 && TrackedComponent::liveCount == 2,
			"insertion must reuse the pending row after rollback");
	}
	GILGAMESH_CHECK_MESSAGE(TrackedComponent::liveCount == 0,
		"destroying the archetype must destroy every committed component");
}

GILGAMESH_TEST("Engine.ECS.Archetype", "IncompleteRowDoesNotCommit")
{
	EntityRegistry registry;
	ArchetypeManager manager;
	manager.RegisterComponent<int>();
	manager.RegisterComponent<float>();
	const std::array<ComponentTypeId, 2> signature{
		ComponentTypeId{ typeid(int) }, ComponentTypeId{ typeid(float) }
	};
	Archetype& archetype = manager.FindOrCreate(signature);

	// A successful row gives a stable chunk pointer for rollback checks.
	const auto [chunk, firstRow] = archetype.InsertRow(
		registry.Create(), [](Chunk::PendingRow& pending)
		{
			pending.Construct<int>(1);
			pending.Construct<float>(1.0f);
		});
	GILGAMESH_CHECK_MESSAGE(firstRow == 0 && chunk->Count() == 1,
		"the first row must be committed");

	bool rejected = false;
	try
	{
		(void)archetype.InsertRow(
			registry.Create(), [](Chunk::PendingRow& pending)
			{
				pending.Construct<int>(2);
			});
	}
	catch (const std::logic_error&)
	{
		rejected = true;
	}
	GILGAMESH_CHECK_MESSAGE(rejected && chunk->Count() == 1 &&
		chunk->Get<int>(0) == 1 && chunk->Get<float>(0) == 1.0f,
		"a row with a missing column must not change committed storage");

	// The complete retry should use the same chunk and next row index.
	const auto [reusedChunk, reusedRow] = archetype.InsertRow(
		registry.Create(), [](Chunk::PendingRow& pending)
		{
			pending.Construct<int>(3);
			pending.Construct<float>(3.0f);
		});
	GILGAMESH_CHECK_MESSAGE(reusedChunk == chunk && reusedRow == 1 &&
		chunk->Count() == 2 && chunk->Get<int>(1) == 3,
		"incomplete-row rollback must leave the chunk reusable");
}

GILGAMESH_TEST("Engine.ECS.Archetype", "FullChunkBecomesAvailableAfterErase")
{
	EntityRegistry registry;
	ArchetypeManager manager;
	manager.RegisterComponent<int>();
	const std::array<ComponentTypeId, 1> signature{ ComponentTypeId{ typeid(int) } };
	Archetype& archetype = manager.FindOrCreate(signature);
	Chunk* firstChunk = nullptr;
	bool rowsStayedSequential = true;

	// Fill one chunk and verify all rows used that allocation.
	for (std::uint32_t i = 0; i < archetype.layout.capacity; ++i)
	{
		const auto [chunk, row] = archetype.InsertRow(
			registry.Create(), [i](Chunk::PendingRow& pending)
			{
				pending.Construct<int>(static_cast<int>(i));
			});
		if (firstChunk == nullptr)
			firstChunk = chunk;
		rowsStayedSequential = rowsStayedSequential &&
			chunk == firstChunk && row == i;
	}
	GILGAMESH_CHECK_MESSAGE(rowsStayedSequential && firstChunk != nullptr &&
		firstChunk->IsFull(),
		"rows must fill the first chunk before another is selected");

	// A full chunk is removed from the availability list.
	const auto [secondChunk, secondRow] = archetype.InsertRow(
		registry.Create(), [](Chunk::PendingRow& pending)
		{
			pending.Construct<int>(-1);
		});
	GILGAMESH_CHECK_MESSAGE(secondChunk != firstChunk && secondRow == 0,
		"the next insertion must allocate another chunk");

	// Erasing from the full chunk makes it the next insertion target.
	const auto displaced = archetype.EraseRow(*firstChunk, archetype.layout.capacity - 1);
	GILGAMESH_CHECK_MESSAGE(!displaced && !firstChunk->IsFull(),
		"erasing the final row must reopen the full chunk");
	const auto [reusedChunk, reusedRow] = archetype.InsertRow(
		registry.Create(), [](Chunk::PendingRow& pending)
		{
			pending.Construct<int>(-2);
		});
	GILGAMESH_CHECK_MESSAGE(reusedChunk == firstChunk &&
		reusedRow == archetype.layout.capacity - 1 && firstChunk->IsFull(),
		"the reopened chunk must be reused before another allocation");
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

GILGAMESH_TEST("Engine.ECS.EntityRegistry", "HasRejectsEntitiesWithoutRows")
{
	EntityRegistry registry;
	EntityRegistry otherRegistry;
	const Entity entity = registry.Create();
	const Entity foreign = otherRegistry.Create();

	// New entities have no archetype row, and invalid handles cannot own one.
	GILGAMESH_CHECK_MESSAGE(!registry.Has<int>(entity),
		"a new entity must not report a component before placement");
	GILGAMESH_CHECK_MESSAGE(!registry.Has<int>(Entity{}),
		"the null handle must not report a component");
	GILGAMESH_CHECK_MESSAGE(!registry.Has<int>(foreign),
		"an entity from another registry must not report a component");

	// A destroyed handle remains unable to report components.
	GILGAMESH_CHECK_MESSAGE(registry.Destroy(entity),
		"the entity must be destroyed for the stale-handle check");
	GILGAMESH_CHECK_MESSAGE(!registry.Has<int>(entity),
		"a stale handle must not report a component");
}

GILGAMESH_TEST("Engine.ECS.EntityRegistry", "GetRejectsUnplacedEntity")
{
	EntityRegistry registry;
	const Entity entity = registry.Create();

	// A newly created entity has no constructed component to return by reference.
	bool rejected = false;
	try
	{
		(void)registry.Get<int>(entity);
	}
	catch (const std::out_of_range&)
	{
		rejected = true;
	}
	GILGAMESH_CHECK_MESSAGE(rejected,
		"Get must reject an entity without the requested component");
}

GILGAMESH_TEST("Engine.ECS.EntityRegistry", "EmplaceMigratesAndRepairsSwappedRows")
{
	TrackedComponent::liveCount = 0;
	{
		EntityRegistry registry;
		const Entity first = registry.Create();
		const Entity second = registry.Create();
		const Entity third = registry.Create();

		// Place three entities in one archetype so removing the first swaps the last.
		registry.Emplace<TrackedComponent>(first, 10);
		registry.Emplace<TrackedComponent>(second, 20);
		registry.Emplace<TrackedComponent>(third, 30);
		GILGAMESH_CHECK_MESSAGE(TrackedComponent::liveCount == 3,
			"each entity must own one live component before migration");

		// Adding float moves the first component and repairs the third entity's slot.
		registry.Emplace<float>(first, 1.5f);
		GILGAMESH_CHECK_MESSAGE(registry.Has<float>(first) &&
			registry.Get<float>(first) == 1.5f &&
			registry.Get<TrackedComponent>(first).value &&
			*registry.Get<TrackedComponent>(first).value == 10 &&
			registry.Get<TrackedComponent>(third).value &&
			*registry.Get<TrackedComponent>(third).value == 30 &&
			TrackedComponent::liveCount == 3,
			"migration must preserve values and update the source row swap");

		// A failed constructor must leave the second entity in its source row.
		bool rejected = false;
		try
		{
			registry.Emplace<ThrowingComponent>(second, true);
		}
		catch (const std::runtime_error&)
		{
			rejected = true;
		}
		GILGAMESH_CHECK_MESSAGE(rejected && !registry.Has<ThrowingComponent>(second) &&
			registry.Get<TrackedComponent>(second).value &&
			*registry.Get<TrackedComponent>(second).value == 20 &&
			TrackedComponent::liveCount == 3,
			"failed migration must preserve the source component and slot");

		// Destroying the third entity swaps the second into its row.
		GILGAMESH_CHECK_MESSAGE(registry.Destroy(third) &&
			!registry.IsValid(third) &&
			registry.Get<TrackedComponent>(second).value &&
			*registry.Get<TrackedComponent>(second).value == 20 &&
			TrackedComponent::liveCount == 2,
			"destruction must repair the slot of a swapped-in survivor");

		// Each remaining destruction must end exactly one component lifetime.
		GILGAMESH_CHECK_MESSAGE(registry.Destroy(first) &&
			TrackedComponent::liveCount == 1 &&
			registry.Destroy(second) &&
			TrackedComponent::liveCount == 0,
			"destroying migrated entities must release all component values");
	}
	GILGAMESH_CHECK_MESSAGE(TrackedComponent::liveCount == 0,
		"registry destruction must not leave live components");
}

GILGAMESH_TEST("Engine.ECS.EntityRegistry", "RemoveRetainsValuesAndRepairsSwappedRows")
{
	TrackedComponent::liveCount = 0;
	{
		EntityRegistry registry;
		const Entity first = registry.Create();
		const Entity second = registry.Create();

		// Place both entities in {TrackedComponent, int} before removing row zero.
		registry.Emplace<TrackedComponent>(first, 10);
		registry.Emplace<int>(first, 100);
		registry.Emplace<TrackedComponent>(second, 20);
		registry.Emplace<int>(second, 200);
		GILGAMESH_CHECK_MESSAGE(TrackedComponent::liveCount == 2,
			"both entities must own the component being removed");

		// Removing the first row moves int and swaps the second source row into it.
		registry.Remove<TrackedComponent>(first);
		GILGAMESH_CHECK_MESSAGE(registry.IsValid(first) &&
			!registry.Has<TrackedComponent>(first) &&
			registry.Has<int>(first) && registry.Get<int>(first) == 100 &&
			registry.Has<TrackedComponent>(second) &&
			registry.Get<TrackedComponent>(second).value &&
			*registry.Get<TrackedComponent>(second).value == 20 &&
			registry.Get<int>(second) == 200 &&
			TrackedComponent::liveCount == 1,
			"Remove must destroy T, preserve U, and repair the swapped entity slot");

		// Removing the last component leaves the handle valid but without a row.
		registry.Remove<int>(first);
		bool getRejected = false;
		try
		{
			(void)registry.Get<int>(first);
		}
		catch (const std::out_of_range&)
		{
			getRejected = true;
		}
		GILGAMESH_CHECK_MESSAGE(registry.IsValid(first) &&
			!registry.Has<int>(first) && !registry.Has<TrackedComponent>(first) &&
			getRejected && TrackedComponent::liveCount == 1,
			"removing the last component must clear the row without destroying the entity");

		// Missing and stale removals leave the surviving entity untouched.
		registry.Remove<TrackedComponent>(first);
		registry.Remove<int>(first);
		GILGAMESH_CHECK_MESSAGE(registry.Destroy(first),
			"the componentless entity must remain destructible");
		registry.Remove<int>(first);
		GILGAMESH_CHECK_MESSAGE(registry.IsValid(second) &&
			registry.Get<int>(second) == 200 &&
			registry.Get<TrackedComponent>(second).value &&
			*registry.Get<TrackedComponent>(second).value == 20 &&
			TrackedComponent::liveCount == 1,
			"no-op removals must not disturb a surviving entity");

		GILGAMESH_CHECK_MESSAGE(registry.Destroy(second) &&
			TrackedComponent::liveCount == 0,
			"the remaining tracked component must be destroyed once");
	}
	GILGAMESH_CHECK_MESSAGE(TrackedComponent::liveCount == 0,
		"registry teardown must leave no live tracked components");
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
