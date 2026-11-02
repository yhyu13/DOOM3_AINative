#pragma once

// PT-S1..S5 tessellation and Q2 ordered stream for the Q6 DXR capture.
//
// The declared scene (`dxr/q6_pt_scene.h`) is the single source of geometry. The
// capture builds the Q2 `SceneCommandQueue` in declared surface order, resolves
// each draw through `PtGeometrySource`, and lets `Q6EngineFrame` keep the order.
// The device scene is then built from the ordered surfaces, so a change in the
// stream changes the BLAS set, not just a material table.

#include "capture_scene.h"
#include "dxr/q6_frame.h"
#include "dxr/q6_pt_scene.h"

#include <cstdint>
#include <string>
#include <vector>

namespace q6cap {

// World-space triangle list (three Vec3 per triangle) for one surface at
// `frame`. A moving surface translates by `moveDelta * frame`; the sheet is
// posed by the same two-bone model as the synthetic skinned fixture.
void TessellateSurface(const doom3::dxr::Q6SurfaceSpec& surface, uint32_t frame,
                       std::vector<Vec3>* out);

// The Q2 stream in declared surface order, plus one point-light packet.
bool BuildQ2Queue(const doom3::dxr::Q6PtSceneSpec& spec, uint32_t width, uint32_t height,
                  doom3::rhi::SceneCommandQueue* queue, std::string* error);

// Resolves a surface id by tessellating the declared scene at `frame`.
class PtGeometrySource : public doom3::rhi::SceneGeometrySource {
public:
    explicit PtGeometrySource(const doom3::dxr::Q6PtSceneSpec& spec) : spec_(spec) {}
    void SetFrame(uint32_t frame) { frame_ = frame; }
    bool ResolveSurface(uint32_t surfaceId,
                        doom3::rhi::SceneSurfaceGeometry* geometry) override;
    bool ResolveLight(const doom3::rhi::LightPacket&,
                      doom3::rhi::SceneLightGeometry*) override {
        return false;
    }

private:
    const doom3::dxr::Q6PtSceneSpec& spec_;
    uint32_t frame_ = 0;
};

// Converts the ordered Q6 surfaces into the capture scene. Vertices are already
// world-space, so the object transform is identity and `skinned` is false; the
// pose is carried in the vertices, not re-applied.
bool BuildCaptureScene(const doom3::dxr::Q6PtSceneSpec& spec,
                       const doom3::dxr::Q6EngineFrame& frame, uint32_t sceneFrame,
                       uint32_t width, uint32_t height, uint32_t atlasColumns, Scene* scene,
                       std::string* error);

// Per-surface world-space vertices in triangle order, for the Q3 motion build.
// `previousFrame` supplies the previous pose of the same surface order.
void PosesForFrame(const doom3::dxr::Q6PtSceneSpec& spec, uint32_t frame,
                   std::vector<doom3::dxr::Q6SurfacePose>* poses);

}  // namespace q6cap
