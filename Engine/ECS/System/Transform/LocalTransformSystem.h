#pragma once

#include <cstdint>

#include "Engine/ECS/System/Core/ISystem.h"

struct TransformComponent;

// Evaluates parent-relative TRS as row-vector local matrices, independently of hierarchy order.
class LocalTransformSystem final : public ISystem
{
public:
	// Run before LocalToWorldSystem. Evaluate chunks written since this consumer's last pass.
	// Initialize localRevision on first evaluation, then advance it only on matrix changes.
	void Update(World& world, float deltaTime) override;

	// Restart observation across lifecycle changes without discarding evaluated component caches.
	void OnEnable() override;
	void OnDisable() override;
	void OnDestroy(World& world) override;

private:
	// Compose S * R * T and update only the local matrix/revision; never clear world changes.
	static void UpdateLocalMatrix(TransformComponent& transform) noexcept;

private:
	// Last fully consumed ECS write stamp; zero examines all initialized chunks.
	std::uint64_t lastObservedVersion_ = 0;
};
