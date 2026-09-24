#include "RenderGraph.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
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

	// Reports one compile failure without introducing a dedicated error type.
	bool LogCompileFailure(std::string_view message)
	{
		GILGAMESH_LOG(
			Renderer,
			Error,
			"RenderGraph compile failed: {}",
			message);
		return false;
	}

	// Reports one execution failure without introducing a dedicated error type.
	bool LogExecutionFailure(std::string_view message)
	{
		GILGAMESH_LOG(
			Renderer,
			Error,
			"RenderGraph execution failed: {}",
			message);
		return false;
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

	// Resolves a logical depth format to its typeless resource format.
	DXGI_FORMAT GetDepthResourceFormat(DXGI_FORMAT format) noexcept
	{
		switch (format)
		{
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_TYPELESS:
			return DXGI_FORMAT_R16_TYPELESS;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24G8_TYPELESS:
			return DXGI_FORMAT_R24G8_TYPELESS;
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_TYPELESS:
			return DXGI_FORMAT_R32_TYPELESS;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32G8X24_TYPELESS:
			return DXGI_FORMAT_R32G8X24_TYPELESS;
		default:
			return DXGI_FORMAT_UNKNOWN;
		}
	}

	// Resolves a logical depth format to a writable or read-only DSV format.
	DXGI_FORMAT GetDepthStencilViewFormat(DXGI_FORMAT format) noexcept
	{
		switch (format)
		{
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_TYPELESS:
			return DXGI_FORMAT_D16_UNORM;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24G8_TYPELESS:
			return DXGI_FORMAT_D24_UNORM_S8_UINT;
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_TYPELESS:
			return DXGI_FORMAT_D32_FLOAT;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32G8X24_TYPELESS:
			return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		default:
			return DXGI_FORMAT_UNKNOWN;
		}
	}

	// Resolves a logical depth format to its shader-readable view format.
	DXGI_FORMAT GetDepthShaderResourceViewFormat(DXGI_FORMAT format) noexcept
	{
		switch (format)
		{
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_TYPELESS:
			return DXGI_FORMAT_R16_UNORM;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24G8_TYPELESS:
			return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_TYPELESS:
			return DXGI_FORMAT_R32_FLOAT;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32G8X24_TYPELESS:
			return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
		default:
			return DXGI_FORMAT_UNKNOWN;
		}
	}

	// Reports whether the depth family also contains a stencil component.
	bool HasStencilComponent(DXGI_FORMAT format) noexcept
	{
		return format == DXGI_FORMAT_D24_UNORM_S8_UINT
			|| format == DXGI_FORMAT_R24G8_TYPELESS
			|| format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT
			|| format == DXGI_FORMAT_R32G8X24_TYPELESS;
	}

	// Confirms that a supplied native view refers to the imported texture.
	bool IsViewOfTexture(
		ID3D11View* view,
		ID3D11Texture2D* texture) noexcept
	{
		if (!view || !texture)
		{
			return false;
		}

		Microsoft::WRL::ComPtr<ID3D11Resource> viewResource;
		Microsoft::WRL::ComPtr<ID3D11Resource> textureResource;
		view->GetResource(viewResource.GetAddressOf());
		const HRESULT result = texture->QueryInterface(
			IID_PPV_ARGS(textureResource.GetAddressOf()));
		return SUCCEEDED(result)
			&& viewResource.Get() == textureResource.Get();
	}

} // Anonymous namespace

RenderGraph::RenderGraph() noexcept
	: ownerId_(AcquireRenderGraphOwnerId())
{
}

void RenderGraph::Reset() noexcept
{
	assert(!isAddingPass_ && !isExecuting_);
	if (isAddingPass_ || isExecuting_)
	{
		return;
	}

	// Destroy callbacks before resources so captured frame data is released now.
	passes_.clear();
	InvalidateCompiledState();
	textures_.clear();

	// A fresh generation prevents reused indices from reviving stale handles.
	++generation_;
	if (generation_ == 0)
	{
		ownerId_ = AcquireRenderGraphOwnerId();
		generation_ = 1;
	}
}

