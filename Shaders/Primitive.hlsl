cbuffer ObjectConstants : register(b0)
{
    float4x4 modelViewProjection;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float3 col : COLOR;
};

VSOut VSMain(float2 pos : POSITION , float3 col : COLOR)
{
    VSOut output;
    output.pos =
        mul(float4(pos, 1.0f), modelViewProjection);
    output.col = col;
    return output;
};

float4 PSMain(VSOut input) : SV_TARGET
{
    return float4(input.col, 1.0f);
};