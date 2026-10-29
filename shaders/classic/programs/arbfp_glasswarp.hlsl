// arbFP_glasswarp replacement: heat-haze / glass warp fragment stage.
//
// ABI: specs/008-classic-shaders/abi.md sections 2.3, 4, 6.
//
// The engine binds only a fragment program for the glasswarp stage (census.md section 3, entry 18), so
// the vertex stage comes from the port's generic path and the shared D3StageVSOutput layout is used.
//
// Independent implementation from the ABI; retail text not consulted (provenance P-1).
//
// Authored decisions:
//   D-G1  the warp offset is the two-channel heat-haze image at unit 1 (scratchImage2), remapped from
//         [0,1] to [-1,1] and scaled by WarpStrength;
//   D-G2  the distorted sample of the stage image at unit 0 is the result; the second scratch image at
//         unit 2 (scratchImage) is bound by the engine but is not yet used, which is recorded as a
//         parity gap rather than hidden by a dummy read.
// Parity unverified until T-027.

#include "../include/d3_abi.hlsli"
#include "../include/d3_input.hlsli"
#include "../include/d3_arb.hlsli"

float4 ps_main(D3StageVSOutput input) : SV_Target
{
    float2 haze = D3ScratchImage2.Sample(D3Scratch2Sampler, input.TexCoord1).xy * 2.0f - 1.0f;  // D-G1
    float2 uv = input.TexCoord0 + haze * D3Glasswarp.WarpStrength.xy;

    float4 stage = D3StageImage.Sample(D3StageImageSampler, uv);                                // D-G2
    float3 color = stage.rgb * D3Glasswarp.Color.rgb * input.Color.rgb;
    return float4(saturate(color), stage.a * D3Glasswarp.Color.a);
}
