#include "VertexBuffer.h"

#include <cstring>
#include <utility>

HRESULT VertexBuffer::Create(ID3D11Device* device,
	std::uint32_t byteWidth,
	std::uint32_t count,
	std::uint32_t stride,
	const void* data,
	BufferUsage usage)
{
	if (!device || !data) return E_POINTER;
	if (byteWidth == 0 || count == 0 || stride == 0) return E_INVALIDARG;

	// Buffer description block
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = byteWidth;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	
	// Determine vertex buffer usage
	if (usage == BufferUsage::BUFFER_IMMUTABLE)
	{
		desc.Usage = D3D11_USAGE_IMMUTABLE;
	}
	else if (usage == BufferUsage::BUFFER_DYNAMIC)
	{
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}
	else
	{
		return E_INVALIDARG;
	}

	D3D11_SUBRESOURCE_DATA sData = { data };
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;

	const HRESULT result = device->CreateBuffer(
		&desc,
		&sData,
		buffer.GetAddressOf());
	if (FAILED(result))
	{
		return result;
	}
	buffer->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(std::strlen("VertexBuffer")), "VertexBuffer");

	buffer_ = std::move(buffer);
	count_  = count;
	stride_ = stride;
	usage_  = usage;
	return S_OK;
}

void VertexBuffer::Bind(ID3D11DeviceContext* context) const noexcept
{
	if (!context || !buffer_) return;

	ID3D11Buffer* vertexBuffer = buffer_.Get();
	const UINT stride = stride_;
	const UINT offset = 0;
	context->IASetVertexBuffers(
		0,
		1,
		&vertexBuffer,
		&stride,
		&offset);
}

void VertexBuffer::Update(ID3D11DeviceContext* context, const void* data, std::uint32_t count)
{
	if (!context || !data || !buffer_ || count > count_ ||
		usage_ != BufferUsage::BUFFER_DYNAMIC) return;

	// For reading/writing onto live data, as compared to SUBRESOURCE_DATA
	D3D11_MAPPED_SUBRESOURCE mData = {};

	if (FAILED(context->Map(buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mData))) return;
	std::memcpy(mData.pData, data, count * static_cast<size_t>(stride_));
	context->Unmap(buffer_.Get(), NULL);
}
