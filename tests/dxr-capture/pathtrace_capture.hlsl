// Q6 DXR capture shader (spec 004 T069, issue #68).
//
// Iterative camera path inside the ray-generation shader, one DispatchRays
// call per group of samples so a long accumulation cannot hit a GPU watchdog.
// The transport mirrors the documented Q6 CPU slice (pathtrace_material.h):
// cosine-weighted Lambertian, a bounded two-lobe conductor, Schlick/Snell
// dielectric transport with a transmission tint, emission, next-event
// estimation against one point light, and a two-hemisphere environment split at
// world z = 0. Alpha testing uses the same deterministic 2x2 checker mask as
// the CPU fixture, evaluated in an any-hit shader.
//
// The RNG is PCG32 seeded from the pixel and the absolute sample index. It is
// not the CPU slice's generator; the two are compared through converged means
// and analytic expectations, not through identical sample sequences.

struct Material {
    float4 albedoRoughness;  // rgb albedo, roughness
    float4 tintIor;          // rgb transmission tint, ior
    float4 emissionKind;     // rgb emission, kind (0 diffuse, 1 conductor, 2 dielectric, 3 emissive)
    float4 alphaUv;          // alphaTest, alphaThreshold, uvMode, unused
    float4 uvOriginScale;    // originX, originZ, scaleX, scaleZ
};

struct InstanceInfo {
    uint materialIndex;
    uint positionBase;
    uint pad0;
    uint pad1;
};

RaytracingAccelerationStructure gScene : register(t0);
StructuredBuffer<Material> gMaterials : register(t1);
StructuredBuffer<InstanceInfo> gInstances : register(t2);
StructuredBuffer<float3> gPositions : register(t3);
RWTexture2D<float4> gOutput : register(u0);
RWTexture2D<float> gDepth : register(u1);

cbuffer Params : register(b0) {
    float4 camPos;
    float4 camFwd;
    float4 camRight;
    float4 camUp;
    float4 extent;      // tanHalfFovX, tanHalfFovY, width, height
    float4 envUpper;    // rgb, w = enabled
    float4 envLower;    // rgb
    float4 lightPos;    // xyz, w = enabled
    float4 lightColor;  // rgb radiance
    uint4 counts;       // sampleIndex, maxDepth, width, height
    uint4 seed;         // base, frame, unused, unused
    uint4 misc;         // samplesThisDispatch, unused, unused, unused
};

static const float kPi = 3.14159265358979323846f;
static const float kEpsilon = 1.0e-3f;

struct Payload {
    float3 position;
    float t;
    float3 normal;
    uint material;
    float2 uv;
    uint instance;
    uint primitive;
    uint hit;
    float pad0;
    float pad1;
    float pad2;
};

void InitPayload(inout Payload p) {
    p.position = float3(0.0f, 0.0f, 0.0f);
    p.t = 0.0f;
    p.normal = float3(0.0f, 0.0f, 1.0f);
    p.material = 0;
    p.uv = float2(0.0f, 0.0f);
    p.instance = 0;
    p.primitive = 0;
    p.hit = 0;
    p.pad0 = 0.0f;
    p.pad1 = 0.0f;
    p.pad2 = 0.0f;
}

