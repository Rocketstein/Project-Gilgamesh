#include "Shader.h"

#include <utility>

HRESULT VertexShader::Initialize(ID3D11Device* device, std::vector<std::byte> bytecode)
{
	if (device == nullptr || bytecode.empty()) return E_INVALIDARG;

	ShaderReflectionData reflection;
	const HRESULT reflectionResult = ExtractShaderReflection(
		bytecode,
		reflection);
	if (FAILED(reflectionResult)) return reflectionResult;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;

	const HRESULT result = device->CreateVertexShader(
		bytecode.data(),
		bytecode.size(),
		nullptr,
		shader.GetAddressOf());

	if (FAILED(result)) return result;

	// Publish the shader, metadata, and bytecode only after every step succeeds.
	shader_ = std::move(shader);
	reflection_ = std::move(reflection);
	bytecode_ = std::move(bytecode);

	return S_OK;
}

HRESULT PixelShader::Initialize(ID3D11Device* device, std::vector<std::byte> bytecode)
{
	if (device == nullptr || bytecode.empty()) return E_INVALIDARG;

	ShaderReflectionData reflection;
	const HRESULT reflectionResult = ExtractShaderReflection(
		bytecode,
		reflection);
	if (FAILED(reflectionResult)) return reflectionResult;

	Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;

	const HRESULT result = device->CreatePixelShader(
		bytecode.data(),
		bytecode.size(),
		nullptr,
		shader.GetAddressOf());

	if (FAILED(result)) return result;

	// The pixel shader needs only its native object and reflected metadata.
	shader_ = std::move(shader);
	reflection_ = std::move(reflection);

	return S_OK;
}
