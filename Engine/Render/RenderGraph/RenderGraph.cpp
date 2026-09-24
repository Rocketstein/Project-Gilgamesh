#include "RenderGraph.h"

#include <atomic>
#include <cassert>
#include <stdexcept>

namespace
{
	// A process-wide identity keeps handles from different graphs distinct.
	std::uint64_t AcquireRenderGraphOwnerId() noexcept
	{
		static std::atomic<std::uint64_t> nextOwnerId{ 1 };
		return nextOwnerId.fetch_add(1, std::memory_order_relaxed);
	}
} // Anonymous namespace

RenderGraph::RenderGraph() noexcept
	: ownerId_(AcquireRenderGraphOwnerId())
{
}

void RenderGraph::Reset() noexcept
{
	assert(!isAddingPass_);
	if (isAddingPass_)
	{
		return;
	}

	// Destroy callbacks before resources so captured frame data is released now.
	passes_.clear();
	textures_.clear();

	// A fresh generation prevents reused indices from reviving stale handles.
	++generation_;
	if (generation_ == 0)
	{
		ownerId_ = AcquireRenderGraphOwnerId();
		generation_ = 1;
	}
}

RenderGraphTextureHandle RenderGraph::CreateTexture(
	std::string_view name,
	const RenderGraphTextureDesc& description)
{
	if (isAddingPass_)
	{
		throw std::logic_error(
			"RenderGraph resources cannot be created inside pass setup");
	}

	if (textures_.size() >= RenderGraphTextureHandle::InvalidIndex)
	{
		throw std::length_error("RenderGraph texture capacity exceeded");
	}

	const auto index = static_cast<std::uint32_t>(textures_.size());
	textures_.push_back(TextureRecord{
		std::string(name),
		description,
		std::nullopt
	});

	return RenderGraphTextureHandle(index, generation_, ownerId_);
}

RenderGraphTextureHandle RenderGraph::ImportTexture(
	std::string_view name,
	const RenderGraphTextureDesc& description,
	const ImportedRenderGraphTexture& importedTexture)
{
	if (isAddingPass_)
	{
		throw std::logic_error(
			"RenderGraph resources cannot be imported inside pass setup");
	}

	if (textures_.size() >= RenderGraphTextureHandle::InvalidIndex)
	{
		throw std::length_error("RenderGraph texture capacity exceeded");
	}

	const auto index = static_cast<std::uint32_t>(textures_.size());
	textures_.push_back(TextureRecord{
		std::string(name),
		description,
		importedTexture
	});

	return RenderGraphTextureHandle(index, generation_, ownerId_);
}

bool RenderGraph::IsValid(RenderGraphTextureHandle handle) const noexcept
{
	return handle.ownerId_ == ownerId_
		&& handle.generation_ == generation_
		&& handle.index_ < textures_.size();
}