uint PcgHash(uint v) {
    const uint state = v * 747796405u + 2891336453u;
    const uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float Rand(inout uint state) {
    state = PcgHash(state + 0x9E3779B9u);
    return float(state >> 8u) * (1.0f / 16777216.0f);
}

float3 EnvironmentColor(float3 direction) {
    if (envUpper.w < 0.5f) {
        return float3(0.0f, 0.0f, 0.0f);
    }
    return direction.z >= 0.0f ? envUpper.rgb : envLower.rgb;
}

float LuminanceOf(float3 value) {
    return 0.2126f * value.r + 0.7152f * value.g + 0.0722f * value.b;
}

float3 SampleCosineHemisphere(float3 normal, float u1, float u2) {
    const float radius = sqrt(clamp(u1, 0.0f, 1.0f));
    const float phi = 2.0f * kPi * clamp(u2, 0.0f, 1.0f);
    const float x = radius * cos(phi);
    const float y = radius * sin(phi);
    const float z = sqrt(max(0.0f, 1.0f - radius * radius));
    const float3 up =
        abs(normal.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    const float3 tangent = normalize(cross(up, normal));
    const float3 bitangent = cross(normal, tangent);
    return normalize(tangent * x + bitangent * y + normal * z);
}

float FresnelSchlick(float f0, float cosine) {
    const float c = clamp(cosine, 0.0f, 1.0f);
    const float m = 1.0f - c;
    return f0 + (1.0f - f0) * m * m * m * m * m;
}

bool RefractDirection(float3 incidence, float3 normal, float eta, out float3 direction) {
    const float cosI = -dot(incidence, normal);
    if (!(cosI > 0.0f)) {
        direction = incidence;
        return false;
    }
    const float sinT2 = eta * eta * max(0.0f, 1.0f - cosI * cosI);
    if (sinT2 >= 1.0f) {
        direction = incidence;
        return false;
    }
    const float cosT = sqrt(1.0f - sinT2);
    direction = normalize(incidence * eta + normal * (eta * cosI - cosT));
    return true;
}

float2 ComputeUv(Material m, float3 local) {
    if (m.alphaUv.z < 0.5f || m.alphaUv.z > 1.5f) {
        return float2(0.0f, 0.0f);
    }
    return float2((local.x - m.uvOriginScale.x) * m.uvOriginScale.z,
                  (local.z - m.uvOriginScale.y) * m.uvOriginScale.w);
}

[shader("raygeneration")]
void RayGen() {
    const uint2 pixel = DispatchRaysIndex().xy;
    const uint pixelId = pixel.y * counts.z + pixel.x;
    const uint samplesThisDispatch = max(misc.x, 1u);

    float3 sum = float3(0.0f, 0.0f, 0.0f);
    float firstDepth = -1.0f;

    for (uint sample = 0; sample < samplesThisDispatch; ++sample) {
        const uint sampleIndex = counts.x + sample;
        uint rngState =
            PcgHash(pixelId * 0x9E3779B9u + sampleIndex * 0x85EBCA6Bu + seed.x) | 1u;
        const float jitterX = Rand(rngState);
        const float jitterY = Rand(rngState);

        const float u = (float(pixel.x) + jitterX) / extent.z;
        const float v = (float(pixel.y) + jitterY) / extent.w;
        const float ndcX = 2.0f * u - 1.0f;
        const float ndcY = 1.0f - 2.0f * v;
        const float3 direction = normalize(camFwd.xyz + camRight.xyz * ndcX * extent.x +
                                           camUp.xyz * ndcY * extent.y);

        float3 origin = camPos.xyz;
        float3 dir = direction;
        float3 radiance = float3(0.0f, 0.0f, 0.0f);
        float3 throughput = float3(1.0f, 1.0f, 1.0f);
        float pathFirstDepth = -1.0f;

        for (uint depth = 0; depth < counts.y; ++depth) {
            RayDesc ray;
            ray.Origin = origin;
            ray.Direction = dir;
            ray.TMin = 1.0e-4f;
            ray.TMax = 1.0e30f;
            Payload payload;
            InitPayload(payload);
            TraceRay(gScene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);

            if (payload.hit == 0u) {
                radiance += throughput * EnvironmentColor(dir);
                break;
            }
            if (depth == 0) {
                pathFirstDepth = payload.t;
            }

            const Material material = gMaterials[payload.material];
            const int kind = int(material.emissionKind.w + 0.5f);
            const float3 emission = material.emissionKind.rgb;
            if (max(emission.r, max(emission.g, emission.b)) > 0.0f) {
                radiance += throughput * emission;
                if (kind == 3) {
                    break;
                }
            } else if (kind == 3) {
                break;
            }

            const bool entering = dot(payload.normal, dir) < 0.0f;
            const float3 facing = entering ? payload.normal : -payload.normal;
            const float3 albedo = material.albedoRoughness.rgb;

            // Next-event estimation, diffuse surfaces only, one point light.
            if (kind == 0 && lightPos.w > 0.5f) {
                const float3 offset = lightPos.xyz - payload.position;
                const float distanceSquared = dot(offset, offset);
                if (distanceSquared > kEpsilon * kEpsilon) {
                    const float distance = sqrt(distanceSquared);
                    const float3 toLight = offset / distance;
                    const float cosine = dot(facing, toLight);
                    if (cosine > 0.0f) {
                        Payload shadow;
                        InitPayload(shadow);
                        RayDesc shadowRay;
                        shadowRay.Origin = payload.position + facing * kEpsilon;
                        shadowRay.Direction = toLight;
                        shadowRay.TMin = 1.0e-4f;
                        shadowRay.TMax = distance - kEpsilon;
                        TraceRay(gScene,
                                 RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
                                     RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
                                 0xFF, 0, 1, 0, shadowRay, shadow);
                        if (shadow.hit == 0u) {
                            radiance += throughput * albedo * (cosine / kPi) *
                                        lightColor.rgb / distanceSquared;
                        }
                    }
                }
            }

            float3 nextDirection = dir;
            float3 weight = float3(1.0f, 1.0f, 1.0f);
            float3 nextOrigin = payload.position + facing * kEpsilon;
            bool scattered = true;

            if (kind == 0) {
                nextDirection = SampleCosineHemisphere(facing, Rand(rngState), Rand(rngState));
                weight = albedo;
            } else if (kind == 1) {
                const float roughness = clamp(material.albedoRoughness.w, 0.0f, 1.0f);
                const float3 mirror = dir - 2.0f * dot(dir, facing) * facing;
                if (roughness > 0.0f && Rand(rngState) < roughness) {
                    nextDirection = SampleCosineHemisphere(facing, Rand(rngState), Rand(rngState));
                } else if (dot(mirror, facing) > 0.0f) {
                    nextDirection = normalize(mirror);
                } else {
                    scattered = false;
                }
                weight = albedo;
            } else if (kind == 2) {
                const float cosine = max(0.0f, -dot(dir, facing));
                const float fresnel = FresnelSchlick(0.04f, cosine);
                if (Rand(rngState) < fresnel) {
                    nextDirection = normalize(dir - 2.0f * dot(dir, facing) * facing);
                    weight = float3(1.0f, 1.0f, 1.0f);
                } else {
                    const float ior = material.tintIor.w > 0.0f ? material.tintIor.w : 1.5f;
                    const float eta = entering ? 1.0f / ior : ior;
                    float3 refracted;
                    if (!RefractDirection(dir, facing, eta, refracted)) {
                        nextDirection = normalize(dir - 2.0f * dot(dir, facing) * facing);
                        weight = float3(1.0f, 1.0f, 1.0f);
                    } else {
                        nextDirection = refracted;
                        weight = material.tintIor.rgb;
                        nextOrigin = payload.position - facing * kEpsilon;
                    }
                }
            } else {
                scattered = false;
            }

            if (!scattered) {
                break;
            }
            throughput *= weight;
            if (LuminanceOf(throughput) <= 0.0f) {
                break;
            }
            origin = nextOrigin;
            dir = nextDirection;
        }

        sum += radiance;
        if (sample == 0) {
            firstDepth = pathFirstDepth;
        }
    }

    // Progressive mean. counts.x is the number of samples already in the
    // target; one dispatch adds misc.x independent samples per pixel.
    const float previous = float(counts.x);
    const float3 accumulated = gOutput[pixel].rgb;
    gOutput[pixel] =
        float4((accumulated * previous + sum) / (previous + float(samplesThisDispatch)), 1.0f);
    gDepth[pixel] = firstDepth;
}

[shader("miss")]
void Miss(inout Payload payload) {
    payload.hit = 0;
}

[shader("anyhit")]
void AnyHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attributes) {
    const InstanceInfo instance = gInstances[InstanceID()];
    const Material material = gMaterials[instance.materialIndex];
    if (material.alphaUv.x > 0.5f) {
        const float3 local = ObjectRayOrigin() + ObjectRayDirection() * RayTCurrent();
        const float2 uv = ComputeUv(material, local);
        const float u = uv.x < 0.0f ? 0.0f : (uv.x >= 1.0f ? 0.999999f : uv.x);
        const float v = uv.y < 0.0f ? 0.0f : (uv.y >= 1.0f ? 0.999999f : uv.y);
        const int cellU = int(u * 2.0f);
        const int cellV = int(v * 2.0f);
        const float coverage = ((cellU + cellV) & 1) == 0 ? 1.0f : 0.0f;
        if (coverage < material.alphaUv.y) {
            IgnoreHit();
            return;
        }
    }
    payload.hit = 1;
}

[shader("closesthit")]
void ClosestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attributes) {
    const InstanceInfo instance = gInstances[InstanceID()];
    const uint primitive = PrimitiveIndex();
    const uint base = instance.positionBase + primitive * 3u;
    const float3 v0 = gPositions[base + 0u];
    const float3 v1 = gPositions[base + 1u];
    const float3 v2 = gPositions[base + 2u];
    const float3 normal = normalize(cross(v1 - v0, v2 - v0));
    const float3 local = ObjectRayOrigin() + ObjectRayDirection() * RayTCurrent();

    payload.position = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    payload.t = RayTCurrent();
    payload.normal = normal;
    payload.material = instance.materialIndex;
    payload.uv = ComputeUv(gMaterials[instance.materialIndex], local);
    payload.instance = InstanceID();
    payload.primitive = primitive;
    payload.hit = 1;
}
