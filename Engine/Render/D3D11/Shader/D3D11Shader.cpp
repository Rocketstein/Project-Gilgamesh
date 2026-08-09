#include "D3D11Shader.h"

HRESULT VertexShader::Initialize(ID3D11Device* device, std::vector<std::byte> bytecode)
{
	if (device == nullptr || bytecode.empty()) return E_INVALIDARG;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;

	const HRESULT result = device->CreateVertexShader(
		bytecode.data(),
		bytecode.size(),
		nullptr,
		shader.GetAddressOf());

	if (FAILED(result)) return result;

	shader_ = std::move(shader);
	bytecode_ = std::move(bytecode);

	return S_OK;
}

HRESULT PixelShader::Initialize(ID3D11Device* device, std::vector<std::byte> bytecode)
{
	if (device == nullptr || bytecode.empty()) return E_INVALIDARG;

	Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;

	const HRESULT result = device->CreatePixelShader(
		bytecode.data(),
		bytecode.size(),
		nullptr,
		shader.GetAddressOf());

	if (FAILED(result)) return result;

	shader_ = std::move(shader);
	bytecode_ = std::move(bytecode);

	return S_OK;
}