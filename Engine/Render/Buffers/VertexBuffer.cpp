#include "VertexBuffer.h"

#include <cstring>
#include <utility>

// Create and initialize a vertex buffer without publishing partial state.
HRESULT VertexBuffer::Create(ID3D11Device* device,
	std::uint32_t byteWidth,
	std::uint32_t count,
	std::uint32_t stride,
	const void* data,
	BufferUsage usage)
{
	if (!device || (!data && usage == BufferUsage::BUFFER_IMMUTABLE)) return E_POINTER;
	if (byteWidth == 0 || count == 0 || stride == 0 ||
		static_cast<std::uint64_t>(count) * stride > byteWidth)
	{
		return E_INVALIDARG;
	}

	// Allocate enough bytes for the complete declared vertex capacity.
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = byteWidth;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	
	// Only dynamic buffers support CPU replacement after creation.
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
		data ? &sData : nullptr,
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

// Discard prior contents and publish a bounded dynamic upload only after mapping succeeds.
bool VertexBuffer::Update(ID3D11DeviceContext* context, const void* data, std::uint32_t count)
{
	if (!context || !data || !buffer_ || count == 0 || count > count_ ||
		usage_ != BufferUsage::BUFFER_DYNAMIC) return false;

	// The creation-time capacity check guarantees this copy fits the GPU allocation.
	D3D11_MAPPED_SUBRESOURCE mData = {};

	if (FAILED(context->Map(buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mData))) return false;
	std::memcpy(mData.pData, data, count * static_cast<size_t>(stride_));
	context->Unmap(buffer_.Get(), 0);
	return true;
}
