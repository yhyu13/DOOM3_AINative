// Q4 hybrid opaque PBR shader for the capture harness.
//
// Direct light is a GGX/Smith/Schlick term with the same windowed
// inverse-square attenuation and PCF comparison shadow contract the portable
// T052/T053 slices describe. Indirect light evaluates the same L1 SH probe and
// the same below-horizon environment fallback. The display transform is the
// separate tonemap pass.
//
// Skinning: every vertex carries four bone indices and weights. Rigid draws use
// bone 0 with weight 1 and the host writes an identity palette, so the same
// vertex shader is an exact identity for them. Skinned draws receive a
// linear-blend palette computed on the host for the current frame.
//
// Matrices are row-major in the constant buffer and multiplied as row vectors:
//   world * viewProj, and world * lightViewProj for the shadow pass.

cbuffer Frame : register(b0)
{
    row_major float4x4 viewProj;
    row_major float4x4 lightViewProj;
    row_major float4x4 world;
    row_major float4x4 normalMatrix;
    float4 cameraPos;   // xyz, w = exposure
    float4 lightDir;    // xyz = travel direction, w = intensity
    float4 lightColor;  // rgb, w = 1 / shadow size
    float4 material;    // x = roughness scale, y = metalness, z = ao, w = flags
    float4 sh0;
    float4 sh1;
    float4 sh2;
    float4 envZenith;
    float4 envHorizon;
    float4 envGround;
    float4 fog;         // density, heightFalloff, cameraHeight, fogHeight
    float4 fogColor;    // rgb, w = enabled
    float4 params;      // x = alpha cutoff, y = alpha test, z = transmission
    float4 viewport;    // xy = render extent, zw = display extent
    float4 emissiveParams;      // x = scale, y = apply, zw unused
    float4 transmissionParams;  // x = ior, y = thickness, z = frost, w = enabled
    row_major float4x4 bones[12];
};

Texture2D<float4> baseColorTexture : register(t0);
Texture2D<float4> normalTexture : register(t1);
Texture2D<float4> roughnessTexture : register(t2);
Texture2D<float4> metalnessTexture : register(t3);
Texture2D<float4> aoTexture : register(t4);
Texture2D<float4> emissiveTexture : register(t5);
Texture2D<float> shadowTexture : register(t6);
Texture2D<float4> sceneCopyTexture : register(t7);
SamplerState linearSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    uint4 blendIndices : BLENDINDICES;
    float4 blendWeights : BLENDWEIGHT;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 worldPos : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 tangent : TEXCOORD2;
    float2 uv : TEXCOORD3;
};

float4x4 SkinMatrix(uint4 indices, float4 weights)
{
    float4x4 skin = (float4x4)0;
    [unroll] for (int i = 0; i < 4; ++i)
    {
        skin += bones[indices[i]] * weights[i];
    }
    return skin;
}

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    const float4x4 skin = SkinMatrix(input.blendIndices, input.blendWeights);
    float4 worldPosition = mul(mul(float4(input.position, 1.0), skin), world);
    output.position = mul(worldPosition, viewProj);
    output.worldPos = worldPosition.xyz;
    float3 skinnedNormal = mul(input.normal, (float3x3)skin);
    output.normal = normalize(mul(float4(skinnedNormal, 0.0), normalMatrix).xyz);
    float3 skinnedTangent = mul(input.tangent.xyz, (float3x3)skin);
    output.tangent = float4(
        normalize(mul(float4(skinnedTangent, 0.0), normalMatrix).xyz),
        input.tangent.w);
    output.uv = input.uv;
    return output;
}

struct ShadowOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

ShadowOutput VSShadow(VSInput input)
{
    ShadowOutput output;
    const float4x4 skin = SkinMatrix(input.blendIndices, input.blendWeights);
    float4 worldPosition = mul(mul(float4(input.position, 1.0), skin), world);
    output.position = mul(worldPosition, lightViewProj);
    // The shadow pixel shader needs the same UV the visible pass samples with,
    // so an alpha-tested caster writes depth only where its cutout is opaque.
    output.uv = input.uv;
    return output;
}

// Alpha-tested shadow pass. A caster whose material declares an alpha test
// (params.y) discards texels below the same cutoff (params.x) the visible pass
// uses, so a perforated surface casts a perforated shadow instead of its solid
// silhouette. Casters without an alpha test draw the full silhouette.
void PSShadowAlpha(ShadowOutput input)
{
    if (params.y > 0.5)
    {
        clip(baseColorTexture.Sample(linearSampler, input.uv).a - params.x);
    }
}

#define Q4_PI 3.14159265359

