#pragma once

// Optional neural-provider boundary (spec 004 T059; contract provider-contract.md).
//
// This unit is the non-proprietary half of the optional provider: it selects,
// refuses and diagnoses. It links no provider, includes no vendor SDK header and
// downloads nothing at configure, build or run time (FR-014).
//
// The provider itself is not implemented and is not authorized. The contract's
// UNKNOWN register (provider-contract.md §7, U1..U15) is represented here as
// `UnknownState::Unknown`, never as an invented constant, enum, version or
// function. `ProviderCapability().supported` therefore stays false and
// `ProviderSelection::Ready` is unreachable in this build.
//
// Off is the reference: every unavailable or disabled result names the off path
// and the reason, and `providerRequired` is always false. Nothing switches to a
// provider automatically, and nothing switches back silently.
//
// No native type, handle, state or synchronization value appears in this
// header. It lives under src/renderer/ext/ so the engine-facing core gate is not
// weakened.

#include "rhi/rhi.h"

#include <cstdint>
#include <string>

namespace doom3 {
namespace ext {

// A provider slot a caller may name. The value is a project-local slot, not an
// SDK product name, version or support promise.
enum class ProviderKind {
    None = 0,
    NeuralReconstruction = 1,
};

const char* ToString(ProviderKind value);

// Why selection resolved the way it did. Every non-Ready result is explicit;
// a missing provider is never reported as a silent success of the provider path.
enum class ProviderSelection {
    // Nothing was requested. `Off` is the reference and needs no component.
    NoRequest = 0,
    // The caller named a provider and then turned it off. Off is selected, named.
    DisabledByRequest = 1,
    // Requested, but no authorization is recorded (T060). A remembered
    // preference is not an authorization (contract §3.1).
    NotAuthorized = 2,
    // Requested and authorized, but no adapter is linked into this build (T061).
    NotBuilt = 3,
    // Requested, authorized and linked, but the capability register is unsettled
    // or the selected backend cannot carry it.
    Unsupported = 4,
    // Requested, authorized, linked and supported. Unreachable in this build:
    // no row of the UNKNOWN register is settled.
    Ready = 5,
};

const char* ToString(ProviderSelection value);

// The state of one UNKNOWN register row. A row becomes Settled only through the
// evidence named in provider-contract.md §7, never by inference.
enum class UnknownState {
    Unknown = 0,
    Settled = 1,
};

const char* ToString(UnknownState value);

struct UnknownFact {
    // "U1".."U15", matching provider-contract.md §7.
    const char* id;
    // Short label from the register. Not an SDK claim.
    const char* label;
    UnknownState state;
};

// The capability answer for this build. `supported` is false while any required
// row is Unknown; `diagnostic` states what would settle them. The pointers
// remain valid for the life of the process and the report is immutable.
struct CapabilityReport {
    bool supported = false;
    const UnknownFact* unknowns = nullptr;
    uint32_t unknownCount = 0;
    std::string diagnostic;
};

const CapabilityReport& ProviderCapability();

struct ProviderRequest {
    ProviderKind kind = ProviderKind::None;
    // T060's recorded clearance. A remembered preference is not an authorization.
    bool authorizationRecorded = false;
    // The caller explicitly turned the provider off for this renderer instance.
    bool disabledByUser = false;
    // Whether an adapter translation unit is linked into this build. T059 is the
    // off path, so the default build links none.
    bool adapterLinked = false;
};

struct ProviderPlan {
    // The caller named a provider (`kind != None`).
    bool requested = false;
    // True only when the provider would actually run. False for every explicit
    // off path, including a requested-but-unavailable one.
    bool featureEnabled = false;
    // Always false. No stage, frame or test requires a provider.
    bool providerRequired = false;
    ProviderSelection selection = ProviderSelection::NoRequest;
    // The off path carries this frame. It is named by `selection` and
    // `diagnostic`, never entered as a silent fallback.
    bool offPath = true;
    // Actionable when the provider cannot run: what blocks it and what the
    // renderer does instead. Empty only for `Ready`, which this build cannot
    // reach.
    std::string diagnostic;
};

// Precedence when several conditions hold at once: NoRequest, then
// DisabledByRequest, then NotAuthorized, then NotBuilt, then Unsupported. The
// earliest blocking condition is the one reported, so the diagnostic names the
// upstream gate first. Pure: no allocation beyond the returned string, no state,
// no error path.
ProviderPlan PlanProvider(const ProviderRequest& request);

// Semantic inputs a provider evaluation would need (contract §5). The concrete
// SDK parameter names are UNKNOWN; this struct records only what the renderer
// must guarantee or document, so a caller cannot pass a partial or ambiguous
// set and let the boundary infer the rest.
struct ProviderInputs {
    bool colourLease = false;
    bool depthLease = false;
    // Previous rendered position minus current rendered position (FR-008).
    bool motionLease = false;
    // A separate signal from zero motion. A zero vector is never validity.
    bool validityLease = false;
    // The frame's jitter offset is reported, and who applies it is recorded.
    bool jitterReported = false;
    // Whether exposure was applied before the evaluation is documented.
    bool exposureRecorded = false;
    // Stable frame/view identity plus generation, for history invalidation.
    bool viewIdentity = false;
    // Reconstruction must not see the display overlay (FR-011). Must stay true.
    bool overlayExcluded = true;
    // Optional reactive/transparency mask, enabled explicitly (contract §6).
    bool maskSupplied = false;
    // Producer, semantic, format and scale recorded. Required when supplied.
    bool maskSemanticRecorded = false;
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
    uint32_t displayWidth = 0;
    uint32_t displayHeight = 0;
};

struct ProviderInputCheck {
    rhi::Status status = rhi::Status::Ok;
    // Every required lease and convention is present and unambiguous.
    bool complete = false;
    std::string diagnostic;
};

// Validates the input set without inventing any SDK convention. It refuses a
// partial set, an overlay input and an unspecified mask rather than guessing.
ProviderInputCheck ValidateProviderInputs(const ProviderInputs& inputs);

}  // namespace ext
}  // namespace doom3
