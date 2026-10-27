#pragma once
#include <cstdint>
#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Render/Buffers/BufferUsage.h"

// Owns immutable mesh data or a fixed-capacity dynamic vertex upload buffer.
class VertexBuffer final
{
public:
	VertexBuffer() = default;
	VertexBuffer(const VertexBuffer&) = delete;
	VertexBuffer& operator=(const VertexBuffer&) = delete;
	VertexBuffer(VertexBuffer&&) noexcept = default;
	VertexBuffer& operator=(VertexBuffer&&) noexcept = default;

	// Dynamic buffers may omit initial data; immutable buffers require it.
	// The declared count and stride must fit within the allocated byte width.
	[[nodiscard]]
	HRESULT Create(ID3D11Device* device,
		std::uint32_t byteWidth,
		std::uint32_t count,
		std::uint32_t stride,
		const void* data,
		BufferUsage usage = BUFFER_IMMUTABLE);

	void Bind(ID3D11DeviceContext* context) const noexcept;

	[[nodiscard]]
	ID3D11Buffer* GetBuffer() const noexcept { return buffer_.Get(); }

	// Returns the creation capacity, which is unchanged by partial dynamic uploads.
	[[nodiscard]]
	std::uint32_t GetCount() const noexcept { return count_; }

	[[nodiscard]]
	std::uint32_t GetStride() const noexcept { return stride_; }

	// Replace a nonempty prefix of a dynamic buffer; reject invalid inputs or Map failure.
	[[nodiscard]] bool Update(
		ID3D11DeviceContext* context, const void* data, std::uint32_t count);

private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer_;
	std::uint32_t count_ = 0; // Vertex capacity established by the last successful creation.
	std::uint32_t stride_ = 0;
	BufferUsage usage_ = BUFFER_IMMUTABLE;
};
