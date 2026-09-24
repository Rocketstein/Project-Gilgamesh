#include "RenderGraph.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <format>
#include <functional>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

#include "Engine/Core/Logging/Logger.h"

namespace
{
	// A process-wide identity keeps handles from different graphs distinct.
	std::uint64_t AcquireRenderGraphOwnerId() noexcept
	{
		static std::atomic<std::uint64_t> nextOwnerId{ 1 };
		return nextOwnerId.fetch_add(1, std::memory_order_relaxed);
	}

	// Typed depth formats cannot be used as color render targets.
	bool IsTypedDepthFormat(DXGI_FORMAT format) noexcept
	{
		switch (format)
		{
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			return true;
		default:
			return false;
		}
	}

	// Typeless depth families can have a typed depth-stencil view.
	bool IsDepthCompatibleFormat(DXGI_FORMAT format) noexcept
	{
		if (IsTypedDepthFormat(format))
		{
			return true;
		}

		switch (format)
		{
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_R32G8X24_TYPELESS:
			return true;
		default:
			return false;
		}
	}

	// One pass has at most one combined use of a given texture in the DAG.
	struct TextureUse
	{
		std::size_t passIndex = 0;
		bool reads = false;
		bool writes = false;
	};
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
	compiledPassOrder_.clear();
	isCompiled_ = false;
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

bool RenderGraph::Compile()
{
	const auto fail = [](std::string message)
	{
		GILGAMESH_LOG(Renderer, Error, "RenderGraph compile failed: {}", message);
		return false;
	};

	if (isAddingPass_)
	{
		return fail("Compile cannot run inside pass setup");
	}

	// A failed recompilation must never leave a stale executable order visible.
	compiledPassOrder_.clear();
	isCompiled_ = false;

	// Reject descriptions that could not represent a 2D D3D11 texture.
	for (const TextureRecord& texture : textures_)
	{
		const RenderGraphTextureDesc& description = texture.description;
		if (description.extent.width == 0
			|| description.extent.height == 0
			|| description.format == DXGI_FORMAT_UNKNOWN
			|| description.mipLevels == 0
			|| description.sampleCount == 0
			|| (description.sampleCount > 1 && description.mipLevels != 1))
		{
			return fail(std::format(
				"texture '{}' has an invalid description",
				texture.name));
		}
	}

	// One native texture must have one graph handle so aliases cannot bypass hazards.
	for (std::size_t textureIndex = 0; textureIndex < textures_.size(); ++textureIndex)
	{
		const auto& imported = textures_[textureIndex].importedTexture;
		if (!imported || !imported->texture)
		{
			continue;
		}

		for (std::size_t earlierIndex = 0; earlierIndex < textureIndex; ++earlierIndex)
		{
			const auto& earlier = textures_[earlierIndex].importedTexture;
			if (earlier && earlier->texture == imported->texture)
			{
				return fail(std::format(
					"imported textures '{}' and '{}' alias the same native texture",
					textures_[earlierIndex].name,
					textures_[textureIndex].name));
			}
		}
	}

	using AccessType = GraphPassBuilder::TextureAccess::Type;
	constexpr std::uint8_t Read = 1 << 0;
	constexpr std::uint8_t Write = 1 << 1;
	constexpr std::uint8_t Attachment = 1 << 2;
	std::vector<std::vector<TextureUse>> usesByTexture(textures_.size());

	// Validate bindings within each pass and collapse duplicate shader reads.
	for (std::size_t passIndex = 0; passIndex < passes_.size(); ++passIndex)
	{
		const PassDeclaration& pass = passes_[passIndex];
		std::array<bool, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> colorSlots{};
		std::array<bool, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> vertexSlots{};
		std::array<bool, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> pixelSlots{};
		std::vector<std::uint8_t> useFlags(textures_.size(), 0);
		const RenderGraphTextureDesc* firstAttachment = nullptr;
		bool hasDepthTarget = false;

		for (const GraphPassBuilder::TextureAccess& access : pass.textureAccesses)
		{
			if (!IsValid(access.texture))
			{
				return fail(std::format(
					"pass '{}' references an invalid texture handle",
					pass.name));
			}

			const std::size_t textureIndex = access.texture.index_;
			const TextureRecord& texture = textures_[textureIndex];
			const RenderGraphTextureDesc& description = texture.description;
			const ImportedRenderGraphTexture* imported = texture.importedTexture
				? &*texture.importedTexture
				: nullptr;
			std::uint8_t& flags = useFlags[textureIndex];

			if (imported && !imported->texture)
			{
				return fail(std::format(
					"pass '{}' uses imported texture '{}' without a native texture",
					pass.name,
					texture.name));
			}

			// Attachments share one output extent/sample count and exclude SRV use.
			if (access.type == AccessType::ColorTarget
				|| access.type == AccessType::DepthTarget)
			{
				if (flags != 0)
				{
					return fail(std::format(
						"pass '{}' binds texture '{}' both as an attachment and another view",
						pass.name,
						texture.name));
				}

				if (firstAttachment
					&& (firstAttachment->extent != description.extent
						|| firstAttachment->sampleCount != description.sampleCount))
				{
					return fail(std::format(
						"pass '{}' has attachments with different extents or sample counts",
						pass.name));
				}
				firstAttachment = &description;
			}

			switch (access.type)
			{
			case AccessType::ColorTarget:
				if (access.loadOp != RenderGraphLoadOp::Load
					&& access.loadOp != RenderGraphLoadOp::Clear)
				{
					return fail(std::format(
						"pass '{}' has an invalid color load operation",
						pass.name));
				}
				if (access.slot >= colorSlots.size() || colorSlots[access.slot])
				{
					return fail(std::format(
						"pass '{}' has an invalid or duplicate color target slot {}",
						pass.name,
						access.slot));
				}
				if (IsTypedDepthFormat(description.format))
				{
					return fail(std::format(
						"pass '{}' uses depth texture '{}' as a color target",
						pass.name,
						texture.name));
				}
				if (imported && !imported->renderTargetView)
				{
					return fail(std::format(
						"pass '{}' uses imported texture '{}' without a render target view",
						pass.name,
						texture.name));
				}
				colorSlots[access.slot] = true;
				flags = Attachment | Write;
				if (access.loadOp == RenderGraphLoadOp::Load)
				{
					flags |= Read;
				}
				break;

			case AccessType::DepthTarget:
				if (access.loadOp != RenderGraphLoadOp::Load
					&& access.loadOp != RenderGraphLoadOp::Clear)
				{
					return fail(std::format(
						"pass '{}' has an invalid depth load operation",
						pass.name));
				}
				if (access.depthAccess != RenderGraphDepthAccess::ReadOnly
					&& access.depthAccess != RenderGraphDepthAccess::ReadWrite)
				{
					return fail(std::format(
						"pass '{}' has an invalid depth access mode",
						pass.name));
				}
				if (hasDepthTarget)
				{
					return fail(std::format(
						"pass '{}' declares more than one depth target",
						pass.name));
				}
				if (!IsDepthCompatibleFormat(description.format))
				{
					return fail(std::format(
						"pass '{}' uses non-depth texture '{}' as a depth target",
						pass.name,
						texture.name));
				}
				if (access.depthAccess == RenderGraphDepthAccess::ReadOnly
					&& access.loadOp == RenderGraphLoadOp::Clear)
				{
					return fail(std::format(
						"pass '{}' clears read-only depth texture '{}'",
						pass.name,
						texture.name));
				}
				if (imported && !imported->depthStencilView)
				{
					return fail(std::format(
						"pass '{}' uses imported texture '{}' without a depth-stencil view",
						pass.name,
						texture.name));
				}
				hasDepthTarget = true;
				flags = Attachment | Read;
				if (access.depthAccess == RenderGraphDepthAccess::ReadWrite)
				{
					flags |= Write;
					if (access.loadOp == RenderGraphLoadOp::Clear)
					{
						flags = Attachment | Write;
					}
				}
				break;

			case AccessType::VertexShaderRead:
			case AccessType::PixelShaderRead:
			{
				std::array<bool, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>& slots =
					access.type == AccessType::VertexShaderRead
						? vertexSlots
						: pixelSlots;
				if (access.slot >= slots.size() || slots[access.slot])
				{
					return fail(std::format(
						"pass '{}' has an invalid or duplicate shader texture slot {}",
						pass.name,
						access.slot));
				}
				if ((flags & Attachment) != 0)
				{
					return fail(std::format(
						"pass '{}' binds texture '{}' as both an attachment and shader input",
						pass.name,
						texture.name));
				}
				if (imported && !imported->shaderResourceView)
				{
					return fail(std::format(
						"pass '{}' uses imported texture '{}' without a shader resource view",
						pass.name,
						texture.name));
				}
				slots[access.slot] = true;
				flags |= Read;
				break;
			}
			}
		}

		for (std::size_t textureIndex = 0; textureIndex < useFlags.size(); ++textureIndex)
		{
			const std::uint8_t flags = useFlags[textureIndex];
			if (flags != 0)
			{
				usesByTexture[textureIndex].push_back(TextureUse{
					passIndex,
					(flags & Read) != 0,
					(flags & Write) != 0
				});
			}
		}
	}

	std::vector<std::vector<std::size_t>> outgoing(passes_.size());
	std::vector<std::size_t> incomingCount(passes_.size(), 0);
	const auto addDependency = [&](std::size_t producer, std::size_t consumer)
	{
		std::vector<std::size_t>& edges = outgoing[producer];
		if (std::find(edges.begin(), edges.end(), consumer) == edges.end())
		{
			edges.push_back(consumer);
			++incomingCount[consumer];
		}
	};

	// Writers preserve declaration order; pure readers observe one version.
	for (std::size_t textureIndex = 0; textureIndex < usesByTexture.size(); ++textureIndex)
	{
		const std::vector<TextureUse>& uses = usesByTexture[textureIndex];
		const TextureRecord& texture = textures_[textureIndex];
		const bool isImported = texture.importedTexture.has_value();
		std::vector<std::size_t> writers;
		for (const TextureUse& use : uses)
		{
			if (use.writes)
			{
				writers.push_back(use.passIndex);
			}
		}

		if (writers.empty() && !uses.empty() && !isImported)
		{
			return fail(std::format(
				"transient texture '{}' is read but never written",
				texture.name));
		}

		for (std::size_t writerIndex = 1; writerIndex < writers.size(); ++writerIndex)
		{
			addDependency(writers[writerIndex - 1], writers[writerIndex]);
		}

		for (const TextureUse& use : uses)
		{
			const auto nextWriter = std::lower_bound(
				writers.begin(), writers.end(), use.passIndex);

			if (use.writes)
			{
				// Load on the first transient writer has no prior contents.
				if (use.reads && nextWriter == writers.begin() && !isImported)
				{
					return fail(std::format(
						"pass '{}' loads transient texture '{}' before any write",
						passes_[use.passIndex].name,
						texture.name));
				}
				continue;
			}

			if (nextWriter != writers.begin())
			{
				// A read between writers must finish before the next overwrite.
				addDependency(*(nextWriter - 1), use.passIndex);
				if (nextWriter != writers.end())
				{
					addDependency(use.passIndex, *nextWriter);
				}
			}
			else if (nextWriter != writers.end() && isImported)
			{
				// Reads before the first write see the imported initial content.
				addDependency(use.passIndex, *nextWriter);
			}
			else if (nextWriter != writers.end())
			{
				// A transient read declared early consumes its first writer.
				addDependency(*nextWriter, use.passIndex);
				if (writers.size() > 1)
				{
					addDependency(use.passIndex, writers[1]);
				}
			}
		}
	}

	// Stable Kahn sort uses registration order only to break independent ties.
	std::priority_queue<
		std::size_t,
		std::vector<std::size_t>,
		std::greater<std::size_t>> ready;
	for (std::size_t passIndex = 0; passIndex < passes_.size(); ++passIndex)
	{
		if (incomingCount[passIndex] == 0)
		{
			ready.push(passIndex);
		}
	}

	std::vector<std::size_t> order;
	order.reserve(passes_.size());
	while (!ready.empty())
	{
		const std::size_t passIndex = ready.top();
		ready.pop();
		order.push_back(passIndex);

		for (const std::size_t dependent : outgoing[passIndex])
		{
			if (--incomingCount[dependent] == 0)
			{
				ready.push(dependent);
			}
		}
	}

	if (order.size() != passes_.size())
	{
		for (std::size_t passIndex = 0; passIndex < passes_.size(); ++passIndex)
		{
			if (incomingCount[passIndex] != 0)
			{
				return fail(std::format(
					"cyclic dependencies leave pass '{}' blocked",
					passes_[passIndex].name));
			}
		}
	}

	// Publish the plan only after all validation and sorting succeeds.
	compiledPassOrder_ = std::move(order);
	isCompiled_ = true;
	return true;
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
	compiledPassOrder_.clear();
	isCompiled_ = false;

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
	compiledPassOrder_.clear();
	isCompiled_ = false;

	return RenderGraphTextureHandle(index, generation_, ownerId_);
}

bool RenderGraph::IsValid(RenderGraphTextureHandle handle) const noexcept
{
	return handle.ownerId_ == ownerId_
		&& handle.generation_ == generation_
		&& handle.index_ < textures_.size();
}
