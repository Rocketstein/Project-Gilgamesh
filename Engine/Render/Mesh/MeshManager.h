#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "Engine/Core/BoundingShape.h"
#include "StaticMeshData.h"
#include "StaticMesh.h"
#include "MeshHandle.h"

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
	// This API is currently a stub; CreateMesh does not populate bounds metadata yet.
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
	MeshHandle AddMesh(StaticMesh mesh);

	struct MeshSlot
	{
		std::uint32_t generation = 0;
		std::unique_ptr<StaticMesh> mesh;
		BoundingBox localBounds{}; // Shared CPU metadata, separate from the GPU mesh wrapper.
		std::uint64_t boundsRevision = 0; // Zero until local bounds have been evaluated.
	};

	std::vector<MeshSlot> meshSlots_;
	std::vector<std::uint32_t> freeIndices_;
	std::size_t liveCount_ = 0;

	ID3D11Device* device_ = nullptr;
};
