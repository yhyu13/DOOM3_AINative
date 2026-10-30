#pragma once

// Backend-neutral ordered scene stream for the classic path (spec 004 T035/T080).
//
// The engine front end still produces drawSurf_t / viewDef_t / viewLight_t and the legacy
// RC_* command list. Everything GL-shaped is translated at this boundary before it reaches
// this header: the set-buffer selector arrives as a plain integer and is rejected when it is
// not a known value, a capture destination arrives as a registered image identity rather
// than a pointer, and a draw packet carries counts and ids rather than a vertex-cache
// handle. No GL enum, image pointer or vertex-cache offset is named here.
//
// Ordering follows classic-parity.md §4 (O-01..O-14) and engine-integration.md §6.

#include "rhi/rhi.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace doom3 {
namespace rhi {

enum class SceneCommandKind {
    BeginView = 0,
    EndView,
    DrawPacket,
    SetColorTarget,
    CopyToImage,
    SwapBuffers,
};

const char* ToString(SceneCommandKind value);

// The legacy set-buffer value is a GL order code. Only the two values the classic path can
// honour are accepted; anything else is Unsupported and never resolves to "the other" target.
enum class ColorTargetSelector { Current = 0, Offscreen, Unsupported };

constexpr uint32_t kEngineBufferFront = 0x0404u;
constexpr uint32_t kEngineBufferBack = 0x0405u;

Status TranslateColorTarget(uint32_t engineBufferValue, ColorTargetSelector* target,
                            std::string* error);

enum class DepthHackKind { None = 0, Weapon, Model };

// Display overlays are a separate 2D view drawn after the 3D scene at display resolution
// (O-11); an in-world GUI surface stays in the 3D view (O-12).
constexpr uint32_t kOverlayVirtualWidth = 640;
constexpr uint32_t kOverlayVirtualHeight = 480;

struct ViewDesc {
    uint32_t identity = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    bool overlay = false;
    bool worldGui = false;
    DepthHackKind depthHack = DepthHackKind::None;
    // Column-major 4x4 projection in the engine's convention.
    float projection[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                            0.0f, 1.0f, 0.0f, 0.0f,
                            0.0f, 0.0f, 1.0f, 0.0f,
                            0.0f, 0.0f, 0.0f, 1.0f};
};

// 640x480 ortho mapping onto a display-sized overlay view, top-left origin (O-11).
ViewDesc MakeDisplayOverlayView(uint32_t identity, uint32_t width, uint32_t height);

// O-13 depth hacks: the x/y terms are untouched and only the z term and the usable depth
// interval move. The caller passes the same column-major projection it would use for the
// unmodified object.
void ApplyWeaponDepthHack(float projection[16]);
void ApplyModelDepthHack(float projection[16], float distance);

struct DrawSurfPacket {
    uint64_t sortKey = 0;
    uint32_t surfaceId = 0;
    uint32_t materialId = 0;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    uint32_t stageCount = 0;
    uint32_t lightCount = 0;
    bool alphaTested = false;
    bool translucent = false;
    bool depthHack = false;
    bool worldGui = false;
    bool postProcess = false;
};

struct SceneCommand {
    SceneCommandKind kind = SceneCommandKind::BeginView;
    ViewDesc view;
    DrawSurfPacket draw;
    ColorTargetSelector target = ColorTargetSelector::Current;
    uint32_t imageId = 0;
};

// One queue per frame, appended in the engine's RC_* order (O-02).
class SceneCommandQueue {
public:
    // Returns false when the command cannot be expressed (for example a copy with no
    // destination identity); the caller reports it instead of drawing a silent no-op.
    bool BeginView(const ViewDesc& view);
    bool EndView();
    bool Draw(const DrawSurfPacket& packet);
    bool SetColorTarget(ColorTargetSelector target);
    // CaptureRenderToImage: append the copy and keep recording.
    bool CopyToImage(uint32_t imageId);
    // CaptureRenderToFile: append the copy and mark the stream for an immediate
    // execute-and-clear, so the copy keeps its mid-frame position.
    bool CaptureToFile(uint32_t imageId);
    bool SwapBuffers();

    size_t Size() const { return commands_.size(); }
    bool Empty() const { return commands_.empty(); }
    void Clear();
    const std::vector<SceneCommand>& commands() const { return commands_; }

    bool HasPendingFlush() const { return flushPending_; }
    // Exclusive end of the prefix the executor must consume now. Valid only while
    // HasPendingFlush() is true; it stops before commands recorded after the capture.
    size_t FlushMark() const { return flushMark_; }
    // Drops the consumed prefix and clears the pending mark.
    void ConsumeFront(size_t count);

private:
    bool Push(const SceneCommand& command);

    std::vector<SceneCommand> commands_;
    size_t flushMark_ = 0;
    bool flushPending_ = false;
};

constexpr size_t kNoFlushMark = static_cast<size_t>(-1);

struct SceneAdapterDesc {
    uint32_t displayWidth = 0;
    uint32_t displayHeight = 0;
    // The Q1b seam can compose a colour frame but cannot accept vertex/index geometry, so
    // a draw packet can be extracted and classified but not submitted. Structural mode
    // must be requested explicitly so an extraction-only run is never reported as a frame.
    bool structuralOnly = true;
};

struct SceneStats {
    uint32_t views = 0;
    uint32_t overlays = 0;
    uint32_t draws = 0;
    uint32_t drawsRefused = 0;
    uint32_t drawsSubmitted = 0;
};

struct SceneExecuteResult {
    std::vector<SceneCommandKind> executed;
    // Identity of every BeginView in execution order, so a caller can prove the display
    // overlay was appended after the 3D scene rather than merely being drawn "somewhere".
    std::vector<uint32_t> viewIds;
    uint32_t views = 0;
    uint32_t overlays = 0;
    uint32_t drawsExtracted = 0;
    uint32_t drawsSubmitted = 0;
    // Packets whose material plan refused every stage. They are counted and never drawn, so
    // a stub/absent classic program cannot leak into a frame as a silent fallback.
    uint32_t drawsRefused = 0;
    bool drawSubmissionUnavailable = false;
};

class SceneAdapter {
public:
    SceneAdapter();
    ~SceneAdapter();

    SceneAdapter(const SceneAdapter&) = delete;
    SceneAdapter& operator=(const SceneAdapter&) = delete;

    Status Initialize(const SceneAdapterDesc& desc, std::string* error);
    bool initialized() const { return initialized_; }
    const SceneAdapterDesc& desc() const { return desc_; }

    // Consumes the pending prefix in order: everything after a CaptureToFile mark, or the
    // whole queue when no mid-frame capture was recorded. A structurally invalid stream is
    // InvalidArgument and consumes nothing.
    Status Execute(SceneCommandQueue* queue, SceneExecuteResult* result, std::string* error);

    const SceneStats& stats() const { return stats_; }

private:
    SceneAdapterDesc desc_;
    SceneStats stats_;
    bool initialized_ = false;
};

}  // namespace rhi
}  // namespace doom3
