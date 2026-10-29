// Tier B: generic stage shader, the replacement for the fixed-function stage path.
//
// ABI: specs/008-classic-shaders/abi.md sections 1, 5, 6; bounds in
// specs/008-classic-shaders/ownership.md section 3.1.
//
// The classic renderer draws ordinary stages (a material stage with no program keyword) through
// fixed-function texgen, alpha test and vertex colour. The portable RHI has no fixed-function path, so
// this file supplies it. Blend, depth, stencil and cull stay pipeline state and MUST NOT become shader
// variants; only the three axes below multiply.
//
// Variants (7 x 2 x 3 = 42 upper bound):
//   D3_TEXGEN_MODE     0 explicit, 1 diffuse cube, 2 reflect cube, 3 skybox cube,
//                      4 wobble-sky cube, 5 screen, 6 screen2
//                      (glasswarp is NOT a Tier B mode: Tier A owns arbFP_glasswarp)
//   D3_ALPHATEST       0 off, 1 on (reference from VertexParms[0].y, decision D-S2)
//   D3_VERTEXCOLOR     0 ignore, 1 modulate, 2 inverse modulate
//
// Only the combinations listed in stages/variants.json are built. The bound is proven, the shipped set
// is census-driven: an unobserved variant is not compiled and not shipped.
//
// Authored decisions:
//   D-S1  screen texgen maps clip space to [0,1] with the Y axis flipped to match the GL convention;
//   D-S2  the alpha-test reference is VertexParms[0].y, because the engine passes the material's
//         alphaTest value through local parameters rather than a fixed slot;
//   D-S3  fog is linear between FogParams.x and FogParams.y, applied in the fragment stage.

#include "../include/d3_abi.hlsli"
#include "../include/d3_input.hlsli"
#include "../include/d3_arb.hlsli"

#ifndef D3_TEXGEN_MODE
#define D3_TEXGEN_MODE 0
#endif
#ifndef D3_ALPHATEST
#define D3_ALPHATEST 0
#endif
#ifndef D3_VERTEXCOLOR
#define D3_VERTEXCOLOR 1
#endif

#if D3_TEXGEN_MODE < 0 || D3_TEXGEN_MODE > 6
#error "D3_TEXGEN_MODE out of range; glasswarp belongs to Tier A"
#endif

// Cube and 2D stage images occupy the same unit 0 in the engine; the port binds one or the other, so
// exactly one of these two declarations survives into each variant and they never collide.
TextureCube D3GenericStageCube : register(t0);

D3StageVSOutput vs_main(D3StageInput input)
{
    D3StageVSOutput o;
    float4 modelPosition = float4(input.Position, 1.0f);
    o.ClipPosition = mul(modelPosition, D3Stage.ModelViewProjection);
    o.ModelPosition = input.Position;
    o.Color = input.Color;

    float3 v = normalize(D3Stage.LocalViewOrigin.xyz - input.Position);
    float3 n = normalize(input.Normal);

    o.TexCoord0 = float2(0.0f, 0.0f);
    o.TexCoord1 = float2(0.0f, 0.0f);
    o.TexCoordCube = float3(0.0f, 0.0f, 1.0f);

#if D3_TEXGEN_MODE == 0
    o.TexCoord0 = D3_TexCoordFromPosition(input.Position, D3Stage.TexGenS, D3Stage.TexGenT);
#elif D3_TEXGEN_MODE == 1
    o.TexCoordCube = n;
#elif D3_TEXGEN_MODE == 2
    o.TexCoordCube = reflect(-v, n);
#elif D3_TEXGEN_MODE == 3
    o.TexCoordCube = input.Position - D3Stage.LocalViewOrigin.xyz;
#elif D3_TEXGEN_MODE == 4
    o.TexCoordCube = input.Position - D3Stage.LocalViewOrigin.xyz
                   + 0.05f * float3(sin(input.Position.y * 8.0f), 0.0f, cos(input.Position.x * 8.0f));
#elif D3_TEXGEN_MODE == 5 || D3_TEXGEN_MODE == 6
    float4 clip = o.ClipPosition;
    float2 ndc = clip.xy / max(clip.w, 1e-6f);
    float2 screenUv = float2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f);   // D-S1
    o.TexCoord0 = screenUv;
    o.TexCoord1 = screenUv * D3Stage.TexGenS.xy;
#endif

    float viewDistance = length(D3Stage.LocalViewOrigin.xyz - input.Position);
    float fogRange = max(D3Stage.FogParams.y - D3Stage.FogParams.x, 1e-6f);
    o.FogFactor = saturate((viewDistance - D3Stage.FogParams.x) / fogRange) * D3Stage.FogColor.w;  // D-S3
    return o;
}

float4 ps_main(D3StageVSOutput input) : SV_Target
{
#if D3_TEXGEN_MODE >= 1 && D3_TEXGEN_MODE <= 4
    float4 base = D3GenericStageCube.Sample(D3GenericStageSampler, normalize(input.TexCoordCube));
#else
    float4 base = D3GenericStageImage.Sample(D3GenericStageSampler, input.TexCoord0);
#endif

#if D3_ALPHATEST
    clip(base.a - D3Stage.VertexParms[0].y);                                  // D-S2
#endif

#if D3_VERTEXCOLOR == 0
    float3 tint = float3(1.0f, 1.0f, 1.0f);
#elif D3_VERTEXCOLOR == 1
    float3 tint = input.Color.rgb;
#else
    float3 tint = float3(1.0f, 1.0f, 1.0f) - input.Color.rgb;
#endif

    float3 color = base.rgb * tint;
    color = lerp(color, D3Stage.FogColor.rgb, saturate(input.FogFactor));
    return float4(saturate(color), base.a);
}
