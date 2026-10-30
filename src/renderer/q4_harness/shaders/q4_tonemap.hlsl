// Reconstruction + tone-map pass for the Q4 capture harness.
//
// The scene target is rendered at the render extent. This pass samples it at
// the display extent with a clamped linear sampler (the bilinear reconstruction
// path when the extents differ) and applies ACES + exposure. The display target
// is R8G8B8A8_UNORM_SRGB, so the hardware performs the display encoding.

Texture2D<float4> sceneTexture : register(t0);
SamplerState linearSampler : register(s0);

cbuffer Tonemap : register(b0)
{
    float exposure;
    float mode;
    float2 padding;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(uint vertexId : SV_VertexID)
{
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    VSOutput output;
    output.position =
        float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    output.uv = uv;
    return output;
}

float3 AcesFilm(float3 value)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((value * (a * value + b)) /
                    (value * (c * value + d) + e));
}

float4 PSMain(VSOutput input) : SV_Target
{
    float3 color = sceneTexture.Sample(linearSampler, input.uv).rgb;
    color *= exposure;
    color = AcesFilm(color);
    return float4(color, 1.0);
}
