// interaction.vfp replacement: per-light surface interaction.
//
// ABI: specs/008-classic-shaders/abi.md sections 1, 2.1, 3, 4, 6.
//
// This is an independent implementation from the ABI and the public ARB specifications. The retail
// program text is not in this repository and was not consulted (provenance P-1).
//
// Authored decisions, each recorded in evidence/implementation.md because the retail program cannot be
// read to settle it:
//   D-I1  lighting is evaluated in model space, consistent with c[4] (light origin) and c[5] (view
//         origin) being written in model space;
//   D-I2  the bump/diffuse/specular matrices transform the model-space position, not the raw texcoord;
//   D-I3  the diffuse term uses the half-Lambert wrap from d3_arb.hlsli;
//   D-I4  specular is analytic (Blinn-Phong power from SpecularExponent.x) instead of a lookup in the
//         two-dimensional specular table bound at t6, which this milestone does not model;
//   D-I5  attenuation is the falloff table indexed by the light-projection S coordinate, multiplied by
//         the light cube sampled along the direction from the light to the fragment.
// Parity for every one of these is unverified until T-027 runs on authorized data.

#include "../include/d3_abi.hlsli"
#include "../include/d3_input.hlsli"
#include "../include/d3_arb.hlsli"

D3InteractionVSOutput vs_main(D3SurfaceInput input)
{
    D3InteractionVSOutput o;
    float4 modelPosition = float4(input.Position, 1.0f);

    o.ClipPosition = mul(modelPosition, D3Interaction.ModelViewProjection);
    o.ModelPosition = input.Position;

    // Light projection planes, applied to the model-space position (decision D-I1).
    o.LightProjected = float4(dot(modelPosition, D3Interaction.LightProjectionS),
                              dot(modelPosition, D3Interaction.LightProjectionT),
                              dot(modelPosition, D3Interaction.LightProjectionQ),
                              1.0f);

    // Three texture coordinate sets from the S/T matrix pairs (decision D-I2).
    o.BumpUV     = D3_TexCoordFromPosition(input.Position, D3Interaction.BumpMatrixS, D3Interaction.BumpMatrixT);
    o.DiffuseUV  = D3_TexCoordFromPosition(input.Position, D3Interaction.DiffuseMatrixS, D3Interaction.DiffuseMatrixT);
    o.SpecularUV = D3_TexCoordFromPosition(input.Position, D3Interaction.SpecularMatrixS, D3Interaction.SpecularMatrixT);

    // Model-space basis for the bump map; the fragment stage reconstructs the normal from it.
    o.Normal   = input.Normal;
    o.Tangent0 = input.Tangent0;
    o.Tangent1 = input.Tangent1;
    o.Color    = input.Color;
    return o;
}

float4 ps_main(D3InteractionVSOutput input) : SV_Target
{
    float3 sampledNormal = D3BumpMap.Sample(D3BumpSampler, input.BumpUV).xyz;
    float3 n = D3_BasisNormal(sampledNormal, input.Tangent0, input.Tangent1, input.Normal);

    float3 v = normalize(D3Interaction.ViewOrigin.xyz - input.ModelPosition);
    float3 l = normalize(D3Interaction.LightOrigin.xyz - input.ModelPosition);
    float3 h = normalize(l + v);

    float ndl = dot(n, l);
    float diffuseTerm = D3_HalfLambert(ndl);                                   // D-I3
    float specularTerm = pow(saturate(dot(n, h)), max(D3Interaction.SpecularExponent.x, 1.0f)); // D-I4

    float4 albedo = D3DiffuseMap.Sample(D3DiffuseSampler, input.DiffuseUV);
    float4 specularTint = D3SpecularMap.Sample(D3SpecularSampler, input.SpecularUV) * D3Interaction.SpecularColor;

    float falloff = D3FalloffTable.Sample(D3FalloffSampler, float2(input.LightProjected.x, 0.5f)).r;
    float3 lightDirection = normalize(input.ModelPosition - D3Interaction.LightOrigin.xyz);
    float lightCube = D3LightCubeMap.Sample(D3LightCubeSampler, lightDirection).r;
    float attenuation = saturate(falloff * lightCube);                          // D-I5

    float3 diffuse = D3Interaction.DiffuseColor.rgb * albedo.rgb * diffuseTerm;
    diffuse *= input.Color.rgb * D3Interaction.ColorModulate.rgb;

    float3 specular = specularTint.rgb * specularTerm * D3Interaction.ColorModulate.rgb;

    float3 color = (diffuse + specular) * attenuation + D3Interaction.ColorAdd.rgb * albedo.rgb;
    return float4(saturate(color), albedo.a);
}