void RenderGraph::InvalidateCompiledState() noexcept
{
	compiledPassOrder_.clear();
	ReleaseResources();
	compiledDevice_.Reset();
	for (TextureRecord& texture : textures_)
	{
		texture.compiledViewRequirements = TextureViewRequirement::None;
	}
	isCompiled_ = false;
}

void RenderGraph::ReleaseResources() noexcept
{
	for (TextureRecord& texture : textures_)
	{
		texture.ownedSRV.Reset();
		texture.ownedReadOnlyDSV.Reset();
		texture.ownedWritableDSV.Reset();
		texture.ownedRTV.Reset();
		texture.ownedTexture.Reset();
	}
}

bool RenderGraph::ValidateResourceDeclarations() const
{
	std::unordered_map<ID3D11Texture2D*, std::size_t>
		firstImportByTexture;
	firstImportByTexture.reserve(textures_.size());

	for (std::size_t textureIndex = 0;
		textureIndex < textures_.size();
		++textureIndex)
	{
		const TextureRecord& texture = textures_[textureIndex];
		const RenderGraphTextureDesc& description = texture.description;

		// Reject descriptions that could not represent a 2D D3D11 texture.
		if (description.extent.width == 0
			|| description.extent.height == 0
			|| description.format == DXGI_FORMAT_UNKNOWN
			|| description.mipLevels != 1
			|| description.sampleCount == 0)
		{
			return LogCompileFailure(std::format(
				"texture '{}' has an invalid description",
				texture.name));
		}

		const auto& imported = texture.importedTexture;
		if (!imported || !imported->texture)
		{
			continue;
		}

		// One native texture must have one graph handle so hazards stay visible.
		const auto [firstImport, inserted] = firstImportByTexture.emplace(
			imported->texture,
			textureIndex);
		if (!inserted)
		{
			return LogCompileFailure(std::format(
				"imported textures '{}' and '{}' alias the same native texture",
				textures_[firstImport->second].name,
				texture.name));
		}
	}

	return true;
}

