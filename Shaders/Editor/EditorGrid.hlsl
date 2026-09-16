#include "Common/Functions.hlsli"
#include "Common/ConstantBuffer.hlsli"

struct PSOutput
{
    float4 color : SV_TARGET;
    float depth : SV_Depth;
};

// Draws a full screen quad
float4 VSMain(uint vertexID : SV_VertexID) : SV_POSITION
{
    float2 corner = float2(
        vertexID == 2 ? 3.0f : -1.0f,
        vertexID == 1 ? 3.0f : -1.0f);
    return float4(corner, 0.0f, 1.0f);
}

PSOutput PSMain(float4 pos : SV_POSITION)
{
    float2 UV = pos.xy / viewportDim;
    float2 NDC = float2(UV.x * 2 - 1.0f,
                        1.0f - UV.y * 2);
    
    float3 nearPoint = Unproject(NDC, 0.0f);
    float3 farPoint  = Unproject(NDC, 1.0f);
    
    // Ray intersection with the grid plane
    float deltaZ = farPoint.z - nearPoint.z;
    float safeDeltaZ = deltaZ < 0.0f
    ? min(deltaZ, -1e-6f)
    : max(deltaZ, 1e-6f);
    float t = -nearPoint.z / safeDeltaZ; // XY plane at Z = 0
    float3 hit = lerp(nearPoint, farPoint, t);
    
    float2 grid = hit.xy / gridSpacing;
    float2 distanceToLine = abs(frac(grid + 0.5f) - 0.5f);
    float2 pixelWidth = max(fwidth(grid), 1e-5f) * gridWidth;
    float2 axisLines = 1.0f - saturate(distanceToLine / pixelWidth);
    float lineAlpha = max(axisLines.x, axisLines.y);
    
    float cameraDistance = distance(hit, cameraWorldPos);
    float distanceAlpha = 1.0f - smoothstep(
        gridFadeRange.x, gridFadeRange.y, cameraDistance);
    float alpha = gridColor.a * lineAlpha * distanceAlpha;

    // Keep fwidth above these per-pixel rejection branches.
    if (abs(deltaD) < 1e-6f || t < 0.0f || t > 1.0f)
        discard;
    clip(alpha - 1e-3f);

    float4 projected = mul(float4(hit, 1.0f), viewProjection);
    PSOutput output;
    output.color = float4(gridColor.rgb, alpha);
    output.depth = projected.z / projected.w;
    return output;
}