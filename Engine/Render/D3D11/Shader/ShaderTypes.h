#pragma once
#include <cstdint>
#include <limits>

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
