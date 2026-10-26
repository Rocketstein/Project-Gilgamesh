#include "TransformHierarchySystem.h"

#include <cstddef>
#include <limits>
#include <set>
#include <stdexcept>

#include "Engine/Core/Logging/Logger.h"
#include "Engine/ECS/Component/TransformComponent.h"
#include "Engine/ECS/Framework/World.h"

namespace
{
// Track parent-chain walks without recursion or persistent state in components.
enum class VisitState
{
	Unvisited,
	Visiting,
	Complete
};
} // Anonymous namespace

// Reuse the published hierarchy until its authored links or transform membership change.
void TransformHierarchySystem::Update(World& world, float /*deltaTime*/)
{
	if (topologyDirty_ || HasTopologyChanged(world))
	{
		RebuildHierarchy(world);
	}
}

// Validate authored links immediately while leaving cached queries unchanged until Update.
bool TransformHierarchySystem::SetParent(World& world, Entity child, Entity parent)
{
	// Has also validates complete identity, world ownership, and committed entity state.
	if (!world.Has<TransformComponent>(child))
	{
		GILGAMESH_LOG(Runtime, Warning,
			"Cannot set transform parent: the child has no live TransformComponent.");
		return false;
	}
	if (!parent.IsNull() && !world.Has<TransformComponent>(parent))
	{
		GILGAMESH_LOG(Runtime, Warning,
			"Cannot set transform parent: the parent has no live TransformComponent in this World.");
		return false;
	}

	// Inspect current authored links so multiple edits before Update cannot introduce a cycle.
	if (WouldCreateCycle(world, child, parent))
	{
		GILGAMESH_LOG(Runtime, Warning,
			"Cannot set transform parent: the requested ancestry contains a cycle.");
		return false;
	}

	// Preserve all authored TRS and evaluated matrices; only the relationship is changed.
	TransformComponent& transform = world.Get<TransformComponent>(child);
	if (transform.parent != parent)
	{
		transform.parent = parent;
		topologyDirty_ = true;
	}
	return true;
}

// Query only the last published transform membership, not the live registry.
bool TransformHierarchySystem::Contains(Entity entity) const noexcept
{
	return nodes_.contains(entity);
}

// Unknown entities and validated roots both have a null evaluated parent.
Entity TransformHierarchySystem::GetParent(Entity entity) const noexcept
{
	const auto node = nodes_.find(entity);
	return node != nodes_.end() ? node->second.parent : Entity{};
}

// Distinguish a root at depth zero from an entity absent from the published cache.
std::int32_t TransformHierarchySystem::GetDepth(Entity entity) const noexcept
{
	const auto node = nodes_.find(entity);
	return node != nodes_.end() ? node->second.depth : -1;
}

// Borrow the published forest's roots without allocating a query result.
std::span<const Entity> TransformHierarchySystem::GetRoots() const noexcept
{
	return roots_;
}

// Borrow direct children, or an empty view when the parent is not in the hierarchy.
std::span<const Entity> TransformHierarchySystem::GetChildren(Entity parent) const noexcept
{
	const auto node = nodes_.find(parent);
	if (node == nodes_.end()) return {};
	return node->second.children;
}

// Borrow an order in which each parent's world matrix can be evaluated before its children.
std::span<const Entity> TransformHierarchySystem::GetTraversalOrder() const noexcept
{
	return traversalOrder_;
}

// Keep the last published generation across cache invalidation and re-enablement.
std::uint64_t TransformHierarchySystem::GetRevision() const noexcept
{
	return revision_;
}

// Drop borrowed views; the next enabled update must rebuild from current authored links.
void TransformHierarchySystem::OnDisable()
{
	ClearCache();
}

// Release derived topology while leaving entity relationships and transforms untouched.
void TransformHierarchySystem::OnDestroy(World& /*world*/)
{
	ClearCache();
}

// Detect direct edits and complete-handle membership changes without observing TRS values.
bool TransformHierarchySystem::HasTopologyChanged(World& world) const
{
	const World& readOnlyWorld = world;
	std::size_t transformCount = 0;
	bool changed = false;
	readOnlyWorld.ForEachEntity([&](Entity entity)
	{
		if (!readOnlyWorld.Has<TransformComponent>(entity)) return;
		++transformCount;

		// Compare original links, not repaired ones, so unchanged invalid input stays cached.
		const auto node = nodes_.find(entity);
		if (node == nodes_.end() || node->second.sourceParent !=
			readOnlyWorld.Get<TransformComponent>(entity).parent)
		{
			changed = true;
		}
	});
	return changed || transformCount != nodes_.size();
}