bool RenderGraph::ValidateBindingDeclarations(
	TextureUsesByTexture& usesByTexture)
{
	using AccessType = GraphPassBuilder::TextureAccess::Type;
	constexpr std::uint8_t Read = 1 << 0;
	constexpr std::uint8_t Write = 1 << 1;
	constexpr std::uint8_t Attachment = 1 << 2;

	usesByTexture.clear();
	usesByTexture.resize(textures_.size());

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
				return LogCompileFailure(std::format(
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
				return LogCompileFailure(std::format(
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
					return LogCompileFailure(std::format(
						"pass '{}' binds texture '{}' both as an attachment and another view",
						pass.name,
						texture.name));
				}

				if (firstAttachment
					&& (firstAttachment->extent != description.extent
						|| firstAttachment->sampleCount != description.sampleCount))
				{
					return LogCompileFailure(std::format(
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
					return LogCompileFailure(std::format(
						"pass '{}' has an invalid color load operation",
						pass.name));
				}
				if (access.slot >= colorSlots.size() || colorSlots[access.slot])
				{
					return LogCompileFailure(std::format(
						"pass '{}' has an invalid or duplicate color target slot {}",
						pass.name,
						access.slot));
				}
				if (IsDepthCompatibleFormat(description.format))
				{
					return LogCompileFailure(std::format(
						"pass '{}' uses depth texture '{}' as a color target",
						pass.name,
						texture.name));
				}
				if (imported && !imported->renderTargetView)
				{
					return LogCompileFailure(std::format(
						"pass '{}' uses imported texture '{}' without a render target view",
						pass.name,
						texture.name));
				}
				colorSlots[access.slot] = true;
				textures_[textureIndex].compiledViewRequirements |=
					TextureViewRequirement::RenderTarget;
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
					return LogCompileFailure(std::format(
						"pass '{}' has an invalid depth load operation",
						pass.name));
				}
				if (access.depthAccess != RenderGraphDepthAccess::ReadOnly
					&& access.depthAccess != RenderGraphDepthAccess::ReadWrite)
				{
					return LogCompileFailure(std::format(
						"pass '{}' has an invalid depth access mode",
						pass.name));
				}
				if (hasDepthTarget)
				{
					return LogCompileFailure(std::format(
						"pass '{}' declares more than one depth target",
						pass.name));
				}
				if (!IsDepthCompatibleFormat(description.format))
				{
					return LogCompileFailure(std::format(
						"pass '{}' uses non-depth texture '{}' as a depth target",
						pass.name,
						texture.name));
				}
				if (access.depthAccess == RenderGraphDepthAccess::ReadOnly
					&& access.loadOp == RenderGraphLoadOp::Clear)
				{
					return LogCompileFailure(std::format(
						"pass '{}' clears read-only depth texture '{}'",
						pass.name,
						texture.name));
				}
				if (access.loadOp == RenderGraphLoadOp::Clear
					&& (!std::isfinite(access.clearDepth)
						|| access.clearDepth < 0.0f
						|| access.clearDepth > 1.0f))
				{
					return LogCompileFailure(std::format(
						"pass '{}' clears depth texture '{}' with a value outside [0, 1]",
						pass.name,
						texture.name));
				}
				if (imported
					&& !(access.depthAccess == RenderGraphDepthAccess::ReadOnly
						? imported->readOnlyDepthStencilView
						: imported->writableDepthStencilView))
				{
					return LogCompileFailure(std::format(
						"pass '{}' uses imported texture '{}' without a depth-stencil view",
						pass.name,
						texture.name));
				}
				hasDepthTarget = true;
				if (access.depthAccess == RenderGraphDepthAccess::ReadOnly)
				{
					textures_[textureIndex].compiledViewRequirements |=
						TextureViewRequirement::ReadOnlyDepthStencil;
				}
				else
				{
					textures_[textureIndex].compiledViewRequirements |=
						TextureViewRequirement::WritableDepthStencil;
				}
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
					return LogCompileFailure(std::format(
						"pass '{}' has an invalid or duplicate shader texture slot {}",
						pass.name,
						access.slot));
				}
				if ((flags & Attachment) != 0)
				{
					return LogCompileFailure(std::format(
						"pass '{}' binds texture '{}' as both an attachment and shader input",
						pass.name,
						texture.name));
				}
				if (imported && !imported->shaderResourceView)
				{
					return LogCompileFailure(std::format(
						"pass '{}' uses imported texture '{}' without a shader resource view",
						pass.name,
						texture.name));
				}
				slots[access.slot] = true;
				textures_[textureIndex].compiledViewRequirements |=
					TextureViewRequirement::ShaderResource;
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

	return true;
}

bool RenderGraph::Compile(ID3D11Device* device)
{
	using std::vector;
	using std::size_t;

	if (isExecuting_)
	{
		return LogCompileFailure("Compile cannot run during graph execution");
	}

	if (isAddingPass_)
	{
		return LogCompileFailure("Compile cannot run inside pass setup");
	}

	// A failed recompilation must never leave a stale executable order visible.
	InvalidateCompiledState();

	if (!ValidateResourceDeclarations())
	{
		return false;
	}

	TextureUsesByTexture usesByTexture;
	if (!ValidateBindingDeclarations(usesByTexture))
	{
		InvalidateCompiledState();
		return false;
	}
	const auto invalidateAndLogFailure = [this](std::string_view message)
	{
		InvalidateCompiledState();
		return LogCompileFailure(message);
	};

	vector<vector<size_t>> outgoing(passes_.size());
	vector<size_t> incomingCount(passes_.size(), 0);
	const auto addDependency = [&](size_t producer, size_t consumer)
	{
		vector<size_t>& edges = outgoing[producer];
		if (std::find(edges.begin(), edges.end(), consumer) == edges.end())
		{
			edges.push_back(consumer);
			++incomingCount[consumer];
		}
	};

	// Writers preserve declaration order; pure readers observe one version.
	for (size_t textureIndex = 0; textureIndex < usesByTexture.size(); ++textureIndex)
	{
		const vector<TextureUse>& uses = usesByTexture[textureIndex];
		const TextureRecord& texture = textures_[textureIndex];
		const bool isImported = texture.importedTexture.has_value();
		vector<size_t> writers;
		for (const TextureUse& use : uses)
		{
			if (use.writes)
			{
				writers.push_back(use.passIndex);
			}
		}

		if (writers.empty() && !uses.empty() && !isImported)
		{
			return invalidateAndLogFailure(std::format(
				"transient texture '{}' is read but never written",
				texture.name));
		}

		for (size_t writerIndex = 1; writerIndex < writers.size(); ++writerIndex)
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
					return invalidateAndLogFailure(std::format(
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
		size_t,
		vector<size_t>,
		std::greater<size_t>> ready;
	for (size_t passIndex = 0; passIndex < passes_.size(); ++passIndex)
	{
		if (incomingCount[passIndex] == 0)
		{
			ready.push(passIndex);
		}
	}

	vector<size_t> order;
	order.reserve(passes_.size());
	while (!ready.empty())
	{
		const size_t passIndex = ready.top();
		ready.pop();
		order.push_back(passIndex);

		for (const size_t dependent : outgoing[passIndex])
		{
			if (--incomingCount[dependent] == 0)
			{
				ready.push(dependent);
			}
		}
	}

	if (order.size() != passes_.size())
	{
		for (size_t passIndex = 0; passIndex < passes_.size(); ++passIndex)
		{
			if (incomingCount[passIndex] != 0)
			{
				return invalidateAndLogFailure(std::format(
					"cyclic dependencies leave pass '{}' blocked",
					passes_[passIndex].name));
			}
		}
	}

	// Create GPU resources after compilation.
	if (!CreateResources(device))
	{
		InvalidateCompiledState();
		return false;
	}

	// Publish the plan only after all validation and sorting succeeds.
	compiledPassOrder_ = std::move(order);
	compiledDevice_ = device;
	isCompiled_ = true;

	return true;
}

bool RenderGraph::Execute(ID3D11DeviceContext* context)
{
	if (!context)
	{
		return LogExecutionFailure("a D3D11 device context is required");
	}
	if (!isCompiled_ || !compiledDevice_)
	{
		return LogExecutionFailure("the graph has not been compiled");
	}
	if (isAddingPass_)
	{
		return LogExecutionFailure("Execute cannot run inside pass setup");
	}
	if (isExecuting_)
	{
		return LogExecutionFailure("nested graph execution is not allowed");
	}

	// Native resources cannot be used through a context from another device.
	Microsoft::WRL::ComPtr<ID3D11Device> contextDevice;
	context->GetDevice(contextDevice.GetAddressOf());
	if (contextDevice.Get() != compiledDevice_.Get())
	{
		return LogExecutionFailure(
			"the device context does not belong to the compiled device");
	}

	isExecuting_ = true;
	const PassDeclaration* activePass = nullptr;

	try
	{
		for (const std::size_t passIndex : compiledPassOrder_)
		{
			activePass = &passes_[passIndex];
			BindPassAttachments(context, *activePass);
			ClearPassAttachments(context, *activePass);
			BindPassShaderResources(context, *activePass);
			std::invoke(activePass->execute, std::ref(*context));
			UnbindPassResources(context, *activePass);
			activePass = nullptr;
		}
	}
	catch (...)
	{
		// Leave graph-managed bindings clean even when user draw code fails.
		if (activePass)
		{
			UnbindPassResources(context, *activePass);
		}
		isExecuting_ = false;
		throw;
	}

	isExecuting_ = false;
	return true;
}

ID3D11RenderTargetView* RenderGraph::ResolveRenderTargetView(
	const TextureRecord& texture) const noexcept
{
	return texture.importedTexture
		? texture.importedTexture->renderTargetView
		: texture.ownedRTV.Get();
}

ID3D11DepthStencilView* RenderGraph::ResolveDepthStencilView(
	const TextureRecord& texture,
	RenderGraphDepthAccess access) const noexcept
{
	if (texture.importedTexture)
	{
		return access == RenderGraphDepthAccess::ReadOnly
			? texture.importedTexture->readOnlyDepthStencilView
			: texture.importedTexture->writableDepthStencilView;
	}

	return access == RenderGraphDepthAccess::ReadOnly
		? texture.ownedReadOnlyDSV.Get()
		: texture.ownedWritableDSV.Get();
}

ID3D11ShaderResourceView* RenderGraph::ResolveShaderResourceView(
	const TextureRecord& texture) const noexcept
{
	return texture.importedTexture
		? texture.importedTexture->shaderResourceView
		: texture.ownedSRV.Get();
}

void RenderGraph::BindPassAttachments(
	ID3D11DeviceContext* context,
	const PassDeclaration& pass) const noexcept
{
	using AccessType = GraphPassBuilder::TextureAccess::Type;

	std::array<
		ID3D11RenderTargetView*,
		D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> renderTargets{};
	ID3D11DepthStencilView* depthTarget = nullptr;
	const TextureRecord* firstAttachment = nullptr;
	UINT renderTargetCount = 0;

	// Resolve the complete output-merger binding before publishing it.
	for (const GraphPassBuilder::TextureAccess& access : pass.textureAccesses)
	{
		const TextureRecord& texture = textures_[access.texture.index_];
		switch (access.type)
		{
		case AccessType::ColorTarget:
			renderTargets[access.slot] = ResolveRenderTargetView(texture);
			renderTargetCount = std::max(
				renderTargetCount,
				static_cast<UINT>(access.slot + 1));
			if (!firstAttachment)
			{
				firstAttachment = &texture;
			}
			break;

		case AccessType::DepthTarget:
			depthTarget = ResolveDepthStencilView(
				texture,
				access.depthAccess);
			if (!firstAttachment)
			{
				firstAttachment = &texture;
			}
			break;

		default:
			break;
		}
	}

	context->OMSetRenderTargets(
		renderTargetCount,
		renderTargetCount > 0 ? renderTargets.data() : nullptr,
		depthTarget);

	// Full-attachment rendering is the default; callbacks may override it.
	if (firstAttachment)
	{
		D3D11_VIEWPORT viewport{};
		viewport.Width = static_cast<float>(
			firstAttachment->description.extent.width);
		viewport.Height = static_cast<float>(
			firstAttachment->description.extent.height);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		context->RSSetViewports(1, &viewport);
	}

}

void RenderGraph::ClearPassAttachments(
	ID3D11DeviceContext* context,
	const PassDeclaration& pass) const noexcept
{
	using AccessType = GraphPassBuilder::TextureAccess::Type;

	for (const GraphPassBuilder::TextureAccess& access : pass.textureAccesses)
	{
		if (access.loadOp != RenderGraphLoadOp::Clear)
		{
			continue;
		}

		const TextureRecord& texture = textures_[access.texture.index_];
		if (access.type == AccessType::ColorTarget)
		{
			context->ClearRenderTargetView(
				ResolveRenderTargetView(texture),
				access.clearColor.Data());
		}
		else if (access.type == AccessType::DepthTarget)
		{
			// Stencil load/store behavior is not modeled by the graph yet.
			context->ClearDepthStencilView(
				ResolveDepthStencilView(texture, access.depthAccess),
				D3D11_CLEAR_DEPTH,
				access.clearDepth,
				0);
		}
	}
}

void RenderGraph::BindPassShaderResources(
	ID3D11DeviceContext* context,
	const PassDeclaration& pass) const noexcept
{
	using AccessType = GraphPassBuilder::TextureAccess::Type;

	// Shader reads are stage- and slot-specific declarations.
	for (const GraphPassBuilder::TextureAccess& access : pass.textureAccesses)
	{
		if (access.type != AccessType::VertexShaderRead
			&& access.type != AccessType::PixelShaderRead)
		{
			continue;
		}

		ID3D11ShaderResourceView* shaderResource =
			ResolveShaderResourceView(textures_[access.texture.index_]);
		if (access.type == AccessType::VertexShaderRead)
		{
			context->VSSetShaderResources(
				access.slot,
				1,
				&shaderResource);
		}
		else
		{
			context->PSSetShaderResources(
				access.slot,
				1,
				&shaderResource);
		}
	}
}

void RenderGraph::UnbindPassResources(
	ID3D11DeviceContext* context,
	const PassDeclaration& pass) const noexcept
{
	using AccessType = GraphPassBuilder::TextureAccess::Type;
	ID3D11ShaderResourceView* nullView = nullptr;

	// End graph-declared reads so later passes may write the same resources.
	for (const GraphPassBuilder::TextureAccess& access : pass.textureAccesses)
	{
		if (access.type == AccessType::VertexShaderRead)
		{
			context->VSSetShaderResources(access.slot, 1, &nullView);
		}
		else if (access.type == AccessType::PixelShaderRead)
		{
			context->PSSetShaderResources(access.slot, 1, &nullView);
		}
	}

	// End attachment writes so later passes may sample the same resources.
	context->OMSetRenderTargets(0, nullptr, nullptr);
}

bool RenderGraph::CreateResources(ID3D11Device* device)
{
	if (!device)
	{
		return LogCompileFailure(
			"GPU resource creation requires a D3D11 device");
	}

	// Rebuilding must not retain views whose requirements have changed.
	ReleaseResources();
	const auto failCreation = [this](
		const TextureRecord& texture,
		std::string_view objectName,
		HRESULT result)
	{
		ReleaseResources();
		return LogCompileFailure(std::format(
			"failed to create {} for texture '{}' (HRESULT=0x{:08X})",
			objectName,
			texture.name,
			static_cast<std::uint32_t>(result)));
	};
	const auto failTexture = [this](
		const TextureRecord& texture,
		std::string_view reason)
	{
		ReleaseResources();
		return LogCompileFailure(std::format(
			"texture '{}' {}",
			texture.name,
			reason));
	};

	for (TextureRecord& texture : textures_)
	{
		if (texture.compiledViewRequirements == TextureViewRequirement::None)
		{
			continue;
		}

		const auto hasRequirement = [&texture](TextureViewRequirement requirement)
		{
			return (texture.compiledViewRequirements & requirement) != 0;
		};
		const bool requiresRenderTarget =
			hasRequirement(TextureViewRequirement::RenderTarget);
		const bool requiresWritableDepth =
			hasRequirement(TextureViewRequirement::WritableDepthStencil);
		const bool requiresReadOnlyDepth =
			hasRequirement(TextureViewRequirement::ReadOnlyDepthStencil);
		const bool requiresDepth =
			requiresWritableDepth || requiresReadOnlyDepth;
		const bool requiresShaderResource =
			hasRequirement(TextureViewRequirement::ShaderResource);

		const RenderGraphTextureDesc& description = texture.description;
		const DXGI_FORMAT depthResourceFormat =
			GetDepthResourceFormat(description.format);
		const bool isDepthFormat =
			depthResourceFormat != DXGI_FORMAT_UNKNOWN;
		if (requiresDepth && !isDepthFormat)
		{
			return failTexture(
				texture,
				"does not have a depth-compatible format");
		}
		if (requiresRenderTarget && isDepthFormat)
		{
			return failTexture(
				texture,
				"cannot use a depth format as a render target");
		}

		UINT bindFlags = 0;
		if (requiresRenderTarget)
		{
			bindFlags |= D3D11_BIND_RENDER_TARGET;
		}
		if (requiresDepth)
		{
			bindFlags |= D3D11_BIND_DEPTH_STENCIL;
		}
		if (requiresShaderResource)
		{
			bindFlags |= D3D11_BIND_SHADER_RESOURCE;
		}

		// Imported resources are not created here, but their declarations must be truthful.
		if (texture.importedTexture)
		{
			const ImportedRenderGraphTexture& imported =
				*texture.importedTexture;
			if (!imported.texture)
			{
				return failTexture(texture, "does not provide a native texture");
			}

			Microsoft::WRL::ComPtr<ID3D11Device> importedDevice;
			imported.texture->GetDevice(importedDevice.GetAddressOf());
			if (importedDevice.Get() != device)
			{
				return failTexture(
					texture,
					"belongs to a different D3D11 device");
			}

			D3D11_TEXTURE2D_DESC importedDescription{};
			imported.texture->GetDesc(&importedDescription);
			const bool formatMatches = isDepthFormat
				? GetDepthResourceFormat(importedDescription.Format)
					== depthResourceFormat
				: importedDescription.Format == description.format;
			if (importedDescription.Width != description.extent.width
				|| importedDescription.Height != description.extent.height
				|| importedDescription.MipLevels != description.mipLevels
				|| importedDescription.ArraySize != 1
				|| importedDescription.SampleDesc.Count != description.sampleCount
				|| importedDescription.SampleDesc.Quality != 0
				|| !formatMatches)
			{
				return failTexture(
					texture,
					"does not match its declared description");
			}
			if ((importedDescription.BindFlags & bindFlags) != bindFlags)
			{
				return failTexture(
					texture,
					"does not support all declared bindings");
			}

			if (requiresRenderTarget)
			{
				D3D11_RENDER_TARGET_VIEW_DESC viewDescription{};
				if (!IsViewOfTexture(imported.renderTargetView, imported.texture))
				{
					return failTexture(
						texture,
						"has a render-target view for another resource");
				}
				imported.renderTargetView->GetDesc(&viewDescription);
				if (viewDescription.Format != description.format)
				{
					return failTexture(
						texture,
						"has an incompatible render-target view format");
				}
			}

			const DXGI_FORMAT depthViewFormat =
				GetDepthStencilViewFormat(description.format);
			if (requiresWritableDepth)
			{
				D3D11_DEPTH_STENCIL_VIEW_DESC viewDescription{};
				if (!IsViewOfTexture(
					imported.writableDepthStencilView,
					imported.texture))
				{
					return failTexture(
						texture,
						"has a writable depth view for another resource");
				}
				imported.writableDepthStencilView->GetDesc(&viewDescription);
				if (viewDescription.Format != depthViewFormat
					|| viewDescription.Flags != 0)
				{
					return failTexture(
						texture,
						"does not provide a compatible writable depth view");
				}
			}

			if (requiresReadOnlyDepth)
			{
				D3D11_DEPTH_STENCIL_VIEW_DESC viewDescription{};
				if (!IsViewOfTexture(
					imported.readOnlyDepthStencilView,
					imported.texture))
				{
					return failTexture(
						texture,
						"has a read-only depth view for another resource");
				}
				imported.readOnlyDepthStencilView->GetDesc(&viewDescription);
				const UINT requiredFlags = HasStencilComponent(description.format)
					? D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL
					: D3D11_DSV_READ_ONLY_DEPTH;
				if (viewDescription.Format != depthViewFormat
					|| (viewDescription.Flags & requiredFlags) != requiredFlags)
				{
					return failTexture(
						texture,
						"does not provide a compatible read-only depth view");
				}
			}

			if (requiresShaderResource)
			{
				D3D11_SHADER_RESOURCE_VIEW_DESC viewDescription{};
				if (!IsViewOfTexture(imported.shaderResourceView, imported.texture))
				{
					return failTexture(
						texture,
						"has a shader-resource view for another resource");
				}
				imported.shaderResourceView->GetDesc(&viewDescription);
				const DXGI_FORMAT shaderViewFormat = isDepthFormat
					? GetDepthShaderResourceViewFormat(description.format)
					: description.format;
				if (viewDescription.Format != shaderViewFormat)
				{
					return failTexture(
						texture,
						"has an incompatible shader-resource view format");
				}
			}

			continue;
		}

		D3D11_TEXTURE2D_DESC nativeDescription{};
		nativeDescription.Width = description.extent.width;
		nativeDescription.Height = description.extent.height;
		nativeDescription.MipLevels = description.mipLevels;
		nativeDescription.ArraySize = 1;
		nativeDescription.Format = isDepthFormat
			? depthResourceFormat
			: description.format;
		nativeDescription.SampleDesc.Count = description.sampleCount;
		nativeDescription.SampleDesc.Quality = 0;
		nativeDescription.Usage = D3D11_USAGE_DEFAULT;
		nativeDescription.BindFlags = bindFlags;
		nativeDescription.CPUAccessFlags = 0;
		nativeDescription.MiscFlags = 0;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> nativeTexture;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> writableDepthView;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> readOnlyDepthView;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;

		HRESULT result = device->CreateTexture2D(
			&nativeDescription,
			nullptr,
			nativeTexture.GetAddressOf());
		if (FAILED(result))
		{
			return failCreation(texture, "texture", result);
		}

		if (requiresRenderTarget)
		{
			D3D11_RENDER_TARGET_VIEW_DESC viewDescription{};
			viewDescription.Format = description.format;
			if (description.sampleCount > 1)
			{
				viewDescription.ViewDimension =
					D3D11_RTV_DIMENSION_TEXTURE2DMS;
			}
			else
			{
				viewDescription.ViewDimension =
					D3D11_RTV_DIMENSION_TEXTURE2D;
				viewDescription.Texture2D.MipSlice = 0;
			}

			result = device->CreateRenderTargetView(
				nativeTexture.Get(),
				&viewDescription,
				renderTargetView.GetAddressOf());
			if (FAILED(result))
			{
				return failCreation(texture, "render-target view", result);
			}
		}

		if (requiresDepth)
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC viewDescription{};
			viewDescription.Format =
				GetDepthStencilViewFormat(description.format);
			if (description.sampleCount > 1)
			{
				viewDescription.ViewDimension =
					D3D11_DSV_DIMENSION_TEXTURE2DMS;
			}
			else
			{
				viewDescription.ViewDimension =
					D3D11_DSV_DIMENSION_TEXTURE2D;
				viewDescription.Texture2D.MipSlice = 0;
			}

			if (requiresWritableDepth)
			{
				result = device->CreateDepthStencilView(
					nativeTexture.Get(),
					&viewDescription,
					writableDepthView.GetAddressOf());
				if (FAILED(result))
				{
					return failCreation(
						texture,
						"writable depth-stencil view",
						result);
				}
			}

			if (requiresReadOnlyDepth)
			{
				viewDescription.Flags = D3D11_DSV_READ_ONLY_DEPTH;
				if (HasStencilComponent(description.format))
				{
					viewDescription.Flags |= D3D11_DSV_READ_ONLY_STENCIL;
				}
				result = device->CreateDepthStencilView(
					nativeTexture.Get(),
					&viewDescription,
					readOnlyDepthView.GetAddressOf());
				if (FAILED(result))
				{
					return failCreation(
						texture,
						"read-only depth-stencil view",
						result);
				}
			}
		}

		if (requiresShaderResource)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC viewDescription{};
			viewDescription.Format = isDepthFormat
				? GetDepthShaderResourceViewFormat(description.format)
				: description.format;
			if (viewDescription.Format == DXGI_FORMAT_UNKNOWN)
			{
				return failTexture(
					texture,
					"does not have a shader-readable format");
			}

			if (description.sampleCount > 1)
			{
				viewDescription.ViewDimension =
					D3D11_SRV_DIMENSION_TEXTURE2DMS;
			}
			else
			{
				viewDescription.ViewDimension =
					D3D11_SRV_DIMENSION_TEXTURE2D;
				viewDescription.Texture2D.MostDetailedMip = 0;
				viewDescription.Texture2D.MipLevels =
					description.mipLevels;
			}

			result = device->CreateShaderResourceView(
				nativeTexture.Get(),
				&viewDescription,
				shaderResourceView.GetAddressOf());
			if (FAILED(result))
			{
				return failCreation(texture, "shader-resource view", result);
			}
		}

		// Publish a texture only after all of its required views exist.
		texture.ownedTexture = std::move(nativeTexture);
		texture.ownedRTV = std::move(renderTargetView);
		texture.ownedWritableDSV = std::move(writableDepthView);
		texture.ownedReadOnlyDSV = std::move(readOnlyDepthView);
		texture.ownedSRV = std::move(shaderResourceView);
	}

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
	if (isExecuting_)
	{
		throw std::logic_error(
			"RenderGraph resources cannot be created during graph execution");
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
	InvalidateCompiledState();

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
	if (isExecuting_)
	{
		throw std::logic_error(
			"RenderGraph resources cannot be imported during graph execution");
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
	InvalidateCompiledState();

	return RenderGraphTextureHandle(index, generation_, ownerId_);
}

bool RenderGraph::IsValid(RenderGraphTextureHandle handle) const noexcept
{
	return handle.ownerId_ == ownerId_
		&& handle.generation_ == generation_
		&& handle.index_ < textures_.size();
}
