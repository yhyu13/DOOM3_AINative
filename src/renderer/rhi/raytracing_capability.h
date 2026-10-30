#pragma once

// Explicit ray-tracing mode selection for Q6 (spec 004 T064).
//
// Selection is explicit. The caller states which mode it wants and whether it
// permits a fallback. This unit never switches a request to another mode on its
// own: when a capability or quality gate fails and the caller did not approve a
// fallback, the plan is `Unsupported` and says why, while also reporting that
// the non-path-traced renderer is available. That split is what keeps a missing
// device from being reported as a pass and from silently changing the look.
//
// The capability record is supplied by a backend. This unit does not query a
// device, does not know a vendor API and does not translate a tier into a
// hardware claim; a caller that cannot report a fact leaves it unsupported.

#include "rhi/rhi.h"

#include <cstdint>
#include <string>
#include <vector>

namespace doom3 {
namespace rhi {

enum class RaytracingTier {
    Unsupported = 0,
    Tier10,
    Tier11,
    Tier12,
};

const char* ToString(RaytracingTier value);

enum class RaytracingRequest {
    // The hybrid renderer reached in Q4. Always available on this path.
    Hybrid = 0,
    // Converged path-traced reference; not necessarily interactive.
    Reference,
    // Path-traced playable showcase; needs the motion, visual and performance
    // gates to have passed.
    Showcase,
};

const char* ToString(RaytracingRequest value);

struct RaytracingCapability {
    bool deviceSupportsRaytracing = false;
    RaytracingTier tier = RaytracingTier::Unsupported;
    // Shader model the backend reported. 6.3 is the minimum this plan accepts
    // for any ray-tracing mode; a backend that reports nothing leaves both at
    // zero and is treated as unsupported.
    uint32_t shaderModelMajor = 0;
    uint32_t shaderModelMinor = 0;
};

struct QualityGate {
    bool visualGatePassed = false;
    bool performanceGatePassed = false;
    bool motionGatePassed = false;
};

struct RaytracingSelectionDesc {
    RaytracingRequest requested = RaytracingRequest::Hybrid;
    RaytracingCapability capability;
    QualityGate gates;
    // Explicit opt-in to the hybrid renderer when the requested path-traced
    // mode cannot run. False means "report the limitation and stop".
    bool allowFallback = false;
};

struct RaytracingSelection {
    Status status = Status::Ok;
    std::string error;
    RaytracingRequest requested = RaytracingRequest::Hybrid;
    // Meaningful when `status == Status::Ok`: the mode that will run. It is
    // never a different mode than `requested` unless `fallbackApproved` is true.
    RaytracingRequest effective = RaytracingRequest::Hybrid;
    bool fallbackAvailable = true;
    bool fallbackApproved = false;
    bool pathTracingAvailable = false;
    // Every failed capability or gate, in a stable order. Empty on a clean plan.
    std::vector<std::string> limitations;
};

// Rules:
//   Hybrid    -> Ok, effective Hybrid, path tracing available iff the device
//                reports it.
//   Reference -> needs ray-tracing support and shader model >= 6.3.
//   Showcase  -> needs the same plus the motion, visual and performance gates.
// A failed requirement is Unsupported with a limitation. If `allowFallback` is
// true, the plan is Ok with `effective == Hybrid` and `fallbackApproved == true`;
// otherwise `effective` stays `Hybrid` as the available alternative but the plan
// is Unsupported and `fallbackApproved == false`, so a caller cannot mistake it
// for a silent switch.
Status PlanRaytracingSelection(const RaytracingSelectionDesc& desc,
                               RaytracingSelection* selection, std::string* error);

}  // namespace rhi
}  // namespace doom3
