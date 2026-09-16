

float3 Unproject(float4x4 inverseViewProjection, float2 ndc, float depth)
{
    float4 world = mul(
        float4(ndc, depth, 1.0f),
        inverseViewProjection);
    return world.xyz / world.w;
}