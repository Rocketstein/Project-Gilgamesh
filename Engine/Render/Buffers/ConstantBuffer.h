#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_5.h>
#include <wrl/client.h>

class ConstantBuffer final
{
public:
    ConstantBuffer() = default;
    ConstantBuffer(const ConstantBuffer&) = delete;
    ConstantBuffer& operator=(const ConstantBuffer&) = delete;

    // Create a CPU-writable D3D11 constant buffer.
    void Create(ID3D11Device* device, UINT16 byteWidth);

    // Replace the current constant-buffer contents.
    void Update(ID3D11DeviceContext* context, const void* data, UINT16 byteWidth);

    [[nodiscard]]
    ID3D11Buffer* GetBuffer() const noexcept { return buffer_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer_;
};
