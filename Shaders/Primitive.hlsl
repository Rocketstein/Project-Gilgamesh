#include "Common/ConstantBuffer.hlsli"

struct VSOut
{
    float4 pos : SV_POSITION;
    float3 col : COLOR;
};

VSOut VSMain(float3 pos : POSITION, float3 col : COLOR)
{
    VSOut output;
    // Keep per-object transforms separate from the current camera view.
    output.pos =
        mul(mul(float4(pos, 1.0f), model), viewProjection);
    output.col = col;
    return output;
};

float4 PSMain(VSOut input) : SV_TARGET
{
    return float4(input.col, 1.0f);
};
