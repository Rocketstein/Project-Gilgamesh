#include "Common/ConstantBuffer.hlsli"

struct VSOut
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
};

// Debug endpoints already occupy world space, so only the camera transform applies.
VSOut VSMain(float3 position : POSITION, float3 color : COLOR)
{
    VSOut output;
    output.position = mul(float4(position, 1.0f), viewProjection);
    output.color = color;
    return output;
}

float4 PSMain(VSOut input) : SV_TARGET
{
    return float4(input.color, 1.0f);
}