// Build a validated forest privately and publish it only after every allocation succeeds.
void TransformHierarchySystem::RebuildHierarchy(World& world)
{
	std::map<Entity, HierarchyNode> nextNodes;
	std::vector<Entity> nextRoots;
	std::vector<Entity> nextTraversalOrder;
	const World& readOnlyWorld = world;

	// Gather every live transform, including non-renderable and scene-less parents.
	readOnlyWorld.ForEachEntity([&](Entity entity)
	{
		if (!readOnlyWorld.Has<TransformComponent>(entity)) return;
		HierarchyNode& node = nextNodes[entity];
		node.sourceParent = readOnlyWorld.Get<TransformComponent>(entity).parent;
		node.parent = node.sourceParent;
	});

	// Retain raw input for change detection, but detach missing, foreign, or self parents.
	for (auto& [entity, node] : nextNodes)
	{
		if (node.parent.IsNull()) continue;
		if (node.parent == entity || !nextNodes.contains(node.parent))
		{
			GILGAMESH_LOG(Runtime, Warning,
				"Invalid transform parent link; evaluating the child as a root.");
			node.parent = {};
		}
	}

	// Cut the edge closing each cycle. Ordered node iteration makes this deterministic.
	// Iterative parent walks avoid consuming the call stack for deep hierarchies.
	std::map<Entity, VisitState> visits;
	std::vector<Entity> path;
	for (const auto& entry : nextNodes)
	{
		const Entity entity = entry.first;
		if (visits[entity] == VisitState::Complete) continue;
		path.clear();
		Entity current = entity;
		while (!current.IsNull() && visits[current] == VisitState::Unvisited)
		{
			visits[current] = VisitState::Visiting;
			path.push_back(current);
			HierarchyNode& currentNode = nextNodes.at(current);
			if (!currentNode.parent.IsNull() &&
				visits[currentNode.parent] == VisitState::Visiting)
			{
				GILGAMESH_LOG(Runtime, Warning,
					"Transform hierarchy cycle detected; evaluating the closing node as a root.");
				currentNode.parent = {};
				break;
			}
			current = currentNode.parent;
		}
		for (Entity visited : path)
		{
			visits[visited] = VisitState::Complete;
		}
	}

	// Reconstruct adjacency from validated links; components never own child lists.
	nextRoots.reserve(nextNodes.size());
	nextTraversalOrder.reserve(nextNodes.size());
	for (const auto& [entity, node] : nextNodes)
	{
		if (node.parent.IsNull())
		{
			nextRoots.push_back(entity);
		}
		else
		{
			nextNodes.at(node.parent).children.push_back(entity);
		}
	}

	// Use the output vector as a breadth-first queue to assign depths and parent-first order.
	nextTraversalOrder = nextRoots;
	for (std::size_t index = 0; index < nextTraversalOrder.size(); ++index)
	{
		const HierarchyNode& node = nextNodes.at(nextTraversalOrder[index]);
		for (Entity child : node.children)
		{
			if (node.depth == std::numeric_limits<std::int32_t>::max())
			{
				GILGAMESH_LOG(Runtime, Error, "Transform hierarchy depth exceeds the supported range.");
				throw std::overflow_error("Transform hierarchy depth overflow");
			}
			nextNodes.at(child).depth = node.depth + 1;
			nextTraversalOrder.push_back(child);
		}
	}

	// Swapping candidate storage preserves the old published cache if construction fails.
	nodes_.swap(nextNodes);
	roots_.swap(nextRoots);
	traversalOrder_.swap(nextTraversalOrder);
	++revision_;
	topologyDirty_ = false;
}

// Reject ancestry reaching the child or entering an already-corrupt authored cycle.
bool TransformHierarchySystem::WouldCreateCycle(
	const World& world, Entity child, Entity parent) const
{
	std::set<Entity> visited;
	while (!parent.IsNull())
	{
		if (parent == child || !visited.insert(parent).second) return true;

		// Invalid ancestry terminates here; rebuilding will evaluate its broken link as a root.
		if (!world.Has<TransformComponent>(parent)) return false;
		parent = world.Get<TransformComponent>(parent).parent;
	}
	return false;
}

// Invalidate query storage without reusing a previously published revision number.
void TransformHierarchySystem::ClearCache() noexcept
{
	nodes_.clear();
	roots_.clear();
	traversalOrder_.clear();
	topologyDirty_ = true;
}
