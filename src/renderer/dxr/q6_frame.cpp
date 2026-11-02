#include "dxr/q6_frame.h"

#include <cmath>
#include <sstream>
#include <unordered_map>

namespace doom3 {
namespace dxr {
namespace {

uint64_t Fnv1a(uint64_t hash, const void* data, size_t bytes) {
    const auto* values = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < bytes; ++i) {
        hash ^= values[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

uint64_t Fnv1aU32(uint64_t hash, uint32_t value) { return Fnv1a(hash, &value, sizeof(value)); }

uint64_t Fnv1aDouble(uint64_t hash, double value) {
    // Hash the bit pattern so two runs compare the exact same camera, not a
    // rounded decimal print.
    return Fnv1a(hash, &value, sizeof(value));
}

bool Finite(const rhi::MotionVec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

}  // namespace

rhi::MotionValidity ReasonForEvent(Q6FrameEvent event) {
    switch (event) {
        case Q6FrameEvent::Cut:
        case Q6FrameEvent::Teleport:
        case Q6FrameEvent::LevelLoad:
            return rhi::MotionValidity::Reset;
        case Q6FrameEvent::ProviderSwitch:
            return rhi::MotionValidity::ProviderSwitch;
        case Q6FrameEvent::None:
            return rhi::MotionValidity::NoHistory;
    }
    return rhi::MotionValidity::NoHistory;
}

const char* PtSceneName(const std::string& identity) {
    if (identity == "PT-S1") return "static-convergence";
    if (identity == "PT-S2") return "alpha-emissive";
    if (identity == "PT-S3") return "motion";
    if (identity == "PT-S4") return "transmission";
    if (identity == "PT-S5") return "large-environment";
    return "unknown";
}

std::string Q6FrameIdentity::Describe() const {
    std::ostringstream out;
    out << scene << " (" << sceneName << ")"
        << " view=" << viewId << " frame=" << frameIndex << " render=" << renderWidth << "x"
        << renderHeight << " display=" << displayWidth << "x" << displayHeight
        << " eye=(" << camera.eye.x << "," << camera.eye.y << "," << camera.eye.z << ")"
        << " poseGen=" << poseGeneration << " motionGen=" << motionGeneration
        << " reset=" << rhi::ToString(resetReason) << " history=" << (historyValid ? 1 : 0)
        << " draws=" << drawOrder.size() << " hash=0x" << std::hex << hash << std::dec;
    return out.str();
}

bool Q6EngineFrame::Refuse(const std::string& reason) {
    if (!error_.empty()) {
        error_ += "; ";
    }
    error_ += reason;
    return false;
}

bool Q6EngineFrame::Consume(const rhi::SceneCommandQueue& queue,
                            rhi::SceneGeometrySource* geometry,
                            const std::vector<Q6SurfaceAssignment>& assignments,
                            std::string* error) {
    surfaces_.clear();
    lights_.clear();
    stats_ = Q6SeamStats{};
    error_.clear();
    consumed_ = false;

    if (geometry == nullptr) {
        Refuse("the Q2 stream needs a geometry source");
        if (error != nullptr) *error = error_;
        return false;
    }
    std::unordered_map<uint32_t, Q6SurfaceAssignment> bySurface;
    for (const Q6SurfaceAssignment& assignment : assignments) {
        bySurface[assignment.surfaceId] = assignment;
    }

    bool sceneStarted = false;
    uint32_t order = 0;
    for (const rhi::SceneCommand& command : queue.commands()) {
        switch (command.kind) {
            case rhi::SceneCommandKind::BeginView: {
                if (command.view.overlay) {
                    ++stats_.overlays;
                    // O-11: a display overlay is composed after the scene. A
                    // stream that draws it before any scene view is refused,
                    // not reordered.
                    if (!sceneStarted) {
                        Refuse("a display overlay was declared before the scene view");
                        if (error != nullptr) *error = error_;
                        return false;
                    }
                } else {
                    ++stats_.views;
                    sceneStarted = true;
                }
                break;
            }
            case rhi::SceneCommandKind::DrawPacket: {
                ++stats_.draws;
                if (!sceneStarted) {
                    Refuse("a draw packet appeared before the scene view");
                    if (error != nullptr) *error = error_;
                    return false;
                }
                const auto found = bySurface.find(command.draw.surfaceId);
                if (found == bySurface.end()) {
                    ++stats_.drawsRefused;
                    Refuse("surface " + std::to_string(command.draw.surfaceId) +
                           " has no content assignment");
                    continue;
                }
                rhi::SceneSurfaceGeometry resolved;
                if (!geometry->ResolveSurface(command.draw.surfaceId, &resolved) ||
                    resolved.vertices.empty()) {
                    ++stats_.drawsRefused;
                    Refuse("surface " + std::to_string(command.draw.surfaceId) +
                           " has no resolvable geometry");
                    continue;
                }
                Q6OrderedSurface surface;
                surface.order = order++;
                surface.surfaceId = command.draw.surfaceId;
                surface.materialId = command.draw.materialId;
                surface.cell = found->second.cell;
                surface.alphaTested = found->second.alphaTested || command.draw.alphaTested;
                surface.translucent = command.draw.translucent;
                surface.vertices = std::move(resolved.vertices);
                for (int i = 0; i < 16; ++i) {
                    surface.transform[i] = resolved.transform[i];
                }
                surfaces_.push_back(std::move(surface));
                ++stats_.drawsResolved;
                break;
            }
            case rhi::SceneCommandKind::LightPacket: {
                Q6OrderedLight light;
                light.order = order++;
                light.lightId = command.light.lightId;
                light.pointLight = command.light.light.pointLight;
                light.noShadows = command.light.light.noShadows;
                light.falloffRadius = command.light.light.falloffRadius;
                lights_.push_back(light);
                ++stats_.lights;
                break;
            }
            default:
                break;
        }
    }
    if (!sceneStarted) {
        Refuse("the Q2 stream contained no scene view");
        if (error != nullptr) *error = error_;
        return false;
    }
    if (stats_.drawsResolved == 0) {
        Refuse("the Q2 stream resolved no draw");
        if (error != nullptr) *error = error_;
        return false;
    }
    stats_.orderValid = error_.empty();
    consumed_ = stats_.orderValid;
    if (error != nullptr) *error = error_;
    return stats_.orderValid;
}

uint64_t Q6EngineFrame::ComputeIdentityHash(const rhi::CameraState& camera,
                                                  uint32_t frameIndex) const {
    uint64_t hash = 1469598103934665603ull;
    hash = Fnv1a(hash, identity_.scene.data(), identity_.scene.size());
    hash = Fnv1aU32(hash, frameIndex);
    hash = Fnv1aU32(hash, camera.width);
    hash = Fnv1aU32(hash, camera.height);
    hash = Fnv1aDouble(hash, camera.eye.x);
    hash = Fnv1aDouble(hash, camera.eye.y);
    hash = Fnv1aDouble(hash, camera.eye.z);
    hash = Fnv1aDouble(hash, camera.forward.x);
    hash = Fnv1aDouble(hash, camera.forward.y);
    hash = Fnv1aDouble(hash, camera.forward.z);
    hash = Fnv1aDouble(hash, camera.left.x);
    hash = Fnv1aDouble(hash, camera.left.y);
    hash = Fnv1aDouble(hash, camera.left.z);
    hash = Fnv1aDouble(hash, camera.up.x);
    hash = Fnv1aDouble(hash, camera.up.y);
    hash = Fnv1aDouble(hash, camera.up.z);
    hash = Fnv1aDouble(hash, camera.tanX);
    hash = Fnv1aDouble(hash, camera.tanY);
    for (const Q6OrderedSurface& surface : surfaces_) {
        hash = Fnv1aU32(hash, surface.surfaceId);
        hash = Fnv1aU32(hash, surface.materialId);
        hash = Fnv1aU32(hash, static_cast<uint32_t>(surface.cell));
    }
    for (const Q6OrderedLight& light : lights_) {
        hash = Fnv1aU32(hash, light.lightId);
        hash = Fnv1aU32(hash, light.pointLight ? 1u : 0u);
    }
    return hash;
}

bool Q6EngineFrame::BeginFrame(rhi::FrameIntegration* integration,
                               const rhi::CameraState& camera, uint32_t viewId,
                               uint32_t frameIndex, const rhi::ViewIdentity& identity,
                               const std::vector<rhi::RenderRecord>& records,
                               const std::vector<rhi::MotionVec3>& currentPose,
                               uint32_t poseIdentity, uint32_t poseGeneration,
                               Q6FrameEvent event, bool historyValid, std::string* error) {
    if (!consumed_) {
        if (error != nullptr) *error = "Consume must succeed before BeginFrame";
        return false;
    }
    if (integration == nullptr) {
        if (error != nullptr) *error = "BeginFrame needs the Q3 frame integration";
        return false;
    }
    rhi::RenderedViewCommit commit;
    commit.kind = rhi::ViewKind::Main;
    commit.identity = identity;
    commit.camera = camera;
    commit.records = records;
    commit.poseIdentity = poseIdentity;
    commit.poseGeneration = poseGeneration;
    commit.posedPositions = currentPose;
    if (!integration->CommitView(commit)) {
        if (error != nullptr) *error = "the Q3 integration refused the rendered view";
        return false;
    }

    if (identity_.scene.empty()) {
        identity_.scene = "PT-S1";
    }
    identity_.sceneName = PtSceneName(identity_.scene);
    identity_.viewId = viewId;
    identity_.frameIndex = frameIndex;
    identity_.renderWidth = camera.width;
    identity_.renderHeight = camera.height;
    identity_.displayWidth = integration->desc().display.width;
    identity_.displayHeight = integration->desc().display.height;
    identity_.camera = camera;
    identity_.poseIdentity = poseIdentity;
    identity_.poseGeneration = poseGeneration;
    identity_.motionGeneration = static_cast<uint32_t>(integration->motion().generation());
    identity_.resetReason = ReasonForEvent(event);
    identity_.historyValid = historyValid;
    identity_.drawOrder.clear();
    identity_.cells.clear();
    for (const Q6OrderedSurface& surface : surfaces_) {
        identity_.drawOrder.push_back(surface.surfaceId);
        identity_.cells.push_back(surface.cell);
    }

    uint64_t hash = 1469598103934665603ull;
    hash = Fnv1a(hash, identity_.scene.data(), identity_.scene.size());
    hash = Fnv1aU32(hash, viewId);
    hash = Fnv1aU32(hash, frameIndex);
    hash = Fnv1aU32(hash, camera.width);
    hash = Fnv1aU32(hash, camera.height);
    hash = Fnv1aDouble(hash, camera.eye.x);
    hash = Fnv1aDouble(hash, camera.eye.y);
    hash = Fnv1aDouble(hash, camera.eye.z);
    hash = Fnv1aDouble(hash, camera.forward.x);
    hash = Fnv1aDouble(hash, camera.forward.y);
    hash = Fnv1aDouble(hash, camera.forward.z);
    hash = Fnv1aDouble(hash, camera.left.x);
    hash = Fnv1aDouble(hash, camera.left.y);
    hash = Fnv1aDouble(hash, camera.left.z);
    hash = Fnv1aDouble(hash, camera.up.x);
    hash = Fnv1aDouble(hash, camera.up.y);
    hash = Fnv1aDouble(hash, camera.up.z);
    hash = Fnv1aDouble(hash, camera.tanX);
    hash = Fnv1aDouble(hash, camera.tanY);
    hash = Fnv1aU32(hash, poseIdentity);
    hash = Fnv1aU32(hash, poseGeneration);
    hash = Fnv1aU32(hash, static_cast<uint32_t>(event));
    hash = Fnv1aU32(hash, historyValid ? 1u : 0u);
    for (const Q6OrderedSurface& surface : surfaces_) {
        hash = Fnv1aU32(hash, surface.surfaceId);
        hash = Fnv1aU32(hash, surface.materialId);
        hash = Fnv1aU32(hash, static_cast<uint32_t>(surface.cell));
    }
    for (const Q6OrderedLight& light : lights_) {
        hash = Fnv1aU32(hash, light.lightId);
        hash = Fnv1aU32(hash, light.pointLight ? 1u : 0u);
    }
    identity_.hash = hash;
    if (error != nullptr) error->clear();
    return true;
}

bool Q6EngineFrame::BuildMotion(rhi::FrameIntegration* integration,
                                const rhi::CameraState& previousCamera,
                                const Q6SurfacePose* surfaces, uint32_t surfaceCount,
                                const Q6HitReference* hits, uint32_t width, uint32_t height,
                                Q6MotionStats* stats, std::string* error) {
    if (integration == nullptr) {
        if (error != nullptr) *error = "BuildMotion needs the Q3 frame integration";
        return false;
    }
    if (width == 0 || height == 0 || hits == nullptr) {
        if (error != nullptr) *error = "BuildMotion needs a non-zero extent and a hit buffer";
        return false;
    }
    if (integration->motion().width() != width || integration->motion().height() != height) {
        if (error != nullptr) *error = "the Q3 motion buffer extent does not match the frame";
        return false;
    }
    Q6MotionStats local;
    local.pixels = width * height;
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const Q6HitReference hit = hits[static_cast<size_t>(y) * width + x];
            rhi::MotionSample sample;
            if (hit.surface >= surfaceCount) {
                sample.validity = rhi::MotionValidity::NoHistory;
            } else {
                const Q6SurfacePose& surface = surfaces[hit.surface];
                const size_t base = static_cast<size_t>(hit.primitive) * 3u;
                if (base + 2 >= surface.current.size() ||
                    surface.current.size() != surface.previous.size()) {
                    sample.validity = rhi::MotionValidity::Topology;
                } else {
                    const double u = hit.baryU;
                    const double v = hit.baryV;
                    const double w = 1.0 - u - v;
                    auto blend = [&](const std::vector<rhi::MotionVec3>& values) {
                        return rhi::MotionVec3{
                            w * values[base + 0].x + u * values[base + 1].x +
                                v * values[base + 2].x,
                            w * values[base + 0].y + u * values[base + 1].y +
                                v * values[base + 2].y,
                            w * values[base + 0].z + u * values[base + 1].z +
                                v * values[base + 2].z};
                    };
                    const rhi::MotionVec3 current = blend(surface.current);
                    const rhi::MotionVec3 previous = blend(surface.previous);
                    if (!Finite(current) || !Finite(previous)) {
                        sample.validity = rhi::MotionValidity::NoHistory;
                    } else {
                        rhi::RenderRecord currentRecord;
                        currentRecord.point = current;
                        currentRecord.hasRenderedRecord = true;
                        rhi::RenderRecord previousRecord;
                        previousRecord.point = previous;
                        previousRecord.hasRenderedRecord = true;
                        sample = rhi::ReprojectPoint(previousCamera, previousRecord,
                                                     identity_.camera, currentRecord);
                    }
                }
            }
            if (!integration->StorePerPixelMotion(x, y, sample)) {
                if (error != nullptr) *error = "the Q3 motion store refused a pixel";
                return false;
            }
            if (sample.validity == rhi::MotionValidity::Valid) {
                ++local.validPixels;
            } else {
                ++local.invalidPixels;
                if (sample.validity == rhi::MotionValidity::Disocclusion) {
                    ++local.disoccludedPixels;
                } else if (sample.validity == rhi::MotionValidity::BehindCamera) {
                    ++local.behindCameraPixels;
                }
            }
        }
    }
    identity_.motionGeneration = static_cast<uint32_t>(integration->motion().generation());
    if (stats != nullptr) *stats = local;
    if (error != nullptr) error->clear();
    return true;
}

}  // namespace dxr
}  // namespace doom3


