// Q4 hybrid opaque PBR shader for the capture harness.
//
// Direct light is a GGX/Smith/Schlick term with the same windowed
// inverse-square attenuation and PCF comparison shadow contract the portable
// T052/T053 slices describe. Indirect light evaluates the same L1 SH probe and
// the same below-horizon environment fallback. The display transform is the
// separate tonemap pass.
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
};

Texture2D<float4> baseColorTexture : register(t0);
Texture2D<float4> normalTexture : register(t1);
Texture2D<float4> roughnessTexture : register(t2);
Texture2D<float4> metalnessTexture : register(t3);
Texture2D<float4> aoTexture : register(t4);
Texture2D<float4> emissiveTexture : register(t5);
Texture2D<float> shadowTexture : register(t6);
SamplerState linearSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 worldPos : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 tangent : TEXCOORD2;
    float2 uv : TEXCOORD3;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    float4 worldPosition = mul(float4(input.position, 1.0), world);
    output.position = mul(worldPosition, viewProj);
    output.worldPos = worldPosition.xyz;
    output.normal = normalize(mul(float4(input.normal, 0.0), normalMatrix).xyz);
    output.tangent = float4(
        normalize(mul(float4(input.tangent.xyz, 0.0), normalMatrix).xyz),
        input.tangent.w);
    output.uv = input.uv;
    return output;
}

struct ShadowOutput
{
    float4 position : SV_Position;
};

ShadowOutput VSShadow(VSInput input)
{
    ShadowOutput output;
    float4 worldPosition = mul(float4(input.position, 1.0), world);
    output.position = mul(worldPosition, lightViewProj);
    return output;
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
        emissive = emissiveTexture.Sample(linearSampler, input.uv).rgb;
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

    if ((flags & 16u) != 0u)
    {
        // Bounded glass approximation, not a refraction claim: a tinted
        // environment transmission plus a weak direct lobe.
        float3 transmitted = baseColor * Environment(-V, roughness);
        direct *= 0.15;
        ambient = transmitted * 0.8 + ambient * 0.2;
    }

    float3 color = direct + ambient + emissive;
    if (fogColor.w > 0.5)
    {
        float distanceToCamera = length(cameraPos.xyz - input.worldPos);
        float opticalDepth =
            max(fog.x, 0.0) * distanceToCamera *
            exp(-max(fog.y, 0.0) * max(fog.z - fog.w, 0.0));
        float transmittance = exp(-opticalDepth);
        color = color * transmittance + fogColor.rgb * (1.0 - transmittance);
    }
    return float4(color, 1.0);
}
