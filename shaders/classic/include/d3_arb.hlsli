#ifndef D3_ARB_HLSLI
#define D3_ARB_HLSLI

// ARB opcode emulation for the classic programs.
//
// The retail programs were ARB assembly, so their semantics include a few operations that have no
// direct HLSL builtin. This header gives each one a name and the meaning the public ARB
// specifications assign to it (abi.md section 7). It contains no retail program text: everything here
// is written from the opcode descriptions and the ABI.
//
// Deliberately absent: implicit fixed-function state. There are no implicit matrices, no fog and no
// built-in texture matrices; programs read the explicit constant buffers in d3_abi.hlsli.

// Saturating write: the ARB destination saturates to [0,1] where the instruction says so.
float4 D3_Saturate(float4 v) { return saturate(v); }
float3 D3_Saturate3(float3 v) { return saturate(v); }

// LRP: linear interpolate, dst = a * (1 - t) + b * t.
float4 D3_Lrp(float4 a, float4 b, float t) { return lerp(a, b, t); }
float3 D3_Lrp3(float3 a, float3 b, float t) { return lerp(a, b, t); }

// CMP: component-wise select, dst = (src0 < 0) ? src1 : src2. ARB tests the sign, not against another
// operand, which is why the comparison is against zero here.
float4 D3_Cmp(float4 src0, float4 lt, float4 ge)
{
    return float4(src0.x < 0.0f ? lt.x : ge.x,
                  src0.y < 0.0f ? lt.y : ge.y,
                  src0.z < 0.0f ? lt.z : ge.z,
                  src0.w < 0.0f ? lt.w : ge.w);
}

// DST: distance vector, dst = (1, d0, d1, d2) with d0 = |a0 - b0| and d1/d2 evaluated only when the
// corresponding pair is nonzero, exactly as the ARB definition states.
float4 D3_Dst(float4 a, float4 b)
{
    float d0 = abs(a.x - b.x);
    float d1 = dot(a.yz, b.yz) != 0.0f ? abs(a.y - b.y) : 0.0f;
    float d2 = dot(a.w, b.w) != 0.0f ? abs(a.w - b.w) : 0.0f;
    return float4(1.0f, d0, d1, d2);
}

// SCS: sine and cosine pair, dst = (cos(x), sin(x), 0, 0), scaled by the second argument when used.
float2 D3_Scs(float x) { return float2(cos(x), sin(x)); }

// LIT: the ARB lighting helper. dst = (1, max(dot(n, l), 0), pow(max(dot(n, l), 0), exponent), 1).
// The ARB opcode derives the exponent from the source w component of the light vector; callers pass it
// explicitly so the constant is visible at the call site.
float4 D3_Lit(float3 normal, float3 light, float exponent)
{
    float ndl = max(dot(normal, light), 0.0f);
    float spec = ndl > 0.0f ? pow(ndl, max(exponent, 1.0f)) : 0.0f;
    return float4(1.0f, ndl, spec, 1.0f);
}

// TXP: projective texture read. ARB TXP divides the coordinate by q and then samples; the HLSL
// equivalent needs an explicit divide and an explicit LOD, because a non-uniform branch around an
// implicit-LOD sample is undefined in both DXIL and WGSL.
float4 D3_TexProj(Texture2D tex, SamplerState smp, float4 coord)
{
    float3 uvw = coord.xyz / coord.w;
    return tex.SampleLevel(smp, uvw.xy, 0.0f);
}

// TXB: sample with a bias, spelled out so the LOD is part of the call.
float4 D3_TexBias(Texture2D tex, SamplerState smp, float2 uv, float bias)
{
    return tex.SampleBias(smp, uv, bias);
}

// TXP against a cube map.
float4 D3_TexProjCube(TextureCube tex, SamplerState smp, float4 coord)
{
    float3 dir = coord.xyz / coord.w;
    return tex.SampleLevel(smp, dir, 0.0f);
}

// Tangent-space normal decode from a stored normal map, then the model-space basis combination every
// bump-mapped classic program needs. Tangents are the engine's tangents[0] / tangents[1] (S and T
// directions); the basis is orthonormalised so a slightly non-unit mesh tangent cannot skew the light
// vector.
float3 D3_BasisNormal(float3 sampled, float3 tangent0, float3 tangent1, float3 normal)
{
    float3 n = normalize(normal);
    float3 t = normalize(tangent0 - n * dot(n, tangent0));
    float3 b = normalize(tangent1 - n * dot(n, tangent1) - t * dot(t, tangent1));
    float3 ts = sampled * 2.0f - 1.0f;
    return normalize(t * ts.x + b * ts.y + n * ts.z);
}

// Half-Lambert wrap used by the authored interaction model. This is a decision recorded in the
// implementation evidence, not a claim about the retail program, whose text is not in this repository.
float D3_HalfLambert(float ndl)
{
    return saturate(ndl * 0.5f + 0.5f);
}

// Apply a texture matrix pair to a model-space position: u = dot((pos,1), S), v = dot((pos,1), T).
// The engine writes the bump/diffuse/specular matrices as S/T row pairs for exactly this use
// (abi.md section 3.1), and the classic texgen path transforms the position rather than the raw
// texcoord. Stated as an authored decision because the retail program text is unavailable.
float2 D3_TexCoordFromPosition(float3 position, float4 matrixS, float4 matrixT)
{
    float4 p = float4(position, 1.0f);
    return float2(dot(p, matrixS), dot(p, matrixT));
}

#endif // D3_ARB_HLSLI
