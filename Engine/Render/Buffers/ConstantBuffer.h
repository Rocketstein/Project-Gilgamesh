#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_5.h>
#include <wrl/client.h>

template <class T>
class ConstantBuffer final
{
    static_assert(
        std::is_trivially_copyable_v<T>,
        "Constant-buffer data must be trivially copyable");

    static_assert(
        sizeof(T) % 16 == 0,
        "D3D11 constant-buffer size must be a multiple of 16 bytes");

public:
    ConstantBuffer() = default;
    ConstantBuffer(const ConstantBuffer&) = delete;
    ConstantBuffer& operator=(const ConstantBuffer&) = delete;
    ConstantBuffer(ConstantBuffer&&) noexcept = default;
    ConstantBuffer& operator=(ConstantBuffer&&) noexcept = default;

    // Create a CPU-writable D3D11 constant buffer.
    bool Create(ID3D11Device* device)
    {
        UINT byteWidth = static_cast<UINT>(sizeof(T));
        if (!device || byteWidth == 0) return false;

        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = byteWidth;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        device->CreateBuffer(&desc, nullptr, &buffer_);
        buffer_->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ConstantBuffer")), "ConstantBuffer");
        initialized_ = true;

        return true;
    }

    // Replace the current constant-buffer contents.
    void Update(ID3D11DeviceContext* context, const T& value) const
    {
        if (!context || !buffer_) return;

        D3D11_MAPPED_SUBRESOURCE mData = {};
        context->Map(buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mData);
        memcpy(mData.pData, &value, sizeof(T));
        context->Unmap(buffer_.Get(), NULL);
    }

    [[nodiscard]]
    ID3D11Buffer* GetBuffer() const noexcept { return buffer_.Get(); }

    [[nodiscard]]
    bool IsInitialized() const noexcept { return initialized_; }

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer_;
    bool initialized_ = false;
};
