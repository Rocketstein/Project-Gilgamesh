#include "ConstantBuffer.h"

void ConstantBuffer::Create(ID3D11Device* device, UINT16 byteWidth)
{
	if (!device || byteWidth == 0) return;

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = byteWidth;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	device->CreateBuffer(&desc, nullptr, &buffer_);
	buffer_->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ConstantBuffer")), "ConstantBuffer");
}

void ConstantBuffer::Update(ID3D11DeviceContext* context, const void* data, UINT16 byteWidth)
{
	if (!context || !data) return;

	D3D11_MAPPED_SUBRESOURCE mData = {};
	context->Map(buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mData);
	memcpy(mData.pData, data, byteWidth);
	context->Unmap(buffer_.Get(), NULL);
}