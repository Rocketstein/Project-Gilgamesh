#include "MeshManager.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace
{
	// Converts an element count into D3D11-safe 32-bit count and byte width values.
	template<typename Element>
	bool TryGetBufferMetrics(
		std::size_t elementCount,
		std::uint32_t& count,
		std::uint32_t& byteWidth) noexcept
	{
		constexpr auto MaxBufferSize =
			std::numeric_limits<std::uint32_t>::max();

		if (elementCount == 0 ||
			elementCount > MaxBufferSize / sizeof(Element))
		{
			return false;
		}

		count = static_cast<std::uint32_t>(elementCount);
		byteWidth = static_cast<std::uint32_t>(elementCount * sizeof(Element));
		return true;
	}
} // Anonymous namespace

bool MeshManager::Initialize(ID3D11Device* device)
{
	if (!device) return false;
	if (device_ == device) return true;

	Shutdown();
	device_ = device;
	return true;
}

void MeshManager::Shutdown()
{
	std::vector<std::uint32_t> freeIndices;
	freeIndices.reserve(meshSlots_.size());

	for (std::size_t index = 0; index < meshSlots_.size(); ++index)
	{
		MeshSlot& slot = meshSlots_[index];
		if (slot.mesh &&
			slot.generation != std::numeric_limits<std::uint32_t>::max())
		{
			++slot.generation;
		}

		slot.mesh.reset();
		if (slot.generation != std::numeric_limits<std::uint32_t>::max())
		{
			freeIndices.push_back(static_cast<std::uint32_t>(index));
		}
	}

	freeIndices_ = std::move(freeIndices);
	liveCount_ = 0;
	device_ = nullptr;
}

MeshHandle MeshManager::CreateMesh(const StaticMeshData& data)
{
	if (!device_) return {};

	std::uint32_t vertexCount = 0;
	std::uint32_t vertexByteWidth = 0;
	if (!TryGetBufferMetrics<NormalVertex>(
		data.vertices_.size(),
		vertexCount,
		vertexByteWidth))
	{
		return {};
	}

	std::uint32_t indexCount = 0;
	std::uint32_t indexByteWidth = 0;
	if (!TryGetBufferMetrics<std::uint32_t>(
		data.indices_.size(),
		indexCount,
		indexByteWidth))
	{
		return {};
	}

	for (const std::uint32_t index : data.indices_)
	{
		if (index >= data.vertices_.size()) return {};
	}

	VertexBuffer vertexBuffer;
	const HRESULT vertexResult = vertexBuffer.Create(
		device_,
		vertexByteWidth,
		vertexCount,
		static_cast<std::uint32_t>(sizeof(NormalVertex)),
		data.vertices_.data());
	if (FAILED(vertexResult)) return {};

	IndexBuffer indexBuffer;
	const HRESULT indexResult = indexBuffer.Create(
		device_,
		indexByteWidth,
		indexCount,
		data.indices_.data());
	if (FAILED(indexResult)) return {};

	return AddMesh(StaticMesh{
		std::move(vertexBuffer),
		std::move(indexBuffer)
	});
}

MeshHandle MeshManager::AddMesh(StaticMesh mesh)
{
	if (freeIndices_.empty() && meshSlots_.size() >= MeshHandle::InvalidIndex)
	{
		throw std::length_error("MeshManager handle capacity exceeded");
	}

	auto storedMesh = std::make_unique<StaticMesh>(std::move(mesh));

	if (!freeIndices_.empty())
	{
		const std::uint32_t index = freeIndices_.back();
		freeIndices_.pop_back();

		MeshSlot& slot = meshSlots_[index];
		slot.mesh = std::move(storedMesh);
		++liveCount_;
		return MeshHandle(index, slot.generation);
	}

	const auto index = static_cast<std::uint32_t>(meshSlots_.size());
	meshSlots_.push_back(MeshSlot{ 0, std::move(storedMesh) });
	++liveCount_;
	return MeshHandle(index, 0);
}

bool MeshManager::IsValid(MeshHandle handle) const noexcept
{
	if (handle.index_ >= meshSlots_.size())
	{
		return false;
	}

	const MeshSlot& slot = meshSlots_[handle.index_];
	return slot.mesh != nullptr && slot.generation == handle.generation_;
}

const StaticMesh* MeshManager::TryGet(MeshHandle handle) const noexcept
{
	return IsValid(handle)
		? meshSlots_[handle.index_].mesh.get()
		: nullptr;
}

bool MeshManager::RemoveMesh(MeshHandle handle)
{
	if (!IsValid(handle))
	{
		return false;
	}

	MeshSlot& slot = meshSlots_[handle.index_];
	if (slot.generation != std::numeric_limits<std::uint32_t>::max())
	{
		// Grow the free list before mutating the slot so allocation failure
		// leaves the mesh and handle valid.
		freeIndices_.push_back(handle.index_);
		++slot.generation;
	}

	slot.mesh.reset();
	--liveCount_;
	return true;
}
