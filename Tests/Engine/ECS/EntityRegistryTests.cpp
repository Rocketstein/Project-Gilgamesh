#include "Engine/ECS/Entity/EntityRegistry.h"
#include "Engine/ECS/Framework/World.h"
#include "Tests/TestFramework.h"

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
