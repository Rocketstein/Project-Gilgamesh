#pragma once

#include <cstdint>

#include "Engine/Core/Math/Matrix.h"
#include "Engine/Core/Math/Quat.h"
#include "Engine/ECS/Entity/Entity.h"

// Authored parent-relative TRS and system-owned matrix caches; roots use world-relative TRS.
// Evaluation order is hierarchy -> local matrix -> world matrix -> bounds -> extraction.
struct TransformComponent
{
	Vector3 position_{}; // Position relative to the parent, or world origin for a root.
	Quat rotation_ = Quat::Identity(); // Orientation relative to the parent.
	Vector3 scale{ 1.0f, 1.0f, 1.0f }; // Unit scale keeps a default mesh visible.
	Entity parent{}; // Authored parent link; null denotes a root. No child list is authored.

	// Only transform systems write these caches; keep the full affine world matrix.
	Matrix4 localToParent = Matrix4::Identity(); // LocalTransformSystem evaluates S * R * T.
	Matrix4 localToWorld = Matrix4::Identity(); // LocalToWorldSystem evaluates local * parentWorld.
	std::uint64_t localRevision = 0; // Zero until first local evaluation, then advances on changes.
	std::uint64_t worldRevision = 0; // Zero until first world evaluation, then advances on changes.

	// LocalToWorldSystem tracks its own inputs without clearing another consumer's state.
	Entity evaluatedParent{}; // Last validated parent used to evaluate localToWorld.
	std::uint64_t evaluatedLocalRevision = 0; // Local matrix revision consumed by world evaluation.
	std::uint64_t evaluatedParentWorldRevision = 0; // Parent world revision consumed; zero for roots.
};
