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
};

// Value copy of the engine's q2LightFacts_t.
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
};

// Mirror of the engine's q2SceneSink_t callback table, with portable value
// types. The engine registration unit keeps the two in lockstep explicitly.
struct Q2SinkCallbacks {
    void* user = nullptr;
    bool (*initialize)(void* user, int width, int height, char* reason,
                       int reasonSize) = nullptr;
    void (*shutdown)(void* user) = nullptr;
    bool (*beginView)(void* user, unsigned int viewId, int x, int y, int width,
                      int height, bool overlay, bool worldGui,
                      const float projection[16]) = nullptr;
    bool (*drawPacket)(void* user, unsigned int surfaceId,
                       const Q2SinkMaterialFacts* material, int vertexCount,
                       int indexCount, int depthHackKind, float modelDepthHack,
                       bool worldGui, bool postProcess) = nullptr;
    bool (*lightPacket)(void* user, const Q2SinkLightFacts* light) = nullptr;
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
    SceneGeometrySource* geometry = nullptr;
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
class EngineQ2SinkConsumer {
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
    static bool BeginViewThunk(void* user, unsigned int viewId, int x, int y,
                               int width, int height, bool overlay,
                               bool worldGui, const float projection[16]);
    static bool DrawPacketThunk(void* user, unsigned int surfaceId,
                                const Q2SinkMaterialFacts* material,
                                int vertexCount, int indexCount,
                                int depthHackKind, float modelDepthHack,
                                bool worldGui, bool postProcess);
    static bool LightPacketThunk(void* user, const Q2SinkLightFacts* light);
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
};

}  // namespace rhi
}  // namespace doom3
