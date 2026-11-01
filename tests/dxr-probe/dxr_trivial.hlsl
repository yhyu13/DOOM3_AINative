// Minimal DXR 1.0/1.1 shader library used by q6_dxr_probe.
//
// The probe traces one camera ray per pixel at a single triangle. A hit writes
// green, a miss writes red, so the readback proves DispatchRays executed and
// the acceleration structure was reachable rather than merely allocated.

RaytracingAccelerationStructure gScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

struct Payload {
    float4 color;
};

[shader("raygeneration")]
void MyRayGen() {
    Payload payload;
    payload.color = float4(0.0, 0.0, 0.0, 1.0);

    RayDesc ray;
    ray.Origin = float3(0.0, 0.0, 1.0);
    ray.Direction = float3(0.0, 0.0, -1.0);
    ray.TMin = 0.001;
    ray.TMax = 100.0;

    TraceRay(gScene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    gOutput[DispatchRaysIndex().xy] = payload.color;
}

[shader("miss")]
void MyMiss(inout Payload payload) {
    payload.color = float4(1.0, 0.0, 0.0, 1.0);
}

[shader("closesthit")]
void MyClosestHit(inout Payload payload,
                  in BuiltInTriangleIntersectionAttributes attributes) {
    payload.color = float4(0.0, 1.0, 0.0, 1.0);
}
