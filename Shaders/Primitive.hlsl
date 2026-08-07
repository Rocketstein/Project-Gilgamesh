struct VSOut
{
    float4 pos : SV_POSITION;
    float3 col : COLOR;
};

VSOut VSMain(float2 pos : POSITION , float3 col : COLOR)
{
    VSOut output;
    output.pos = float4(pos, 0.0f, 1.0f);
    output.col = col;
    return output;
};

float4 PSMain(VSOut input) : SV_TARGET
{
    return float4(input.col, 1.0f);
};