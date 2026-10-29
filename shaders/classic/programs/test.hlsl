// test.vfp replacement: the debug-only program bound under r_testARBProgram.
//
// ABI: specs/008-classic-shaders/abi.md sections 1, 2.1, 3.
//
// The debug program exists so a developer can see what the interaction path is feeding the fragment
// stage without a reference renderer. It visualises the decoded model-space normal from the bump map.
// It is not a parity program and must stay out of any comparison.
//
// Independent implementation from the ABI; retail text not consulted (provenance P-1).

#include "../include/d3_abi.hlsli"
#include "../include/d3_input.hlsli"
#include "../include/d3_arb.hlsli"

D3InteractionVSOutput vs_main(D3SurfaceInput input)
{
    D3InteractionVSOutput o;
    float4 modelPosition = float4(input.Position, 1.0f);
    o.ClipPosition = mul(modelPosition, D3Interaction.ModelViewProjection);
    o.ModelPosition = input.Position;
    o.LightProjected = float4(dot(modelPosition, D3Interaction.LightProjectionS),
                              dot(modelPosition, D3Interaction.LightProjectionT),
                              dot(modelPosition, D3Interaction.LightProjectionQ),
                              1.0f);
    o.BumpUV     = D3_TexCoordFromPosition(input.Position, D3Interaction.BumpMatrixS, D3Interaction.BumpMatrixT);
    o.DiffuseUV  = D3_TexCoordFromPosition(input.Position, D3Interaction.DiffuseMatrixS, D3Interaction.DiffuseMatrixT);
    o.SpecularUV = D3_TexCoordFromPosition(input.Position, D3Interaction.SpecularMatrixS, D3Interaction.SpecularMatrixT);
    o.Normal   = input.Normal;
    o.Tangent0 = input.Tangent0;
    o.Tangent1 = input.Tangent1;
    o.Color    = input.Color;
    return o;
}

float4 ps_main(D3InteractionVSOutput input) : SV_Target
{
    float3 sampled = D3BumpMap.Sample(D3BumpSampler, input.BumpUV).xyz;
    float3 n = D3_BasisNormal(sampled, input.Tangent0, input.Tangent1, input.Normal);
    return float4(n * 0.5f + 0.5f, 1.0f);
}
