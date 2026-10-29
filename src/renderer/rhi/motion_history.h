#pragma once

// Portable motion and correspondence seam for Q3 (tasks T042 and T046).
//
// This header is part of the engine-facing core and names no native API type,
// handle or synchronization value. It defines the analytic contract used by
// tests/renderer/motion-fixtures.md: previous unjittered UV minus current
// unjittered UV, top-left origin, validity separate from the vector.
//
// The seam is deliberately narrow. It projects one already-posed world point
// per state and decides whether a previous rendered correspondence exists; it
// does not know about meshes, shaders, frame slots or a backend. Skinned pose
// retention lives in skinned_history.h.
//
// Validity is an enum, not a sentinel vector. A stationary surface is VALID
// with (0,0); a missing correspondence is INVALID and its vector must not be
// consumed.

#include <cstdint>

namespace doom3 {
namespace rhi {

struct MotionVec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// One camera state for one rendered view. `forward`, `left` and `up` are the
// engine's view axes; the history projection is the closed form derived in
// tests/renderer/motion-fixtures.md section 2.2. `jitterPixels*` belongs to the
// raster projection only and is never part of the history vector.
struct CameraState {
    MotionVec3 eye;
    MotionVec3 forward{1.0, 0.0, 0.0};
    MotionVec3 left{0.0, 1.0, 0.0};
    MotionVec3 up{0.0, 0.0, 1.0};
    // Engine 16:9 pair from idGameLocal::CalcFov (base 90, 640x480 seed).
    double tanX = 4.0 / 3.0;
    double tanY = 3.0 / 4.0;
    // One-sided pixel shift in [0,1); excluded from the history projection.
    double jitterPixelsX = 0.0;
    double jitterPixelsY = 0.0;
    uint32_t width = 1280;
    uint32_t height = 720;
};

struct MotionUv {
    double u = 0.0;
    double v = 0.0;
};

// Reasons are stable identifiers, not a numeric code. A family may be split
// further without changing the fixture contract.
enum class MotionValidity : uint32_t {
    Valid = 0,
    NoHistory,
    Disocclusion,
    BehindCamera,
    Topology,
    ProviderSwitch,
    Generation,
    Reset,
    Resize,
    EntityReuse,
};

const char* ToString(MotionValidity value);

// One point in one rendered state. `hasRenderedRecord` false means the main
// view has not rendered that correspondence yet (no previous frame).
struct RenderRecord {
    MotionVec3 point;
    bool hasRenderedRecord = true;
};

struct MotionVector {
    double du = 0.0;
    double dv = 0.0;
};

struct MotionSample {
    MotionValidity validity = MotionValidity::NoHistory;
    MotionVector vector;

    // A valid zero is a real stationary vector.
    bool HasVector() const { return validity == MotionValidity::Valid; }
};

// History projection: unjittered, top-left origin. Returns false when the point
// is behind the camera or the projection is degenerate.
bool ProjectHistoryPoint(const CameraState& camera, const MotionVec3& point,
                         MotionUv* uv);

// Raster projection: the same basis with the one-sided jitter shift
// (u -= jx/width, v += jy/height). It exists so a test can prove the history
// path ignores jitter; it is not the history value.
bool ProjectRasterPoint(const CameraState& camera, const MotionVec3& point,
                        MotionUv* uv);

// previous unjittered UV minus current unjittered UV. The previous state must
// be the last frame that rendered the main view, not the last simulation tick
// and not a subview. Invalid results carry no usable vector.
MotionSample ReprojectPoint(const CameraState& previousCamera,
                            const RenderRecord& previous,
                            const CameraState& currentCamera,
                            const RenderRecord& current);

// IEEE binary16 word for RG16F storage, and its decode. Round to nearest even.
// The word is the 16-bit pattern, not the little-endian byte string.
uint16_t MotionHalfWord(double value);
double DecodeMotionHalfWord(uint16_t word);

enum class SurfaceClass : uint32_t {
    Static,
    Rigid,
    Skinned,
    Viewmodel,
    Particle,
    Transparent,
    Reflection,
    Decal,
    Deformation,
};

// Inputs to the correspondence/rejection policy. They are independent booleans
// so a caller cannot fabricate a vector by omitting a check.
struct CorrespondenceDesc {
    bool hasPreviousRenderedRecord = true;
    bool sameSurfaceIdentity = true;
    bool sameTopology = true;
    bool newPixel = false;
    bool providerSwitched = false;
    bool generationMismatch = false;
    bool resetOccurred = false;
    bool resized = false;
    bool entitySlotReused = false;
    bool respawned = false;
    // A subview (mirror, camera, shadow) must not produce or commit the main
    // view's correspondence.
    bool subview = false;
};

struct CorrespondenceResult {
    MotionValidity validity = MotionValidity::NoHistory;
    bool canUseVector = false;
};

// Explicit policy for T046. A rejected input never returns a fabricated zero.
// Precedence is documented in the implementation so a new reason cannot
// silently change an existing one.
CorrespondenceResult EvaluateCorrespondence(SurfaceClass surface,
                                            const CorrespondenceDesc& desc);

}  // namespace rhi
}  // namespace doom3
