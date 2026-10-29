// bumpyEnvironment.vfp replacement: cube reflection perturbed by a bump stage.
//
// ABI: specs/008-classic-shaders/abi.md sections 1, 2.2, 4, 6.
//
// Independent implementation from the ABI; retail text not consulted (provenance P-1).
//
// Authored decisions:
//   D-E1  model-space lighting, as in environment.hlsl;
//   D-B1  the bump map at unit 1 is a tangent-space normal map; the basis comes from the engine's two
//         tangent vectors and the vertex normal (d3_arb.hlsli D3_BasisNormal);
//   D-B2  the perturbed normal replaces the geometric normal for the reflection direction only; the
//         reflected term is still unlit, matching the environment family.
// Parity unverified until T-027.

#include "../include/d3_abi.hlsli"
#include "../include/d3_input.hlsli"
#include "../include/d3_arb.hlsli"

D3SurfaceVSOutput vs_main(D3SurfaceInput input)
{
    D3SurfaceVSOutput o;
    float4 modelPosition = float4(input.Position, 1.0f);
    o.ClipPosition = mul(modelPosition, D3Environment.ModelViewProjection);
    o.ModelPosition = input.Position;
    o.Normal = input.Normal;
    o.BumpUV = input.TexCoord;
    o.Tangent0 = input.Tangent0;
    o.Tangent1 = input.Tangent1;
    o.Color = input.Color;
    return o;
}

float4 ps_main(D3SurfaceVSOutput input) : SV_Target
{
    float3 sampledNormal = D3EnvironmentBump.Sample(D3EnvironmentBumpSampler, input.BumpUV).xyz;
    float3 n = D3_BasisNormal(sampledNormal, input.Tangent0, input.Tangent1, input.Normal);   // D-B1
    float3 v = normalize(D3Environment.LocalViewOrigin.xyz - input.ModelPosition);
    float3 r = reflect(-v, n);

    float4 reflected = D3StageCubeMap.Sample(D3StageCubeSampler, r);
    float3 color = reflected.rgb * input.Color.rgb * D3Environment.ColorModulate.rgb + D3Environment.ColorAdd.rgb;
    return float4(saturate(color), reflected.a);
}
