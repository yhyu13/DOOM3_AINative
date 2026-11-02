#pragma once

// Consumer for the engine's value-only Q2 scene seam (spec 004 T035/T080/T085).
//
// The reviewed engine fork emits a q2SceneSink_t: ordered beginView, draw and
// light callbacks, then the legacy frame commands. This header is the portable
// half of the consumer. It names no engine type, so it can be compiled and
// exercised without the engine headers:
//
//   * the engine registration unit converts q2MaterialFacts_t / q2LightFacts_t
//     into the value structs below and forwards the calls;
//   * the consumer appends the same order to a SceneCommandQueue and executes
//     it through FrameIntegration, so the Q2 stream reaches the Q3 history,
//     pose and per-pixel motion seam;
//   * initialization fails rather than drawing a silent no-op when the caller
//     asked for real frame submission but supplied no backend or geometry.
//
// The fork call sites that would feed pose data and a live geometry source
// remain a separate engine-fork change (T044 and the fork half of T085); this
// consumer is the superproject side that those call sites target.

#include "rhi/engine_scene_adapter.h"
#include "rhi/frame_integration.h"
#include "rhi/frame_motion_resources.h"
#include "rhi/lighting.h"

#include <cstdint>
#include <string>

namespace doom3 {
namespace rhi {

// Upper bound mirrored from RenderApi.h's Q2_MAX_STAGE_TEXTURES. Keeping the
// number in the portable header avoids including an engine header.
constexpr int kQ2MaxStageTextures = 8;

// Per-stage texture identity and sampler facts (spec 004 T081). `imageId` is the
// engine registry identity; filter/repeat are the engine enums forwarded as ints.
struct Q2SinkTextureStageFacts {
    int numStages = 0;
    uint32_t imageId[kQ2MaxStageTextures] = {};
    int filter[kQ2MaxStageTextures] = {};
    int repeat[kQ2MaxStageTextures] = {};
    bool hasImage[kQ2MaxStageTextures] = {};
};

// Value copy of the engine's q2MaterialFacts_t. Only the fields the classic
// plan consumes are kept; ids are assigned by the caller.
struct Q2SinkMaterialFacts {
    float sort = 0.0f;
    int numStages = 0;
    int coverage = 0;
    bool hasTexture = false;
    bool hasProgram = false;
    bool alphaTest = false;
    bool translucent = false;
    bool fogLight = false;
    bool blendLight = false;
    bool noShadows = false;
    bool noSelfShadow = false;
    bool castsShadow = false;
    Q2SinkTextureStageFacts textures;
};

// Value copy of the engine's q2LightFacts_t. One packet carries one shadow
// chain; the engine emits a second packet for a second chain.
struct Q2SinkLightFacts {
    bool fogLight = false;
    bool blendLight = false;
    bool pointLight = false;
    bool parallel = false;
    bool hasProjection = false;
    bool noShadows = false;
    float falloffRadius = 0.0f;
    int numShadowFrustums = 0;
    int globalShadows = 0;
    int localInteractions = 0;
    int localShadows = 0;
    int globalInteractions = 0;
    int translucentInteractions = 0;
    int chainKind = 0;  // 0 none, 1 global, 2 local
    int chainSurfaceCount = 0;
    bool chainMaterialNoShadows = false;
    bool chainMaterialNoSelfShadow = false;
    bool chainSameEntity = false;
    bool excluded = false;
    int exclusionKind = 0;
};

// One world surface's geometry snapshot (T080). The engine sends pointers with
// an offset/stride layout; the registration unit converts them into this
// portable value type before the consumer sees it.
struct Q2SinkSurfaceGeometry {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    float transform[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                           0.0f, 1.0f, 0.0f, 0.0f,
                           0.0f, 0.0f, 1.0f, 0.0f,
                           0.0f, 0.0f, 0.0f, 1.0f};
};

// One uploaded image (T081). `pixels` is tightly packed RGBA8 (format 0) or R8
// (format 1) and may be empty for a render target.
struct Q2SinkImageUpload {
    uint32_t imageId = 0;
    std::string name;
    int width = 0;
    int height = 0;
    int mipCount = 0;
    int format = 0;
    bool sRGB = false;
    int minFilter = 0;
    int magFilter = 0;
    int wrapS = 0;
    int wrapT = 0;
    std::vector<uint8_t> pixels;
    int bytesPerRow = 0;
};

// One private skinned pose snapshot (T044).
struct Q2SinkPrivatePose {
    uint32_t viewId = 0;
    uint32_t viewKind = 0;
    uint32_t surfaceId = 0;
    uint32_t generation = 0;
    std::vector<MotionVec3> positions;
};

// Mirror of the engine's q2SceneSink_t callback table, with portable value
// types. The engine registration unit keeps the two in lockstep explicitly.
struct Q2SinkCallbacks {
    void* user = nullptr;
    bool (*initialize)(void* user, int width, int height, char* reason,
                       int reasonSize) = nullptr;
    void (*shutdown)(void* user) = nullptr;
    bool (*imageUpload)(void* user, const Q2SinkImageUpload* image) = nullptr;
    bool (*beginView)(void* user, unsigned int viewId, int x, int y, int width,
                      int height, bool overlay, bool worldGui,
                      const float projection[16]) = nullptr;
    bool (*surfacePacket)(void* user, unsigned int surfaceId,
                          const Q2SinkSurfaceGeometry* geometry) = nullptr;
    bool (*drawPacket)(void* user, unsigned int surfaceId,
                       const Q2SinkMaterialFacts* material, int vertexCount,
                       int indexCount, int depthHackKind, float modelDepthHack,
                       bool worldGui, bool postProcess) = nullptr;
    bool (*lightPacket)(void* user, const Q2SinkLightFacts* light) = nullptr;
    bool (*privatePose)(void* user, const Q2SinkPrivatePose* pose) = nullptr;
    bool (*endView)(void* user) = nullptr;
    bool (*setColorTarget)(void* user, int selector) = nullptr;
    bool (*copyToImage)(void* user, unsigned int imageId,
                        const char* imageName) = nullptr;
    bool (*swapBuffers)(void* user) = nullptr;
};

struct EngineQ2SinkDesc {
    // Real submission is the default. A structural-only consumer must be
    // requested explicitly and then reports drawSubmissionUnavailable instead
    // of pretending a frame was drawn.
    bool structuralOnly = false;
    SceneGeometryBackend* backend = nullptr;
    // Null means the consumer itself resolves the surfaces received through
    // surfacePacket. An external source still wins when supplied.
    SceneGeometrySource* geometry = nullptr;
    // Shadow-volume/shade geometry is not part of the value-only engine seam yet.
    // A fixture can supply it; the engine leaves this null and gets a refusal.
    SceneGeometrySource* lightGeometrySource = nullptr;
    // Retaining every surface/image copy is useful for a fixture and wrong for a
    // long-running engine frame loop unless the caller opted in.
    bool captureGeometry = true;
    bool captureImageUploads = true;
    Pipeline surfacePipeline;
    Pipeline shadowBackPipeline;
    Pipeline shadowFrontPipeline;
    Pipeline lightShadePipeline;
    uint32_t surfaceAttachmentIndex = 0;
    uint32_t lightAttachmentIndex = 0;
    // Optional: the scene render extent when it differs from the display
    // extent the engine reports at initialization. Zero means "same".
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
};

// One consumer per process. The engine installs it through R_SetQ2SceneSink;
// a fixture installs the same callback table directly, so the translation and
// execution path under test is the one the engine would use.
//
// It also implements SceneGeometrySource for the surfaces the engine sends:
// ResolveSurface returns the snapshot received through surfacePacket. The engine
// does not yet send shadow-volume geometry, so ResolveLight delegates to an
// optional lightGeometrySource (a fixture supplies one; the engine leaves it
// null and the light packet is refused instead of drawing a guessed volume).
class EngineQ2SinkConsumer : public SceneGeometrySource {
public:
    EngineQ2SinkConsumer();
    explicit EngineQ2SinkConsumer(const EngineQ2SinkDesc& desc);
    ~EngineQ2SinkConsumer();

