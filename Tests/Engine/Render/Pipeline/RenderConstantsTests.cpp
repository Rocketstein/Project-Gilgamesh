#include "Engine/Render/Pipeline/Resource/RenderConstants.h"
#include "Tests/TestFramework.h"

#include <cstddef>

GILGAMESH_TEST(
    "Engine.Render.RenderConstants",
    "MatchesConstantBufferPacking")
{
    // Validate the explicit sizes consumed by D3D11 constant buffers.
    GILGAMESH_CHECK_MESSAGE(
        sizeof(FrameConstants) == 288 &&
        sizeof(EditorConstants) == 32 &&
        sizeof(ObjectConstants) == 64,
        "render constants must occupy complete 16-byte registers");

    // The float3 gap must place viewportDim at the next HLSL register.
    GILGAMESH_CHECK_MESSAGE(
        offsetof(FrameConstants, cameraWorldPos) == 256 &&
        offsetof(FrameConstants, viewportDim) == 272,
        "frame constants must match HLSL register packing");
}
