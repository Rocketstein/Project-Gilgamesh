#pragma once

#include <cstdint>
#include <map>
#include <span>
#include <vector>

#include "Engine/ECS/Entity/Entity.h"
#include "Engine/ECS/System/Core/ISystem.h"

// Maintains a world-wide, parent-before-child traversal of TransformComponent entities.
// Parent links are authored in components; children, depths, and traversal order are derived.
class TransformHierarchySystem final : public ISystem
{
public:
	// Run after structural playback and before local/world evaluation. Detect direct parent
	// edits and entity/component removal; rebuild caches only when topology changes.
	// Invalid direct links are logged and evaluated as roots without rewriting authored TRS.
	// Cycles are repaired in the cache by detaching the edge that closes an ordered parent walk.
	void Update(World& world, float deltaTime) override;

	// Change a live child's parent immediately while preserving local TRS, before presentation.
	// Null detaches to a root. Reject missing transforms, foreign/stale handles, and cycles.
	// Same-world cross-scene links are allowed. Reserved entities must finish playback first.
	// Do not call during hierarchy/world evaluation or extraction. Returns false on rejection.
	[[nodiscard]] bool SetParent(World& world, Entity child, Entity parent);

	// Test membership in the most recently completed hierarchy evaluation.
	[[nodiscard]] bool Contains(Entity entity) const noexcept;

	// Return the validated parent, or a null entity for a root or unknown entity.
	[[nodiscard]] Entity GetParent(Entity entity) const noexcept;

	// Return zero-based depth, or -1 when the entity is not in the evaluated hierarchy.
	[[nodiscard]] std::int32_t GetDepth(Entity entity) const noexcept;

	// Borrowed query views expire on the next Update, disable, or destruction.
	// Parent edits are reflected in these views only after Update completes.
	[[nodiscard]] std::span<const Entity> GetRoots() const noexcept;
	[[nodiscard]] std::span<const Entity> GetChildren(Entity parent) const noexcept;
	[[nodiscard]] std::span<const Entity> GetTraversalOrder() const noexcept;

	// Zero means no successful build yet; each published rebuild advances the revision.
	// Cache invalidation retains the last published value; re-enabling publishes a newer value.
	[[nodiscard]] std::uint64_t GetRevision() const noexcept;

	// Invalidate derived views so re-enabling requires a fresh topology evaluation.
	void OnDisable() override;
	void OnDestroy(World& world) override;

private:
	// Compare live transform identities and authored parents with the cached topology.
	[[nodiscard]] bool HasTopologyChanged(World& world) const;

	// Validate links and rebuild adjacency, roots, depths, and a parent-first traversal.
	void RebuildHierarchy(World& world);

	// Follow authored links to reject self-parenting and ancestry cycles before mutation.
	[[nodiscard]] bool WouldCreateCycle(const World& world, Entity child, Entity parent) const;

	// Drop all borrowed-view storage and request a complete rebuild.
	void ClearCache() noexcept;

private:
	// One validated node; full Entity handles prevent recycled indices from aliasing nodes.
	struct HierarchyNode
	{
		Entity sourceParent{}; // Authored link observed during the latest rebuild.
		Entity parent{}; // Validated parent used by world-matrix evaluation.
		std::vector<Entity> children; // Derived direct descendants; never component pointers.
		std::int32_t depth = 0; // Root depth is zero.
	};

	std::map<Entity, HierarchyNode> nodes_; // Validated topology for the owning World's transforms.
	std::vector<Entity> roots_; // Entry points for complete hierarchy traversal.
	std::vector<Entity> traversalOrder_; // Every parent precedes each of its descendants.
	std::uint64_t revision_ = 0; // Published topology generation; zero is not yet evaluated.
	bool topologyDirty_ = true; // Explicit parent changes and cache invalidation request a rebuild.
};
