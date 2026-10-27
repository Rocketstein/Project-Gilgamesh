#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "Engine/Core/BoundingShape.h"
#include "StaticMeshData.h"
#include "StaticMesh.h"
#include "MeshHandle.h"

// Owns uploaded meshes and immutable CPU metadata for generation-checked handles.
class MeshManager final
{
public:
	MeshManager() = default;
	~MeshManager() = default;

	MeshManager(const MeshManager&) = delete;
	MeshManager& operator=(const MeshManager&) = delete;
	MeshManager(MeshManager&&) = delete;
	MeshManager& operator=(MeshManager&&) = delete;

	[[nodiscard]]
	bool Initialize(ID3D11Device* device);

	void Shutdown();

	// Uploads CPU mesh data and returns its managed handle, or an invalid handle on failure.
	[[nodiscard]]
	MeshHandle CreateMesh(const StaticMeshData& data);

	[[nodiscard]]
	bool IsInitialized() const noexcept { return device_ != nullptr; }

	[[nodiscard]]
	std::size_t Num() const noexcept { return liveCount_; }

	// Reports whether a handle still refers to a live mesh in its current generation.
	[[nodiscard]]
	bool IsValid(MeshHandle handle) const noexcept;

	// Returns a stable mesh pointer for a valid handle, or nullptr for an invalid one.
	[[nodiscard]]
	const StaticMesh* TryGet(MeshHandle handle) const noexcept;

	// Copy shared CPU-local bounds and their revision without resolving GPU buffers.
	// Return false without changing outputs when the handle or metadata is invalid.
	[[nodiscard]] bool TryGetLocalBounds(
		MeshHandle handle,
		BoundingBox& outBounds,
		std::uint64_t& outRevision) const noexcept;

	// Releases the referenced mesh and invalidates its handle for future lookups.
	[[nodiscard]]
	bool RemoveMesh(MeshHandle handle);

private:
	// Stores an uploaded mesh in a new or recycled slot and returns its handle.
	[[nodiscard]]
	MeshHandle AddMesh(StaticMesh mesh, const BoundingBox& localBounds);

	// A slot keeps CPU bounds alive for exactly the lifetime of its uploaded mesh.
	struct MeshSlot
	{
		std::uint32_t generation = 0;
		std::unique_ptr<StaticMesh> mesh;
		BoundingBox localBounds{}; // Shared CPU metadata, separate from the GPU mesh wrapper.
		std::uint64_t boundsRevision = 0; // Live immutable meshes use one; empty slots use zero.
	};

	std::vector<MeshSlot> meshSlots_;
	std::vector<std::uint32_t> freeIndices_;
	std::size_t liveCount_ = 0;

	ID3D11Device* device_ = nullptr;
};
