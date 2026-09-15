#pragma once

#include "Engine/Render/D3D11/D3D11Device.h"
#include "Engine/Render/D3D11/PresentationSurface.h"
#include "Engine/Render/D3D11/Shader/D3D11ShaderManager.h"
#include "Engine/Render/Mesh/MeshManager.h"

struct RendererDesc
{
	HWND outputWindow = nullptr;
	Extent2D extent = {};
	bool vsync = true;

	std::filesystem::path shaderDirectory;
};

enum class RenderResult
{
	Ok,
	Occluded,
	DeviceLost,
	Failed
};

class Renderer
{
public:
	bool Initialize(RendererDesc desc);
	bool Resize(Extent2D extent);

	bool BeginFrame();
	void BindOutput() noexcept;
	// Render();
	RenderResult EndFrame();

	[[nodiscard]]
	bool IsOccluded() const { return surface_.PresentTest() == DXGI_STATUS_OCCLUDED; }

	[[nodiscard]]
	ID3D11Device* GetDevice() const { return device_.GetDevice(); }

	[[nodiscard]]
	ID3D11DeviceContext* GetDeviceContext() const { return device_.GetDeviceContext(); }

	[[nodiscard]]
	ShaderManager& GetShaderManager() noexcept
	{
		return shaderManager_;
	}

	[[nodiscard]]
	const ShaderManager& GetShaderManager() const noexcept
	{
		return shaderManager_;
	}

	[[nodiscard]]
	MeshManager& GetMeshManager() noexcept
	{
		return meshManager_;
	}

	[[nodiscard]]
	const MeshManager& GetMeshManager() const noexcept
	{
		return meshManager_;
	}

private:
	RendererDesc rendererDesc_;

	// D3D11 resources
	D3D11Device device_;
	D3D11PresentationSurface surface_;
	ShaderManager shaderManager_;
	MeshManager meshManager_;
};
