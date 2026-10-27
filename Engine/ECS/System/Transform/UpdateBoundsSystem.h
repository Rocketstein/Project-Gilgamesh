#pragma once

#include "Engine/ECS/System/Core/ISystem.h"

class MeshManager;
struct AABBComponent;
struct MeshComponent;
struct TransformComponent;

// Evaluates an entity's own mesh bounds, not aggregate subtree or spatial-tree bounds.
// Register after LocalToWorldSystem; the later spatial-index/extraction stages consume revisions.
class UpdateBoundsSystem final : public ISystem
{
public:
	// Borrow CPU mesh-bound metadata without accessing a D3D11 device or context.
	// The mesh manager must outlive this system and its containing World.
	explicit UpdateBoundsSystem(const MeshManager& meshes) noexcept;

	// Update existing Transform + Mesh + AABB components; never add components in this phase.
	// Use original mesh-local bounds and the full world matrix, including shear/negative scale.
	// Initialize the bounds revision even on a first invalid result; then advance on changes.
	// Revisions are publication tokens that are not reused when a component is recreated.
	// Missing geometry or an unevaluated world matrix produces valid=false. Never clear
	// transform revisions; entity deletion and invalidation must also reach the spatial index.
	void Update(World& world, float deltaTime) override;

private:
	// Check this consumer's observed world, mesh identity, and local-bounds revision.
	[[nodiscard]] bool NeedsBoundsUpdate(
		const TransformComponent& transform,
		const MeshComponent& mesh,
		const AABBComponent& bounds) const noexcept;

	// Publish transformed bounds, validity, and this consumer's evaluated source revisions.
	void UpdateWorldBounds(
		const TransformComponent& transform,
		const MeshComponent& mesh,
		AABBComponent& bounds) const noexcept;

private:
	const MeshManager& meshes_; // Borrowed shared local bounds and their resource revisions.
};
