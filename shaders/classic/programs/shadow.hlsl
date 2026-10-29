// shadow.vp replacement: stencil-volume projection to infinity.
//
// ABI: specs/008-classic-shaders/abi.md sections 1 and 3.1.
//
// Input is shadowCache_t: one idVec4 per vertex. RB_StencilShadowPass binds four floats per vertex and
// no other attribute exists on this path, so this program reads POSITION0 and nothing else.
//
// The homogeneous trick: the cache stores the near cap with w = 1 and the far cap with w = 0. Passing
// that w straight into the transform places the far cap at infinity in clip space, which is exactly
// the extrusion the stencil-volume technique needs; the rasteriser then clips the resulting volume
// correctly without a per-frame projection variant.
//
// The retail program text is not in this repository and was not consulted (provenance P-1). This is an
// independent implementation from the ABI; the stencil behaviour is unverified until a parity capture
// on authorized data (T-027).

#include "../include/d3_abi.hlsli"
#include "../include/d3_input.hlsli"

float4 vs_main(D3ShadowInput input) : SV_Position
{
    // Row-vector convention: the constant holds four rows and mul(v, M) applies them in order.
    return mul(float4(input.Position.xyz, input.Position.w), D3Shadow.ModelViewProjection);
}
