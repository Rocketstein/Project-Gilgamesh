#include "RenderView.h"

#include "Engine/Core/Logging/Logger.h"

// All scene and editor passes share the same row-vector and transposed-upload contract.
std::optional<FrameConstants> PrepareViewConstants(const RenderView& view)
{
	// A shader view must describe real pixels and an invertible camera transform.
	if (view.extent.width == 0 || view.extent.height == 0)
	{
		GILGAMESH_LOG(Renderer, Error, "Cannot prepare view constants for an empty viewport");
		return std::nullopt;
	}

	const Matrix4 viewProjection = view.view * view.projection;
	Matrix4 inverseVP;
	if (!TryInverse(viewProjection, inverseVP))
	{
		GILGAMESH_LOG(Renderer, Error,
			"Cannot prepare view constants for a non-invertible view projection");
		return std::nullopt;
	}

	// Keep CPU values independent for each view until its graph callback executes.
	FrameConstants constants{};
	constants.view = Transpose(view.view);
	constants.projection = Transpose(view.projection);
	constants.viewProjection = Transpose(viewProjection);
	constants.inverseVP = Transpose(inverseVP);
	constants.cameraWorldPos = view.cameraWorldPos;
	constants.viewportDim = {
		static_cast<float>(view.extent.width),
		static_cast<float>(view.extent.height)
	};
	return constants;
}
