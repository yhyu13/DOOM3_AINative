#ifndef D3_ABI_HLSLI
#define D3_ABI_HLSLI

// Classic (ARB2) shader ABI: constant buffers, texture bindings, packing rules.
//
// Source of truth: specs/008-classic-shaders/abi.md. Every number or name here is traceable to that
// document; if the two disagree, the document wins and this header is wrong.
//
// Rules encoded here (abi.md section 8):
//   - every constant slot is float4 aligned, no float3 that crosses a backend boundary;
//   - matrices are four float4 rows in a declared order (the engine's GL state is column-major, so
//     the rows are the transposed columns);
//   - ARB c[n] numbers never appear here: names carry meaning;
//   - fp32 only: the ARB programs were fp32 and half precision changes parity;
//   - samplers and textures are separate objects, not combined samplers, so D3D12 and WebGPU can
//     both bind them.
//
// One constant buffer per program family (abi.md section 4). There is deliberately no global frame
// constant buffer: c[6..8] are light projection planes during an interaction and model-matrix columns
// during a generic stage, and fragment c[0..1] are screen scale per frame and diffuse/specular colour
// per interaction. c[4].w is 0 and c[5].w is 1 on both the interaction and the shadow path, so those
// two slots are safe to share.

// -------------------------------------------------------------------------------------------------
// Constant buffer layouts
// -------------------------------------------------------------------------------------------------

// Per-light surface interaction (interaction.vfp).
struct D3InteractionCB
{
    float4 LightOrigin;              // vertex c[4], model space; w = 0 on both paths
    float4 ViewOrigin;               // vertex c[5], model space, w = 1
    float4 LightProjectionS;         // vertex c[6]
    float4 LightProjectionT;         // vertex c[7]
    float4 LightProjectionQ;         // vertex c[8]
    float4 FalloffS;                 // vertex c[9]
    float4 BumpMatrixS;              // vertex c[10]
    float4 BumpMatrixT;              // vertex c[11]
    float4 DiffuseMatrixS;           // vertex c[12]
    float4 DiffuseMatrixT;           // vertex c[13]
    float4 SpecularMatrixS;          // vertex c[14]
    float4 SpecularMatrixT;          // vertex c[15]
    float4 ColorModulate;            // vertex c[16]
    float4 ColorAdd;                 // vertex c[17]
    float4 DiffuseColor;             // fragment c[0], per interaction
    float4 SpecularColor;            // fragment c[1], per interaction
    float4 SpecularExponent;         // authored: analytic specular power (x), scale (y)
    row_major float4x4 ModelViewProjection; // supplied by the port (abi.md section 6), row-vector convention
};

// Cube reflection without the interaction pass (environment.vfp, bumpyEnvironment.vfp).
struct D3EnvironmentCB
{
    float4 LocalViewOrigin;          // model space view origin
    float4 ColorModulate;
    float4 ColorAdd;
    row_major float4x4 ModelMatrix;
    row_major float4x4 ModelViewProjection;
};

// Stencil-volume projection (shadow.vp). The vertex program gets no normal, tangent or colour.
struct D3ShadowCB
{
    float4 LightOrigin;              // model space, w = 0: a direction, not a point
    row_major float4x4 ModelMatrix;
    row_major float4x4 ModelViewProjection;
};

// Generic (Tier B) stage: fixed-function replacement driven by texgen and material state.
struct D3StageCB
{
    float4 LocalViewOrigin;
    float4 TexGenS;                  // texture matrix row S
    float4 TexGenT;                  // texture matrix row T
    float4 ColorModulate;
    float4 ColorAdd;
    float4 FogColor;                 // rgb = colour, w = density
    float4 FogParams;                // x = linear fog start, y = linear fog end (distance in view units)
    float4 VertexParms[4];           // material vertexParm slots 0..3, a namespace of its own
    row_major float4x4 ModelMatrix;
    row_major float4x4 ModelViewProjection;
};

// Heat-haze / glass warp fragment stage (arbFP_glasswarp).
struct D3GlasswarpCB
{
    float4 ViewportScale;            // (1/w, 1/h, 0, 1): window to [0,1]
    float4 ScreenScale;              // (w/potW, h/potH, 0, 1)
    float4 WarpStrength;             // xy = distortion in screen units
    float4 Color;
};

// -------------------------------------------------------------------------------------------------
// Bindings
// -------------------------------------------------------------------------------------------------

// One binding slot per family. A program includes this header and uses exactly one family buffer;
// unused declarations are stripped by the compiler, and reflect.ps1 proves the surviving set.
cbuffer D3InteractionConstants : register(b0) { D3InteractionCB D3Interaction; };
cbuffer D3EnvironmentConstants : register(b0) { D3EnvironmentCB D3Environment; };
cbuffer D3ShadowConstants      : register(b0) { D3ShadowCB D3Shadow; };
cbuffer D3StageConstants       : register(b0) { D3StageCB D3Stage; };
cbuffer D3GlasswarpConstants   : register(b0) { D3GlasswarpCB D3Glasswarp; };

// Interaction pass: seven units, one image each (abi.md section 2.1).
//   0 normal cube  1 bump  2 falloff  3 light cube/projection  4 diffuse  5 specular  6 specular table
Texture2D    D3NormalCubeMap  : register(t0);
Texture2D    D3BumpMap        : register(t1);
Texture2D    D3FalloffTable   : register(t2);
TextureCube  D3LightCubeMap   : register(t3);
Texture2D    D3DiffuseMap     : register(t4);
Texture2D    D3SpecularMap    : register(t5);
Texture2D    D3SpecularTable  : register(t6);

SamplerState D3NormalCubeSampler : register(s0);
SamplerState D3BumpSampler       : register(s1);
SamplerState D3FalloffSampler    : register(s2);
SamplerState D3LightCubeSampler  : register(s3);
SamplerState D3DiffuseSampler    : register(s4);
SamplerState D3SpecularSampler   : register(s5);
SamplerState D3SpecularTableSampler : register(s6);

// Environment pass: 0 = the stage cube map, 1 = the bump stage image when one exists.
TextureCube  D3StageCubeMap   : register(t0);
Texture2D    D3EnvironmentBump : register(t1);
SamplerState D3StageCubeSampler : register(s0);
SamplerState D3EnvironmentBumpSampler : register(s1);

// Glasswarp pass: 0 = the stage image, 1 = scratchImage2 (heat haze), 2 = scratchImage.
Texture2D    D3StageImage     : register(t0);
Texture2D    D3ScratchImage2  : register(t1);
Texture2D    D3ScratchImage   : register(t2);
SamplerState D3StageImageSampler : register(s0);
SamplerState D3Scratch2Sampler   : register(s1);
SamplerState D3ScratchSampler    : register(s2);

// Generic stage: unit 0 is the stage image. Further fragmentMap units are declared by the variant.
Texture2D    D3GenericStageImage : register(t0);
SamplerState D3GenericStageSampler : register(s0);

#endif // D3_ABI_HLSLI
