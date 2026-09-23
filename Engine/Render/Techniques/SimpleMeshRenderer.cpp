#include "SimpleMeshRenderer.h"

#include <cstddef>
#include <type_traits>
#include <utility>

#include <DirectXMath.h>

#include "Engine/Render/Mesh/StaticMesh.h"
#include "Engine/Render/Mesh/Vertex.h"

namespace
{
	struct alignas(16) ObjectConstants
	{
		DirectX::XMFLOAT4X4 modelViewProjection;
	};

	static_assert(std::is_standard_layout_v<NormalVertex>,
		"NormalVertex must support explicit input-layout offsets");
	static_assert(sizeof(ObjectConstants) % 16 == 0,
		"D3D11 constant buffers must be a multiple of 16 bytes");
}

bool SimpleMeshRenderer::Initialize(
	ID3D11Device* device,
	ShaderManager& shaders)
{
	if (!device || !shaders.IsInitialized()) return false;

	const auto vertexShader = shaders.LoadVertex(L"Primitive");
	if (!vertexShader) return false;

	const auto pixelShader = shaders.LoadPixel(L"Primitive");
	if (!pixelShader) return false;

	const D3D11_INPUT_ELEMENT_DESC inputElements[] =
	{
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			static_cast<UINT>(offsetof(NormalVertex, position)),
			D3D11_INPUT_PER_VERTEX_DATA,
			0
		},
		{
			"COLOR",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			static_cast<UINT>(offsetof(NormalVertex, color)),
			D3D11_INPUT_PER_VERTEX_DATA,
			0
		}
	};

	const DrawSetupDesc drawSetupDesc{
		.vertexShader = vertexShader.resource,
		.pixelShader = pixelShader.resource,
		.inputElements = inputElements,
		.drawState = {
			.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
			.depthTestEnabled = true,
			.depthWriteEnabled = true,
			.depthComparison = D3D11_COMPARISON_LESS
		}
	};

	DrawSetup newDrawSetup;
	const HRESULT drawSetupResult = newDrawSetup.Initialize(
		device,
		shaders,
		drawSetupDesc);
	if (FAILED(drawSetupResult)) return false;

	D3D11_BUFFER_DESC constantBufferDesc{};
	constantBufferDesc.ByteWidth = sizeof(ObjectConstants);
	constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	Microsoft::WRL::ComPtr<ID3D11Buffer> newObjectConstantBuffer;
	const HRESULT bufferResult = device->CreateBuffer(
		&constantBufferDesc,
		nullptr,
		newObjectConstantBuffer.GetAddressOf());
	if (FAILED(bufferResult)) return false;

	drawSetup_ = std::move(newDrawSetup);
	objectConstantBuffer_ = std::move(newObjectConstantBuffer);
	return true;
}

void SimpleMeshRenderer::Draw(
	ID3D11DeviceContext* context,
	const RenderView& renderView,
	std::span<const StaticMeshDrawItem> drawItems) const noexcept
{
	if (!context || !IsInitialized() || drawItems.empty()) return;

	drawSetup_.Apply(context);

	ID3D11Buffer* objectConstantBuffer =
		objectConstantBuffer_.Get();
	context->VSSetConstantBuffers(
		0,
		1,
		&objectConstantBuffer);

	for (const StaticMeshDrawItem& drawItem : drawItems)
	{
		if (!drawItem.mesh) continue;

		ObjectConstants constants{};
		DirectX::XMStoreFloat4x4(
			&constants.modelViewProjection,
			ToDirectX(Transpose(
				drawItem.localToWorld *
				renderView.view *
				renderView.projection)));

		context->UpdateSubresource(
			objectConstantBuffer_.Get(),
			0,
			nullptr,
			&constants,
			0,
			0);

		drawItem.mesh->Bind(context);
		context->DrawIndexed(
			drawItem.mesh->GetIndexCount(),
			0,
			0);
	}
}

bool SimpleMeshRenderer::IsInitialized() const noexcept
{
	return drawSetup_.IsInitialized() && objectConstantBuffer_ != nullptr;
}