float3 ShCoefficient(int index)
{
    if (index == 0)
    {
        return sh0.xyz;
    }
    if (index == 1)
    {
        return float3(sh0.w, sh1.x, sh1.y);
    }
    if (index == 2)
    {
        return float3(sh1.z, sh1.w, sh2.x);
    }
    return sh2.yzw;
}

float3 EvaluateSh(float3 n)
{
    float3 c0 = ShCoefficient(0);
    float3 c1 = ShCoefficient(1);
    float3 c2 = ShCoefficient(2);
    float3 c3 = ShCoefficient(3);
    float3 value =
        c0 * 0.282095 + (c1 * n.y + c2 * n.z + c3 * n.x) * 0.488603;
    return max(value, 0.0);
}

float3 Environment(float3 reflected, float roughness)
{
    float y = reflected.y;
    float3 sky;
    if (y >= 0.0)
    {
        sky = lerp(envHorizon.rgb, envZenith.rgb, saturate(y));
    }
    else
    {
        // Below-horizon reflections use the ground/horizon blend, never black.
        sky = lerp(envHorizon.rgb, envGround.rgb, saturate(-y));
    }
    float3 average = (envZenith.rgb + envHorizon.rgb + envGround.rgb) / 3.0;
    return lerp(sky, average, saturate(roughness));
}

float DistributionGgx(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(Q4_PI * d * d, 1e-7);
}

float GeometrySmithGgx(float NdotV, float NdotL, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float lambdaV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float lambdaL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    return 0.5 / max(lambdaV + lambdaL, 1e-7);
}

float3 FresnelSchlick(float3 F0, float VdotH)
{
    return F0 + (1.0 - F0) * pow(saturate(1.0 - VdotH), 5.0);
}

float ShadowFactor(float3 worldPos, float3 N, float3 L)
{
    float4 lightPosition = mul(float4(worldPos, 1.0), lightViewProj);
    float3 projected = lightPosition.xyz / lightPosition.w;
    float2 uv = float2(projected.x * 0.5 + 0.5, 0.5 - projected.y * 0.5);
    if (any(uv < 0.0) || any(uv > 1.0))
    {
        return 1.0;
    }
    float bias = 0.0015 + 0.003 * (1.0 - saturate(dot(N, L)));
    float2 texel = lightColor.w;
    float sum = 0.0;
    [unroll] for (int y = -1; y <= 1; ++y)
    {
        [unroll] for (int x = -1; x <= 1; ++x)
        {
            sum += shadowTexture.SampleCmpLevelZero(
                shadowSampler, uv + float2(x, y) * texel, projected.z - bias);
        }
    }
    return sum / 9.0;
}

float4 PSFlat(VSOutput input) : SV_Target
{
    return float4(0.2, 0.4, 0.8, 1.0);
}

float4 PSBaseColor(VSOutput input) : SV_Target
{
    return float4(baseColorTexture.Sample(linearSampler, input.uv).rgb, 1.0);
}

float4 PSNormalRough(VSOutput input) : SV_Target
{
    float3 n = normalTexture.Sample(linearSampler, input.uv).rgb;
    float r = roughnessTexture.Sample(linearSampler, input.uv).r;
    return float4(n, r);
}

float4 PSShadow(VSOutput input) : SV_Target
{
    float3 n = normalize(input.normal);
    float3 L = normalize(-lightDir.xyz);
    float s = ShadowFactor(input.worldPos, n, L);
    return float4(s, s, s, 1.0);
}

float3 ApplyFog(float3 color, float3 worldPos)
{
    if (fogColor.w > 0.5)
    {
        float distanceToCamera = length(cameraPos.xyz - worldPos);
        float opticalDepth =
            max(fog.x, 0.0) * distanceToCamera *
            exp(-max(fog.y, 0.0) * max(fog.z - fog.w, 0.0));
        float transmittance = exp(-opticalDepth);
        return color * transmittance + fogColor.rgb * (1.0 - transmittance);
    }
    return color;
}