    EngineQ2SinkConsumer(const EngineQ2SinkConsumer&) = delete;
    EngineQ2SinkConsumer& operator=(const EngineQ2SinkConsumer&) = delete;

    void SetDesc(const EngineQ2SinkDesc& desc);
    const EngineQ2SinkDesc& desc() const { return desc_; }

    bool Initialize(int width, int height, std::string* error);
    void Shutdown();
    bool initialized() const { return initialized_; }

    // Stable pointer to the callback table; `user` is this consumer.
    const Q2SinkCallbacks* callbacks() const { return &callbacks_; }

    bool BeginView(unsigned int viewId, int x, int y, int width, int height,
                   bool overlay, bool worldGui, const float projection[16]);
    bool DrawPacket(unsigned int surfaceId, const Q2SinkMaterialFacts& material,
                    int vertexCount, int indexCount, int depthHackKind,
                    float modelDepthHack, bool worldGui, bool postProcess);
    bool LightPacket(const Q2SinkLightFacts& light);
    bool EndView();
    bool SetColorTarget(int selector);
    bool CopyToImage(unsigned int imageId, const char* imageName);
    bool SwapBuffers();

    // Engine-fork input (T080/T081/T044). Each stores the value snapshot the
    // registration unit converted; none of these is a draw result.
    bool SurfacePacket(unsigned int surfaceId,
                       const Q2SinkSurfaceGeometry& geometry);
    bool ImageUpload(const Q2SinkImageUpload& image);
    bool PrivatePose(const Q2SinkPrivatePose& pose);

