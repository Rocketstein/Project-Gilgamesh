#include "SimpleMeshRenderer.h"

#include <cstddef>
#include <utility>

#include "Engine/Render/Mesh/StaticMesh.h"
#include "Engine/Render/Mesh/Vertex.h"

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

	ConstantBuffer<ObjectConstants> newObjectBuffer;
	if (!(newObjectBuffer.Create(device))) return false;

	drawSetup_ = std::move(newDrawSetup);
	objectCB_ = std::move(newObjectBuffer);

	return true;
}

void SimpleMeshRenderer::Draw(
	ID3D11DeviceContext* context,
	const RenderView& renderView,
	std::span<const StaticMeshDrawItem> drawItems) const noexcept
{
	if (!context || !IsInitialized() || drawItems.empty()) return;

	drawSetup_.Apply(context);

	ID3D11Buffer* objectBuffer = objectCB_.GetBuffer();
	context->VSSetConstantBuffers(2, 1, &objectBuffer);

	for (const StaticMeshDrawItem& drawItem : drawItems)
	{
		if (!drawItem.mesh) continue;

		// Keep the primitive shader's current MVP contract until frame constants
		// are prepared and bound separately at the view level.
		ObjectConstants constants{};
		constants.model = Transpose(
			drawItem.localToWorld *
			renderView.view *
			renderView.projection);

		if (!objectCB_.Update(context, constants)) return;

		drawItem.mesh->Bind(context);
		context->DrawIndexed(
			drawItem.mesh->GetIndexCount(),
			0,
			0);
	}
}

bool SimpleMeshRenderer::IsInitialized() const noexcept
{
	return drawSetup_.IsInitialized() && objectCB_.IsInitialized();
}
