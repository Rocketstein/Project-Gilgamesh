#pragma once

#include <span>

#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Render/DrawSetup/DrawSetup.h"
#include "Engine/Render/Renderer/RenderView.h"
#include "Engine/Render/Renderer/StaticMeshDrawItem.h"

#include "Engine/Render/Buffers/ConstantBuffer.h"
#include "Engine/Render/Shader/ShaderConstants.h"

// Draws frame-local static-mesh items through the vertex-color draw setup.
class SimpleMeshRenderer final
{
public:
	SimpleMeshRenderer() = default;
	~SimpleMeshRenderer() = default;

	SimpleMeshRenderer(const SimpleMeshRenderer&) = delete;
	SimpleMeshRenderer& operator=(const SimpleMeshRenderer&) = delete;
	SimpleMeshRenderer(SimpleMeshRenderer&&) noexcept = default;
	SimpleMeshRenderer& operator=(SimpleMeshRenderer&&) noexcept = default;

	[[nodiscard]]
	bool Initialize(
		ID3D11Device* device,
		ShaderManager& shaders);

	void Draw(
		ID3D11DeviceContext* context,
		const RenderView& renderView,
		std::span<const StaticMeshDrawItem> drawItems) const noexcept;

	[[nodiscard]]
	bool IsInitialized() const noexcept;

private:
	DrawSetup drawSetup_;
	ConstantBuffer<ObjectConstants> objectCB_;
};
