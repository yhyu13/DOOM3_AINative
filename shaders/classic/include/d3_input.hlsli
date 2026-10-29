#ifndef D3_INPUT_HLSLI
#define D3_INPUT_HLSLI

// Vertex input and inter-stage structures for the classic programs.
//
// The engine binds vertex data through ARB_vertex_program generic attributes. The GL slot numbers are
// an engine detail (and differ between the interaction path and the generic path, see abi.md
// section 1); in DXIL and WGSL the binding is by semantic name and location, so the port declares the
// semantic and the renderer maps the slot to it once. Do not copy GL slot numbers into program code.
//
// Mapping used by the port (abi.md section 1):
//   engine field     semantic     GL slot, interaction path     GL slot, generic/cube path
//   xyz              POSITION0    0                              0
//   st               TEXCOORD0    8                              8
//   tangents[0]      TANGENT0     9                              9
//   tangents[1]      TANGENT1     10                             10
//   normal           NORMAL0      11 (explicit attrib)           2 (glNormalPointer)
//   color            COLOR0       3 (glColorPointer)            3

// -------------------------------------------------------------------------------------------------
// Vertex stage inputs
// -------------------------------------------------------------------------------------------------

// Stencil shadow volumes pass a single idVec4 per vertex (shadowCache_t): xyz plus the homogeneous
// w that extrudes the far cap to infinity. No normal, tangent or colour exists on this path.
struct D3ShadowInput
{
    float4 Position : POSITION0;
};

// Surface geometry: idDrawVert, 60 bytes, in field order (DrawVert.h).
struct D3SurfaceInput
{
    float3 Position : POSITION0;
    float2 TexCoord : TEXCOORD0;
    float3 Normal   : NORMAL0;
    float3 Tangent0 : TANGENT0;
    float3 Tangent1 : TANGENT1;
    float4 Color    : COLOR0;
};

// Tier B generic stage: the port has no fixed-function pipeline, so the stage vertex shader receives
// the same surface layout and applies the material's texture matrix itself.
struct D3StageInput
{
    float3 Position : POSITION0;
    float2 TexCoord : TEXCOORD0;
    float3 Normal   : NORMAL0;
    float2 TexCoord1 : TEXCOORD1;
    float4 Color    : COLOR0;
};

// -------------------------------------------------------------------------------------------------
// Inter-stage structures
// -------------------------------------------------------------------------------------------------

// interaction.vfp: the fragment stage needs the model-space basis for the bump map, the three texture
// coordinate sets, the projected light coordinate and the interpolated vertex colour.
struct D3InteractionVSOutput
{
    float4 ClipPosition  : SV_Position;
    float3 ModelPosition : TEXCOORD0;
    float4 LightProjected : TEXCOORD1;   // xyz = S,T,Q of the light projection
    float2 BumpUV        : TEXCOORD2;
    float2 DiffuseUV     : TEXCOORD3;
    float2 SpecularUV    : TEXCOORD4;
    float3 Normal        : TEXCOORD5;
    float3 Tangent0      : TEXCOORD6;
    float3 Tangent1      : TEXCOORD7;
    float4 Color         : COLOR0;
};

// environment.vfp / bumpyEnvironment.vfp: cube reflection of the stage image, optionally perturbed by
// a bump stage.
struct D3SurfaceVSOutput
{
    float4 ClipPosition  : SV_Position;
    float3 ModelPosition : TEXCOORD0;
    float3 Normal        : TEXCOORD1;
    float2 BumpUV        : TEXCOORD2;
    float3 Tangent0      : TEXCOORD3;   // unused by environment, read by bumpyEnvironment
    float3 Tangent1      : TEXCOORD4;
    float4 Color         : COLOR0;
};

// Tier B stage and the glasswarp fragment program share one layout, because the engine binds only a
// fragment program for glasswarp and the port supplies the vertex stage from the generic path.
struct D3StageVSOutput
{
    float4 ClipPosition  : SV_Position;
    float2 TexCoord0     : TEXCOORD0;
    float2 TexCoord1     : TEXCOORD1;
    float3 ModelPosition : TEXCOORD2;
    float3 TexCoordCube  : TEXCOORD3;   // cube texgen modes; unused by explicit/screen modes
    float  FogFactor     : TEXCOORD4;   // fixed-function fog becomes explicit (abi.md section 6)
    float4 Color         : COLOR0;
};

#endif // D3_INPUT_HLSLI