    // SceneGeometrySource: the ordered draw stream resolves the surface that the
    // engine sent for the same id.
    bool ResolveSurface(uint32_t surfaceId,
                        SceneSurfaceGeometry* geometry) override;
    // `doom3::rhi::LightPacket` is qualified because this class also has a
    // LightPacket() method that would hide the type name in class scope.
    bool ResolveLight(const doom3::rhi::LightPacket& packet,
                      SceneLightGeometry* geometry) override;

    const std::map<uint32_t, SceneSurfaceGeometry>& surfaces() const {
        return surfaces_;
    }
    const std::map<uint32_t, Q2SinkImageUpload>& imageUploads() const {
        return imageUploads_;
    }

    // Q3 side of the wiring. These delegate to the integration so an engine
    // call site can commit the view it rendered, retain a private pose and
    // store one display-space motion sample.
    bool CommitRenderedView(const RenderedViewCommit& commit);
    bool CommitPrivatePose(ViewKind kind, uint32_t surfaceId, uint32_t generation,
                           const std::vector<MotionVec3>& posedPositions);
    MotionValidity PreviousPrivatePose(ViewKind kind, uint32_t surfaceId,
                                       uint32_t generation,
                                       std::vector<MotionVec3>* positions) const;
    bool StorePerPixelMotion(uint32_t x, uint32_t y, const MotionSample& sample);
    Status UploadPerPixelMotion(Device& device, MotionFrameResource* resource,
                                std::string* error) const;

    FrameIntegration& frame() { return frame_; }
    const FrameIntegration& frame() const { return frame_; }
    SceneAdapter& adapter() { return adapter_; }
    const SceneCommandQueue& queue() const { return queue_; }
    const SceneExecuteResult& lastResult() const { return lastResult_; }
    const std::string& lastError() const { return lastError_; }
    bool lastSwapSucceeded() const { return lastSwapSucceeded_; }
    uint32_t refusedCallbacks() const { return refusedCallbacks_; }

private:
    static bool InitializeThunk(void* user, int width, int height,
                                char* reason, int reasonSize);
    static void ShutdownThunk(void* user);
    static bool ImageUploadThunk(void* user, const Q2SinkImageUpload* image);
    static bool BeginViewThunk(void* user, unsigned int viewId, int x, int y,
                               int width, int height, bool overlay,
                               bool worldGui, const float projection[16]);
    static bool SurfacePacketThunk(void* user, unsigned int surfaceId,
                                   const Q2SinkSurfaceGeometry* geometry);
    static bool DrawPacketThunk(void* user, unsigned int surfaceId,
                                const Q2SinkMaterialFacts* material,
                                int vertexCount, int indexCount,
                                int depthHackKind, float modelDepthHack,
                                bool worldGui, bool postProcess);
    static bool LightPacketThunk(void* user, const Q2SinkLightFacts* light);
    static bool PrivatePoseThunk(void* user, const Q2SinkPrivatePose* pose);
    static bool EndViewThunk(void* user);
    static bool SetColorTargetThunk(void* user, int selector);
    static bool CopyToImageThunk(void* user, unsigned int imageId,
                                 const char* imageName);
    static bool SwapBuffersThunk(void* user);

    bool Refuse(const std::string& reason);

    EngineQ2SinkDesc desc_;
    Q2SinkCallbacks callbacks_;
    FrameIntegration frame_;
    SceneAdapter adapter_;
    SceneCommandQueue queue_;
    SceneExecuteResult lastResult_;
    std::string lastError_;
    uint32_t lightSerial_ = 0;
    uint32_t refusedCallbacks_ = 0;
    bool initialized_ = false;
    bool lastSwapSucceeded_ = false;
    std::map<uint32_t, SceneSurfaceGeometry> surfaces_;
    std::map<uint32_t, Q2SinkImageUpload> imageUploads_;
};

}  // namespace rhi
}  // namespace doom3
