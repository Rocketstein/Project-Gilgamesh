#pragma once
#include <d3d11.h>
#include <span>
#include <vector> 
#include <wrl/client.h>

enum class ShaderStage : std::uint8_t
{
	Vertex,
	Pixel,
	Geometry,
	Hull,
	Domain,
	Compute,
};

template <ShaderStage Stage>
class ShaderHandle
{
public:
	constexpr ShaderHandle() = default;

	constexpr bool IsValid() const noexcept
	{
		return index_ != InvalidIndex;
	}

	constexpr explicit operator bool() const noexcept
	{
		return IsValid();
	}

	friend constexpr bool operator==(
		ShaderHandle,
		ShaderHandle) = default;

private:
	static constexpr std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();
	explicit constexpr ShaderHandle(std::uint32_t index) noexcept : index_(index) {}

	std::uint32_t index_ = InvalidIndex;

	friend class ShaderManager;
};

using VertexShaderHandle =
ShaderHandle<ShaderStage::Vertex>;

using PixelShaderHandle =
ShaderHandle<ShaderStage::Pixel>;

using GeometryShaderHandle =
ShaderHandle<ShaderStage::Geometry>;

using ComputeShaderHandle =
ShaderHandle<ShaderStage::Compute>;


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
	std::span<const std::byte> GetByteCode() const { return bytecode_; }

private:
	Microsoft::WRL::ComPtr<ID3D11VertexShader> shader_;
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
	ID3D11PixelShader* GetNativeHandle() { return shader_.Get(); }

	[[nodiscard]]
	std::span<const std::byte> GetBytecode() const noexcept
	{
		return bytecode_;
	}

private:
	Microsoft::WRL::ComPtr<ID3D11PixelShader> shader_;
	std::vector<std::byte> bytecode_;
};