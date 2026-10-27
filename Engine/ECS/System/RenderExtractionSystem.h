#pragma once

#include <optional>

#include "Engine/ECS/System/Core/ISystem.h"
#include "Engine/Render/Renderer/RenderFrameData.h"

// Copies the active scene after structural playback and transform evaluation.
class RenderExtractionSystem final : public ISystem
{
public:
	// Changes bounds visibility for the next extraction without altering an existing snapshot.
	void SetShowBounds(bool showBounds) noexcept { showBounds_ = showBounds; }

	void Update(World& world, float deltaTime) override;

	// Transfers the completed snapshot exactly once, including an empty scene result.
	[[nodiscard]] std::optional<RenderFrameData> TakeFrame();
	// Drops an unconsumed result when presentation is skipped or disabled.
	void DiscardFrame() noexcept;

	void OnDisable() override;
	void OnDestroy(World& world) override;

private:
	bool showBounds_ = false; // Opt-in world-space AABB diagnostics for future snapshots.
	std::optional<RenderFrameData> pendingFrame_; // At most one completed, unconsumed result.
};
