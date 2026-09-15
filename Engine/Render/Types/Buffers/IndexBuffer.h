#pragma once
#include <cstdint>
#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Render/Types/Buffers/BufferUsage.h"

class IndexBuffer final
{
public:
	IndexBuffer() = default;
	IndexBuffer(const IndexBuffer&) = delete;
	IndexBuffer& operator=(const IndexBuffer&) = delete;
	IndexBuffer(IndexBuffer&&) noexcept = default;
	IndexBuffer& operator=(IndexBuffer&&) noexcept = default;

	[[nodiscard]]
	HRESULT Create(ID3D11Device* device,
		std::uint32_t byteWidth,
		std::uint32_t count,
		const void* data,
		BufferUsage usage = BUFFER_IMMUTABLE);

	void Bind(ID3D11DeviceContext* context) const noexcept;

	[[nodiscard]]
	ID3D11Buffer* GetBuffer() const noexcept { return buffer_.Get(); }

	[[nodiscard]]
	std::uint32_t GetCount() const noexcept { return count_; }

	void Update(ID3D11DeviceContext* context, const void* data, std::uint32_t count);

private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer_;
	std::uint32_t count_ = 0;
	BufferUsage usage_ = BUFFER_IMMUTABLE;
};
