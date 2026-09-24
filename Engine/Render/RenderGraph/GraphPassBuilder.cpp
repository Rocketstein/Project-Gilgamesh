#include "GraphPassBuilder.h"

void GraphPassBuilder::SetColorTarget(
	std::uint32_t slot,
	RenderGraphTextureHandle texture,
	RenderGraphLoadOp loadOp,
	const Color4& clearColor)
{
	TextureAccess textureAccess;
	textureAccess.texture = texture;
	textureAccess.type = TextureAccess::Type::ColorTarget;
	textureAccess.slot = slot;
	textureAccess.loadOp = loadOp;
	textureAccess.clearColor = clearColor;
	textureAccesses_.push_back(textureAccess);
}

void GraphPassBuilder::SetDepthTarget(
	RenderGraphTextureHandle texture,
	RenderGraphDepthAccess access,
	RenderGraphLoadOp loadOp,
	float clearDepth)
{
	TextureAccess textureAccess;
	textureAccess.texture = texture;
	textureAccess.type = TextureAccess::Type::DepthTarget;
	textureAccess.loadOp = loadOp;
	textureAccess.depthAccess = access;
	textureAccess.clearDepth = clearDepth;
	textureAccesses_.push_back(textureAccess);
}

void GraphPassBuilder::ReadVertexTexture(
	RenderGraphTextureHandle texture,
	std::uint32_t slot)
{
	TextureAccess textureAccess;
	textureAccess.texture = texture;
	textureAccess.type = TextureAccess::Type::VertexShaderRead;
	textureAccess.slot = slot;
	textureAccesses_.push_back(textureAccess);
}

void GraphPassBuilder::ReadPixelTexture(
	RenderGraphTextureHandle texture,
	std::uint32_t slot)
{
	TextureAccess textureAccess;
	textureAccess.texture = texture;
	textureAccess.type = TextureAccess::Type::PixelShaderRead;
	textureAccess.slot = slot;
	textureAccesses_.push_back(textureAccess);
}
