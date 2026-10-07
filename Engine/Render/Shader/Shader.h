#pragma once
#include <cstddef>
#include <d3d11.h>
#include <span>
#include <vector>
#include <wrl/client.h>

#include "ShaderReflection.h"

class VertexShader final
{
public:
	VertexShader() = default;
	~VertexShader() = default;

	VertexShader(const VertexShader&) = delete;
	VertexShader& operator=(const VertexShader&) = delete;

	VertexShader(VertexShader&&) noexcept = default;
	VertexShader& operator=(VertexShader&&) = default;


	HRESULT Initialize(ID3D11Device* device, std::vector<std::byte> bytecode);

	[[nodiscard]]
	ID3D11VertexShader* GetNativeHandle() const { return shader_.Get(); }

	[[nodiscard]]
	std::span<const std::byte> GetBytecode() const { return bytecode_; }

	[[nodiscard]]
	const ShaderReflectionData& GetReflection() const noexcept { return reflection_; }

private:
	Microsoft::WRL::ComPtr<ID3D11VertexShader> shader_;
	ShaderReflectionData reflection_;
	std::vector<std::byte> bytecode_;
};

class PixelShader final
{
public:
	PixelShader() = default;
	~PixelShader() = default;

	PixelShader(const PixelShader&) = delete;
	PixelShader& operator=(const PixelShader&) = delete;

	PixelShader(PixelShader&&) noexcept = default;
	PixelShader& operator=(PixelShader&&) noexcept = default;

	HRESULT Initialize(ID3D11Device* device, std::vector<std::byte> bytecode);

	[[nodiscard]]
	ID3D11PixelShader* GetNativeHandle() const { return shader_.Get(); }

	[[nodiscard]]
	const ShaderReflectionData& GetReflection() const noexcept { return reflection_; }

private:
	Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_;
	ShaderReflectionData reflection_;
};
