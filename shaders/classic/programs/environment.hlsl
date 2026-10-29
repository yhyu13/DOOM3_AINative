// environment.vfp replacement: cube reflection of the stage image without a bump stage.
//
// ABI: specs/008-classic-shaders/abi.md sections 1, 2.2, 4, 6.
//
// Independent implementation from the ABI; retail text not consulted (provenance P-1).
//
// Authored decisions:
//   D-E1  the reflection direction is computed in model space, because the environment constant buffer
//         carries LocalViewOrigin in model space (abi.md section 4);
//   D-E2  the cube map stores the reflected radiance of the stage image; no lighting term is added,
//         because the classic environment stage is a reflection stage, not an interaction.
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
    float3 n = normalize(input.Normal);
    float3 v = normalize(D3Environment.LocalViewOrigin.xyz - input.ModelPosition);
    float3 r = reflect(-v, n);                                                 // D-E1

    float4 reflected = D3StageCubeMap.Sample(D3StageCubeSampler, r);
    float3 color = reflected.rgb * input.Color.rgb * D3Environment.ColorModulate.rgb + D3Environment.ColorAdd.rgb;
    return float4(saturate(color), reflected.a);
}
