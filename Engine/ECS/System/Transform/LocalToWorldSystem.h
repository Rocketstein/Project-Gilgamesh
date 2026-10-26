#pragma once

#include "Engine/ECS/Entity/Entity.h"
#include "Engine/ECS/System/Core/ISystem.h"

class TransformHierarchySystem;
struct TransformComponent;

// Propagates complete affine world matrices in the hierarchy's parent-first order.
// Register after TransformHierarchySystem and LocalTransformSystem in the transform group.
class LocalToWorldSystem final : public ISystem
{
public:
	// The hierarchy belongs to the same World and must outlive this dependent system.
	explicit LocalToWorldSystem(const TransformHierarchySystem& hierarchy) noexcept;

	// Re-evaluate uninitialized nodes or changed local/parent inputs, including ancestor motion.
	// Root world = local; child world = local * parentWorld. Preserve shear in Matrix4.
	// Hierarchy and local evaluation must finish first; keep storage/topology stable until done.
	// Log and skip inputs whose local or parent world revision is still zero.
	// Record consumed inputs even for equal results; publish a revision on first/change only.
	// World revisions are publication tokens; component/system recreation does not reuse them.
	void Update(World& world, float deltaTime) override;

private:
	// Compare the last consumed local revision, validated parent identity, and parent revision.
	[[nodiscard]] static bool NeedsWorldUpdate(
		const TransformComponent& transform,
		Entity parent,
		const TransformComponent* parentTransform) noexcept;

	// Publish the complete matrix, observed inputs, and this entity's world revision.
	static void UpdateWorldMatrix(
		TransformComponent& transform,
		Entity parent,
		const TransformComponent* parentTransform) noexcept;

private:
	const TransformHierarchySystem& hierarchy_; // Borrowed topology and evaluated parent links.
};