float4 PSMain(VSOutput input) : SV_Target
{
    uint flags = (uint)(material.w + 0.5);
    float4 albedoSample = baseColorTexture.Sample(linearSampler, input.uv);
    if (params.y > 0.5)
    {
        clip(albedoSample.a - params.x);
    }
    float3 baseColor = albedoSample.rgb;

    float3 geometricNormal = normalize(input.normal);
    float2 normalXY = normalTexture.Sample(linearSampler, input.uv).rg * 2.0 - 1.0;
    if ((flags & 1u) != 0u)
    {
        // The sidecar declares OpenGL handedness; DirectX TBN needs -Y.
        normalXY.y = -normalXY.y;
    }
    float3 tangentSpace = float3(
        normalXY, sqrt(saturate(1.0 - dot(normalXY, normalXY))));

    float3 tangent = input.tangent.xyz -
                     geometricNormal * dot(geometricNormal, input.tangent.xyz);
    float tangentLength = length(tangent);
    tangent = tangentLength > 1e-5
                  ? tangent / tangentLength
                  : normalize(cross(geometricNormal, float3(0.0, 1.0, 0.0)));
    float3 bitangent = cross(geometricNormal, tangent) * input.tangent.w;
    float3 normal = normalize(tangent * tangentSpace.x +
                              bitangent * tangentSpace.y +
                              geometricNormal * tangentSpace.z);

    float roughness = saturate(roughnessTexture.Sample(linearSampler, input.uv).r *
                               material.x);
    roughness = clamp(roughness, 0.045, 1.0);

    float metalness = material.y;
    if ((flags & 2u) != 0u)
    {
        metalness = metalnessTexture.Sample(linearSampler, input.uv).r;
    }
    metalness = saturate(metalness);

    float ao = material.z;
    if ((flags & 4u) != 0u)
    {
        ao = aoTexture.Sample(linearSampler, input.uv).r;
    }
    ao = saturate(ao);

    float3 emissive = 0.0;
    if ((flags & 8u) != 0u)
    {
        // The packaged mask is SDR; the sidecar's emissiveScale is applied
        // here, after sRGB decode, into the R16G16B16A16_FLOAT scene target.
        // Radiance above 1.0 therefore reaches the tonemap instead of clipping
        // in the texture.
        emissive = emissiveTexture.Sample(linearSampler, input.uv).rgb *
                   emissiveParams.x * emissiveParams.y;
    }

    float3 L = normalize(-lightDir.xyz);
    float3 V = normalize(cameraPos.xyz - input.worldPos);
    float3 H = normalize(L + V);
    float NdotL = saturate(dot(normal, L));
    float NdotV = saturate(dot(normal, V));
    float NdotH = saturate(dot(normal, H));
    float VdotH = saturate(dot(V, H));

    float3 F0 = lerp(0.04.xxx, baseColor, metalness);
    float D = DistributionGgx(NdotH, roughness);
    float Vis = GeometrySmithGgx(NdotV, NdotL, roughness);
    float3 F = FresnelSchlick(F0, VdotH);
    float3 specularTerm = D * Vis * F;
    float3 diffuseTerm = (1.0 - F) * (1.0 - metalness) * baseColor / Q4_PI;

    float shadow = ShadowFactor(input.worldPos, normal, L);
    float3 radiance = lightColor.rgb * lightDir.w;
    float3 direct = (diffuseTerm + specularTerm) * radiance * NdotL * shadow;

    float3 irradiance = EvaluateSh(normal);
    float3 diffuseIbl = irradiance * baseColor * (1.0 - metalness);
    float3 R = reflect(-V, normal);
    float3 reflected = Environment(R, roughness);
    float3 fresnelIbl =
        F0 + (1.0 - F0) * pow(saturate(1.0 - NdotV), 5.0);
    float3 specularIbl = reflected * fresnelIbl;
    float3 ambient = (diffuseIbl + specularIbl) * ao;

    float3 color = direct + ambient + emissive;
    color = ApplyFog(color, input.worldPos);
    return float4(color, 1.0);
}

// In-world GUI: scene-extent, depth-tested, transformed like any prop. It is
// deliberately unlit and not fogged, so it reads as a screen that follows the
// camera without being part of the display overlay.
float4 PSWorldGui(VSOutput input) : SV_Target
{
    float2 uv = saturate(input.uv);
    float3 panel = float3(0.015, 0.045, 0.060);
    float3 ink = float3(0.05, 0.55, 0.72);
    float3 warm = float3(0.85, 0.55, 0.15);

    // 3-pixel frame in a 128x80 virtual panel.
    float2 px = uv * float2(128.0, 80.0);
    bool border = px.x < 2.0 || px.y < 2.0 || px.x > 126.0 || px.y > 78.0;
    float3 color = border ? ink * 0.8 : panel;

    // Header row.
    if (px.y > 6.0 && px.y < 12.0 && px.x > 6.0 && px.x < 70.0)
    {
        color = ink;
    }
    // Four data rows with lengths that vary along the panel.
    [unroll] for (int row = 0; row < 4; ++row)
    {
        float rowY = 20.0 + row * 12.0;
        if (px.y > rowY && px.y < rowY + 5.0 && px.x > 8.0)
        {
            float length = 22.0 + 62.0 * frac(sin(dot(
                float2(row, floor(params.w * 0.25)),
                float2(12.9898, 78.233))) * 43758.5453);
            if (px.x < 8.0 + length)
            {
                color = (row == 2) ? warm : ink * 0.7;
            }
        }
    }
    // Moving scan column, advanced by the frame index.
    float scan = fmod(params.w * 9.0 + 6.0, 120.0);
    if (px.x > scan && px.x < scan + 2.0 && px.y > 14.0 && px.y < 74.0)
    {
        color += float3(0.12, 0.20, 0.22);
    }
    return float4(color, 1.0);
}

