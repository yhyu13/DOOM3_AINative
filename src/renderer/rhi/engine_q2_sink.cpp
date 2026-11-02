#include "rhi/engine_q2_sink.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace doom3 {
namespace rhi {

namespace {

DepthHackKind ToDepthHackKind(int value, bool* known) {
    switch (value) {
        case 0:
            *known = true;
            return DepthHackKind::None;
        case 1:
            *known = true;
            return DepthHackKind::Weapon;
        case 2:
            *known = true;
            return DepthHackKind::Model;
        default:
            *known = false;
            return DepthHackKind::None;
    }
}

ColorTargetSelector ToColorTarget(int selector, bool* known) {
    switch (selector) {
        case 0:
            *known = true;
            return ColorTargetSelector::Current;
        case 1:
            *known = true;
            return ColorTargetSelector::Offscreen;
        default:
            *known = false;
            return ColorTargetSelector::Unsupported;
    }
}

LightKind ToLightKind(const Q2SinkLightFacts& facts) {
    if (facts.fogLight) {
        return LightKind::Fog;
    }
    if (facts.blendLight) {
        return LightKind::Blend;
    }
    if (facts.parallel) {
        return LightKind::Projected;
    }
    if (facts.pointLight) {
        return LightKind::Point;
    }
    return LightKind::Ambient;
}

ViewKind ToViewKind(uint32_t value) {
    switch (value) {
        case 1: return ViewKind::Mirror;
        case 2: return ViewKind::Camera;
        case 3: return ViewKind::Shadow;
        default: return ViewKind::Main;
    }
}

}  // namespace

EngineQ2SinkConsumer::EngineQ2SinkConsumer() {
    callbacks_.user = this;
    callbacks_.initialize = &EngineQ2SinkConsumer::InitializeThunk;
    callbacks_.shutdown = &EngineQ2SinkConsumer::ShutdownThunk;
    callbacks_.imageUpload = &EngineQ2SinkConsumer::ImageUploadThunk;
    callbacks_.beginView = &EngineQ2SinkConsumer::BeginViewThunk;
    callbacks_.surfacePacket = &EngineQ2SinkConsumer::SurfacePacketThunk;
    callbacks_.drawPacket = &EngineQ2SinkConsumer::DrawPacketThunk;
    callbacks_.lightPacket = &EngineQ2SinkConsumer::LightPacketThunk;
    callbacks_.privatePose = &EngineQ2SinkConsumer::PrivatePoseThunk;
    callbacks_.endView = &EngineQ2SinkConsumer::EndViewThunk;
    callbacks_.setColorTarget = &EngineQ2SinkConsumer::SetColorTargetThunk;
    callbacks_.copyToImage = &EngineQ2SinkConsumer::CopyToImageThunk;
    callbacks_.swapBuffers = &EngineQ2SinkConsumer::SwapBuffersThunk;
}

EngineQ2SinkConsumer::EngineQ2SinkConsumer(const EngineQ2SinkDesc& desc)
    : EngineQ2SinkConsumer() {
    desc_ = desc;
}

EngineQ2SinkConsumer::~EngineQ2SinkConsumer() = default;

void EngineQ2SinkConsumer::SetDesc(const EngineQ2SinkDesc& desc) {
    desc_ = desc;
}

bool EngineQ2SinkConsumer::Initialize(int width, int height, std::string* error) {
    initialized_ = false;
    lastSwapSucceeded_ = false;
    lastError_.clear();
    lastResult_ = SceneExecuteResult();
    queue_.Clear();
    surfaces_.clear();
    imageUploads_.clear();

    auto fail = [error](const std::string& reason) {
        if (error != nullptr) {
            *error = reason;
        }
        return false;
    };

    if (width <= 0 || height <= 0) {
        return fail("the classic scene consumer needs a non-zero display extent");
    }
    // The consumer itself resolves the surfaces the engine sends. A caller that
    // supplied an external source keeps it.
    if (desc_.geometry == nullptr) {
        desc_.geometry = this;
    }
    if (!desc_.structuralOnly) {
        if (desc_.backend == nullptr || desc_.geometry == nullptr) {
            return fail(
                "no classic frame backend is linked into this process: real "
                "submission needs a geometry source and a backend (spec 004 "
                "T085); refusing instead of drawing an empty frame");
        }
        if (desc_.surfacePipeline.id == 0) {
            return fail(
                "the classic frame backend has no surface pipeline; refusing "
                "instead of drawing an empty frame");
        }
    }

    const uint32_t renderWidth =
        desc_.renderWidth != 0 ? desc_.renderWidth : static_cast<uint32_t>(width);
    const uint32_t renderHeight =
        desc_.renderHeight != 0 ? desc_.renderHeight : static_cast<uint32_t>(height);
    const FrameExtent render{renderWidth, renderHeight};
    const FrameExtent display{static_cast<uint32_t>(width),
                              static_cast<uint32_t>(height)};
    std::string frameError;
    if (frame_.Initialize(FrameIntegrationDesc{render, display}, &frameError) !=
        Status::Ok) {
        return fail(frameError);
    }

    SceneAdapterDesc adapterDesc;
    adapterDesc.displayWidth = display.width;
    adapterDesc.displayHeight = display.height;
    adapterDesc.structuralOnly = desc_.structuralOnly;
    adapterDesc.backend = desc_.backend;
    adapterDesc.geometry = desc_.geometry;
    adapterDesc.surfacePipeline = desc_.surfacePipeline;
    adapterDesc.shadowBackPipeline = desc_.shadowBackPipeline;
    adapterDesc.shadowFrontPipeline = desc_.shadowFrontPipeline;
    adapterDesc.lightShadePipeline = desc_.lightShadePipeline;
    adapterDesc.surfaceAttachmentIndex = desc_.surfaceAttachmentIndex;
    adapterDesc.lightAttachmentIndex = desc_.lightAttachmentIndex;
    std::string adapterError;
    if (adapter_.Initialize(adapterDesc, &adapterError) != Status::Ok) {
        return fail(adapterError);
    }

    initialized_ = true;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

void EngineQ2SinkConsumer::Shutdown() {
    queue_.Clear();
    surfaces_.clear();
    imageUploads_.clear();
    lastResult_ = SceneExecuteResult();
    lastError_.clear();
    initialized_ = false;
    lastSwapSucceeded_ = false;
}

bool EngineQ2SinkConsumer::Refuse(const std::string& reason) {
    ++refusedCallbacks_;
    if (lastError_.empty()) {
        lastError_ = reason;
    }
    return false;
}

bool EngineQ2SinkConsumer::BeginView(unsigned int viewId, int x, int y,
                                     int width, int height, bool overlay,
                                     bool worldGui,
                                     const float projection[16]) {
    (void)x;
    (void)y;
    if (!initialized_) {
        return Refuse("a view arrived before the classic scene consumer initialized");
    }
    if (viewId == 0 || width <= 0 || height <= 0) {
        return Refuse("a view declares an empty identity or extent");
    }
    ViewDesc view;
    view.identity = viewId;
    view.width = static_cast<uint32_t>(width);
    view.height = static_cast<uint32_t>(height);
    view.overlay = overlay;
    view.worldGui = worldGui;
    view.depthHack = DepthHackKind::None;
    if (projection != nullptr) {
        std::memcpy(view.projection, projection, sizeof(view.projection));
    }
    if (!queue_.BeginView(view)) {
        return Refuse("the classic stream could not express a view");
    }
    // A stable per-view light identity lets a geometry source cache volumes
    // across frames without inventing a global id the engine never sent.
    lightSerial_ = 0;
    return true;
}

bool EngineQ2SinkConsumer::DrawPacket(unsigned int surfaceId,
                                      const Q2SinkMaterialFacts& material,
                                      int vertexCount, int indexCount,
                                      int depthHackKind, float modelDepthHack,
                                      bool worldGui, bool postProcess) {
    if (!initialized_) {
        return Refuse("a draw packet arrived before the classic scene consumer initialized");
    }
    bool knownDepthHack = false;
    const DepthHackKind depthHack = ToDepthHackKind(depthHackKind, &knownDepthHack);
    if (!knownDepthHack) {
        return Refuse("the engine sent an unknown depth-hack kind " +
                      std::to_string(depthHackKind));
    }
    if (vertexCount <= 0) {
        return Refuse("the engine sent a draw packet with no vertices");
    }
    DrawSurfPacket packet;
    packet.surfaceId = surfaceId;
    packet.vertexCount = static_cast<uint32_t>(vertexCount);
    packet.indexCount = static_cast<uint32_t>(std::max(indexCount, 0));
    packet.stageCount = static_cast<uint32_t>(std::max(material.numStages, 0));
    packet.alphaTested = material.alphaTest;
    packet.translucent = material.translucent;
    packet.depthHack = depthHack != DepthHackKind::None;
    packet.worldGui = worldGui;
    packet.postProcess = postProcess;
    packet.sortKey =
        static_cast<uint64_t>(static_cast<uint32_t>(material.sort));
    (void)modelDepthHack;  // the adapter applies the hack from the projection
    if (!queue_.Draw(packet)) {
        return Refuse("the classic stream could not express a draw packet");
    }
    return true;
}

bool EngineQ2SinkConsumer::LightPacket(const Q2SinkLightFacts& light) {
    if (!initialized_) {
        return Refuse("a light packet arrived before the classic scene consumer initialized");
    }
    doom3::rhi::LightPacket packet;
    packet.lightId = ++lightSerial_;
    packet.light.kind = ToLightKind(light);
    packet.light.pointLight = light.pointLight;
    packet.light.hasProjection = light.hasProjection;
    packet.light.falloffRadius = light.falloffRadius;
    packet.light.noShadows = light.noShadows;
    packet.light.multiFrustum = light.numShadowFrustums > 1;
    packet.chainKind = light.chainKind;
    // The engine emitted one packet per chain and stated why it is excluded.
    // A no-chain packet for a lit surface must not make the adapter derive the
    // default global+local pair, so it is expressed as the material-noShadows
    // suppression; a noShadows light is expressed as the light suppression.
    packet.material.noShadows =
        light.chainMaterialNoShadows ||
        (light.excluded && light.chainKind == 0 && !light.noShadows);
    packet.material.noSelfShadow = light.chainMaterialNoSelfShadow;
    packet.sameEntity = light.chainSameEntity;
    if (!queue_.DrawLight(packet)) {
        return Refuse("the classic stream could not express a light packet");
    }
    return true;
}

bool EngineQ2SinkConsumer::EndView() {
    if (!initialized_) {
        return Refuse("a view ended before the classic scene consumer initialized");
    }
    if (!queue_.EndView()) {
        return Refuse("the classic stream could not express the end of a view");
    }
    return true;
}

bool EngineQ2SinkConsumer::SetColorTarget(int selector) {
    if (!initialized_) {
        return Refuse("a colour target arrived before the classic scene consumer initialized");
    }
    bool known = false;
    const ColorTargetSelector target = ToColorTarget(selector, &known);
    if (!known) {
        return Refuse("the engine sent an unknown colour target selector " +
                      std::to_string(selector));
    }
    if (!queue_.SetColorTarget(target)) {
        return Refuse("the classic stream could not express a colour target");
    }
    return true;
}

bool EngineQ2SinkConsumer::CopyToImage(unsigned int imageId,
                                       const char* imageName) {
    (void)imageName;
    if (!initialized_) {
        return Refuse("a frame copy arrived before the classic scene consumer initialized");
    }
    if (!queue_.CopyToImage(imageId)) {
        return Refuse("a frame copy carries no destination identity");
    }
    return true;
}

bool EngineQ2SinkConsumer::SwapBuffers() {
    if (!initialized_) {
        return Refuse("a frame end arrived before the classic scene consumer initialized");
    }
    lastSwapSucceeded_ = false;
    lastResult_ = SceneExecuteResult();
    lastError_.clear();
    queue_.SwapBuffers();

    std::string executeError;
    const Status status =
        frame_.ExecuteFrame(&adapter_, &queue_, &lastResult_, &executeError);
    queue_.Clear();
    if (status != Status::Ok) {
        lastError_ = executeError.empty()
                         ? "the Q2 stream could not be executed"
                         : executeError;
        return false;
    }
    if (!desc_.structuralOnly) {
        // A refused draw or light would leave a partial frame. The engine's
        // contract is to refuse the frame, not to compose what remains.
        if (lastResult_.drawSubmissionUnavailable ||
            lastResult_.drawsSubmitted != lastResult_.drawsExtracted ||
            lastResult_.lightsRefused != 0) {
            lastError_ =
                "the classic frame was refused: extracted draws " +
                std::to_string(lastResult_.drawsExtracted) + ", submitted " +
                std::to_string(lastResult_.drawsSubmitted) + ", refused lights " +
                std::to_string(lastResult_.lightsRefused);
            return false;
        }
    }
    lastSwapSucceeded_ = true;
    return true;
}

bool EngineQ2SinkConsumer::SurfacePacket(
    unsigned int surfaceId, const Q2SinkSurfaceGeometry& geometry) {
    if (!initialized_) {
        return Refuse(
            "a world surface arrived before the classic scene consumer initialized");
    }
    if (surfaceId == 0) {
        return Refuse("a world surface has no identity");
    }
    if (geometry.vertices.empty()) {
        return Refuse("a world surface carries no vertices");
    }
    for (uint16_t index : geometry.indices) {
        if (index >= geometry.vertices.size()) {
            return Refuse("a world surface index is outside the vertex array");
        }
    }
    if (!desc_.captureGeometry) {
        return true;
    }
    SceneSurfaceGeometry stored;
    stored.vertices = geometry.vertices;
    stored.indices = geometry.indices;
    std::memcpy(stored.transform, geometry.transform, sizeof(stored.transform));
    surfaces_[surfaceId] = std::move(stored);
    return true;
}

bool EngineQ2SinkConsumer::ImageUpload(const Q2SinkImageUpload& image) {
    if (!initialized_) {
        return Refuse(
            "an image upload arrived before the classic scene consumer initialized");
    }
    if (image.imageId == 0) {
        return Refuse("an image upload has no identity");
    }
    if (image.width <= 0 || image.height <= 0) {
        return Refuse("an image upload has an empty extent");
    }
    if (!image.pixels.empty()) {
        const int bytesPerTexel = (image.format == 0) ? 4 : 1;
        if (image.bytesPerRow != image.width * bytesPerTexel) {
            return Refuse("an image upload has an inconsistent row pitch");
        }
        const size_t expected = static_cast<size_t>(image.bytesPerRow) *
                                static_cast<size_t>(image.height);
        if (image.pixels.size() != expected) {
            return Refuse("an image upload has an inconsistent byte count");
        }
    }
    Q2SinkImageUpload stored = image;
    if (!desc_.captureImageUploads) {
        stored.pixels.clear();
    }
    imageUploads_[image.imageId] = std::move(stored);
    return true;
}

bool EngineQ2SinkConsumer::PrivatePose(const Q2SinkPrivatePose& pose) {
    if (!initialized_) {
        return Refuse(
            "a private pose arrived before the classic scene consumer initialized");
    }
    if (pose.surfaceId == 0 || pose.generation == 0 || pose.positions.empty()) {
        return Refuse("a private pose has no identity, generation or positions");
    }
    return frame_.CommitPrivatePose(ToViewKind(pose.viewKind), pose.surfaceId,
                                    pose.generation, pose.positions);
}

bool EngineQ2SinkConsumer::ResolveSurface(uint32_t surfaceId,
                                          SceneSurfaceGeometry* geometry) {
    if (geometry == nullptr) {
        return false;
    }
    const auto found = surfaces_.find(surfaceId);
    if (found == surfaces_.end()) {
        return false;
    }
    *geometry = found->second;
    return true;
}

bool EngineQ2SinkConsumer::ResolveLight(
    const doom3::rhi::LightPacket& packet, SceneLightGeometry* geometry) {
    if (desc_.lightGeometrySource != nullptr &&
        desc_.lightGeometrySource != this) {
        return desc_.lightGeometrySource->ResolveLight(packet, geometry);
    }
    // The value-only engine seam does not carry shadow-volume/shade geometry
    // yet. Refuse the packet instead of inventing a volume.
    return false;
}

bool EngineQ2SinkConsumer::CommitRenderedView(const RenderedViewCommit& commit) {
    return initialized_ && frame_.CommitView(commit);
}

bool EngineQ2SinkConsumer::CommitPrivatePose(
    ViewKind kind, uint32_t surfaceId, uint32_t generation,
    const std::vector<MotionVec3>& posedPositions) {
    return initialized_ &&
           frame_.CommitPrivatePose(kind, surfaceId, generation, posedPositions);
}

MotionValidity EngineQ2SinkConsumer::PreviousPrivatePose(
    ViewKind kind, uint32_t surfaceId, uint32_t generation,
    std::vector<MotionVec3>* positions) const {
    if (!initialized_) {
        return MotionValidity::NoHistory;
    }
    return frame_.PreviousPrivatePose(kind, surfaceId, generation, positions);
}

bool EngineQ2SinkConsumer::StorePerPixelMotion(uint32_t x, uint32_t y,
                                               const MotionSample& sample) {
    return initialized_ && frame_.StorePerPixelMotion(x, y, sample);
}

Status EngineQ2SinkConsumer::UploadPerPixelMotion(
    Device& device, MotionFrameResource* resource, std::string* error) const {
    if (!initialized_) {
        if (error != nullptr) {
            *error = "the classic scene consumer is not initialized";
        }
        return Status::NotReady;
    }
    return CreateMotionFrameResources(device, frame_.motion(), resource, error);
}

bool EngineQ2SinkConsumer::InitializeThunk(void* user, int width, int height,
                                           char* reason, int reasonSize) {
    auto* consumer = static_cast<EngineQ2SinkConsumer*>(user);
    if (consumer == nullptr) {
        if (reason != nullptr && reasonSize > 0) {
            std::snprintf(reason, static_cast<size_t>(reasonSize),
                          "the classic scene consumer has no instance");
        }
        return false;
    }
    std::string error;
    if (!consumer->Initialize(width, height, &error)) {
        if (reason != nullptr && reasonSize > 0) {
            std::snprintf(reason, static_cast<size_t>(reasonSize), "%s",
                          error.c_str());
        }
        return false;
    }
    return true;
}

void EngineQ2SinkConsumer::ShutdownThunk(void* user) {
    if (auto* consumer = static_cast<EngineQ2SinkConsumer*>(user)) {
        consumer->Shutdown();
    }
}

bool EngineQ2SinkConsumer::ImageUploadThunk(void* user,
                                            const Q2SinkImageUpload* image) {
    auto* consumer = static_cast<EngineQ2SinkConsumer*>(user);
    if (consumer == nullptr || image == nullptr) {
        return false;
    }
    return consumer->ImageUpload(*image);
}

bool EngineQ2SinkConsumer::BeginViewThunk(void* user, unsigned int viewId,
                                          int x, int y, int width, int height,
                                          bool overlay, bool worldGui,
                                          const float projection[16]) {
    auto* consumer = static_cast<EngineQ2SinkConsumer*>(user);
    return consumer != nullptr &&
           consumer->BeginView(viewId, x, y, width, height, overlay, worldGui,
                               projection);
}

bool EngineQ2SinkConsumer::SurfacePacketThunk(
    void* user, unsigned int surfaceId, const Q2SinkSurfaceGeometry* geometry) {
    auto* consumer = static_cast<EngineQ2SinkConsumer*>(user);
    if (consumer == nullptr || geometry == nullptr) {
        return false;
    }
    return consumer->SurfacePacket(surfaceId, *geometry);
}

bool EngineQ2SinkConsumer::DrawPacketThunk(
    void* user, unsigned int surfaceId, const Q2SinkMaterialFacts* material,
    int vertexCount, int indexCount, int depthHackKind, float modelDepthHack,
    bool worldGui, bool postProcess) {
    auto* consumer = static_cast<EngineQ2SinkConsumer*>(user);
    if (consumer == nullptr || material == nullptr) {
        return false;
    }
    return consumer->DrawPacket(surfaceId, *material, vertexCount, indexCount,
                                depthHackKind, modelDepthHack, worldGui,
                                postProcess);
}

bool EngineQ2SinkConsumer::LightPacketThunk(void* user,
                                            const Q2SinkLightFacts* light) {
    auto* consumer = static_cast<EngineQ2SinkConsumer*>(user);
    if (consumer == nullptr || light == nullptr) {
        return false;
    }
    return consumer->LightPacket(*light);
}

bool EngineQ2SinkConsumer::PrivatePoseThunk(void* user,
                                            const Q2SinkPrivatePose* pose) {
    auto* consumer = static_cast<EngineQ2SinkConsumer*>(user);
    if (consumer == nullptr || pose == nullptr) {
        return false;
    }
    return consumer->PrivatePose(*pose);
}

bool EngineQ2SinkConsumer::EndViewThunk(void* user) {
    auto* consumer = static_cast<EngineQ2SinkConsumer*>(user);
    return consumer != nullptr && consumer->EndView();
}

bool EngineQ2SinkConsumer::SetColorTargetThunk(void* user, int selector) {
    auto* consumer = static_cast<EngineQ2SinkConsumer*>(user);
    return consumer != nullptr && consumer->SetColorTarget(selector);
}

bool EngineQ2SinkConsumer::CopyToImageThunk(void* user, unsigned int imageId,
                                            const char* imageName) {
    auto* consumer = static_cast<EngineQ2SinkConsumer*>(user);
    return consumer != nullptr && consumer->CopyToImage(imageId, imageName);
}

bool EngineQ2SinkConsumer::SwapBuffersThunk(void* user) {
    auto* consumer = static_cast<EngineQ2SinkConsumer*>(user);
    return consumer != nullptr && consumer->SwapBuffers();
}

}  // namespace rhi
}  // namespace doom3
