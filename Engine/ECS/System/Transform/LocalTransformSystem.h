#pragma once

#include "Engine/ECS/System/Core/ISystem.h"

struct TransformComponent;

// Evaluates parent-relative TRS as row-vector local matrices, independently of hierarchy order.
class LocalTransformSystem final : public ISystem
{
public:
	// Run before LocalToWorldSystem. Compare every authored TRS-derived matrix with its
	// cache so current direct field edits remain observable without a dirty-flag API.
	// Initialize localRevision on first evaluation, then advance it only on matrix changes.
	void Update(World& world, float deltaTime) override;

private:
	// Compose S * R * T and update only the local matrix/revision; never clear world changes.
	static void UpdateLocalMatrix(TransformComponent& transform) noexcept;
};