// Screen-space thin-slab refraction for the glass cell.
//
// Model, stated so it can be judged rather than guessed:
//   - the scene is rendered opaque first and copied to sceneCopyTexture;
//   - the pane publishes a geometric + normal-mapped normal (OpenGL handedness
//     decoded exactly like PSMain);
//   - the view ray is refracted once at entry (IoR from transmissionParams.x)
//     and traced through a slab of transmissionParams.y world units; the exit
//     point is projected to sample the pre-glass scene;
//   - frost (transmissionParams.z) widens the sample to five taps;
//   - baseColor tints the transmitted light, scaled by the authored alpha;
//   - a Fresnel environment reflection is added on top;
//   - the result is opaque-composited: no blend, no depth write.
// Not modelled: multiple internal reflections, dispersion, caustics, and
// per-layer depth rejection at silhouettes (the pane is an isolated quad).
float3 GlassNormal(VSOutput input, uint flags)
{
    float3 geometricNormal = normalize(input.normal);
    float2 normalXY = normalTexture.Sample(linearSampler, input.uv).rg * 2.0 - 1.0;
    if ((flags & 1u) != 0u)
    {
        normalXY.y = -normalXY.y;
    }
    float3 tangentSpace = float3(
        normalXY, sqrt(saturate(1.0 - dot(normalXY, normalXY))));
    float3 tangent = input.tangent.xyz -
                     geometricNormal * dot(geometricNormal, input.tangent.xyz);
    float tangentLength = length(tangent);
    tangent = tangentLength > 1e-5
                  ? tangent / tangentLength
                  : normalize(cross(geometricNormal, float3(0.0, 1.0, 0.0)));
    float3 bitangent = cross(geometricNormal, tangent) * input.tangent.w;
    return normalize(tangent * tangentSpace.x + bitangent * tangentSpace.y +
                     geometricNormal * tangentSpace.z);
}

float4 PSGlass(VSOutput input) : SV_Target
{
    uint flags = (uint)(material.w + 0.5);
    float4 albedo = baseColorTexture.Sample(linearSampler, input.uv);
    float3 N = GlassNormal(input, flags);
    float3 V = normalize(cameraPos.xyz - input.worldPos);
    float NdotV = saturate(dot(N, V));

    float3 entry = input.worldPos;
    float3 transmittedDir = refract(-V, N, 1.0 / max(transmissionParams.x, 1.01));
    float3 exitPos = entry + transmittedDir * transmissionParams.y;
    float4 clip = mul(float4(exitPos, 1.0), viewProj);
    float2 refractedUV = float2(clip.x / clip.w * 0.5 + 0.5,
                                0.5 - clip.y / clip.w * 0.5);

    float2 frost = transmissionParams.z * float2(1.4, 1.4) / viewport.xy;
    float3 background =
        sceneCopyTexture.Sample(linearSampler, refractedUV).rgb * 0.36;
    background += sceneCopyTexture.Sample(linearSampler, refractedUV + float2(frost.x, 0.0)).rgb * 0.16;
    background += sceneCopyTexture.Sample(linearSampler, refractedUV - float2(frost.x, 0.0)).rgb * 0.16;
    background += sceneCopyTexture.Sample(linearSampler, refractedUV + float2(0.0, frost.y)).rgb * 0.16;
    background += sceneCopyTexture.Sample(linearSampler, refractedUV - float2(0.0, frost.y)).rgb * 0.16;

    float tintStrength = saturate(0.12 + albedo.a * 0.30);
    float3 tint = lerp(1.0.xxx, albedo.rgb, tintStrength);
    float3 transmitted = background * tint;

    float3 R = reflect(-V, N);
    float3 reflected = Environment(R, 0.08);
    float fresnel = 0.04 + 0.96 * pow(1.0 - NdotV, 5.0);
    float3 color = lerp(transmitted, reflected, saturate(fresnel + 0.05));

    // A tight key glint only; the pane is not an opaque direct-light receiver.
    float3 L = normalize(-lightDir.xyz);
    float3 H = normalize(L + V);
    float glint = pow(saturate(dot(N, H)), 180.0) * 0.35 *
                  saturate(dot(N, L));
    color += lightColor.rgb * lightDir.w * glint;

    // No second fog pass: the transmitted background was already fogged in the
    // scene pass, so fogging the composite again would double-count it.
    return float4(color, 1.0);
}
