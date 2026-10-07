cbuffer FrameConstants : register(b0)
{
	float4x4 view;
	float4x4 projection;
	float4x4 viewProjection;
	float4x4 inverseViewProj;
	float3 cameraWorldPos;
	float2 viewportDim;
};

cbuffer EditorConstants : register(b1)
{
	// Editor Grid
	float4 gridColor;
	float2 gridFadeRange; // x: fully visible distance; y: invisible distance
	float gridSpacing;
	float gridWidth;
	
};

cbuffer ObjectConstants : register(b2)
{
	float4x4 model;
};