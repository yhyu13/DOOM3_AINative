// Optional neural-provider boundary (spec 004 T059).
//
// This translation unit implements the non-proprietary half: selection, the
// capability answer, and the input gate. It contains no vendor SDK call, no
// download, no dynamic loading and no native handle. The contract is
// specs/004-quality-renderer/provider-contract.md; the UNKNOWN register there
// (§7, U1..U15) is reported, not guessed.

#include "ext/provider.h"

#include <utility>

namespace doom3 {
namespace ext {

namespace {

// provider-contract.md §7, in register order. Labels are short restatements of
// that register; they are not SDK facts.
const UnknownFact kUnknowns[] = {
    {"U1", "SDK name, API surface, version and header identities", UnknownState::Unknown},
    {"U2", "Whether the selected backend is supported at all", UnknownState::Unknown},
    {"U3", "Required resource state and ownership at lease boundaries", UnknownState::Unknown},
    {"U4", "Required queue and synchronization model", UnknownState::Unknown},
    {"U5", "Colour input set: tonemap point, format and exposure", UnknownState::Unknown},
    {"U6", "Expected motion-vector convention", UnknownState::Unknown},
    {"U7", "Jitter coupling: who jitters and what offset arrives", UnknownState::Unknown},
    {"U8", "Mask set, formats, scales and authors", UnknownState::Unknown},
    {"U9", "Reset semantics and cost", UnknownState::Unknown},
    {"U10", "Device-loss behaviour during evaluation", UnknownState::Unknown},
    {"U11", "Hardware capability query, minimum driver and feature level",
     UnknownState::Unknown},
    {"U12", "Distribution and licence terms for the combined work", UnknownState::Unknown},
    {"U13", "Whether a product exists for this backend and its claims", UnknownState::Unknown},
    {"U14", "Cost and benefit on named hardware", UnknownState::Unknown},
    {"U15", "Whether output may be composed with an overlay it must not see",
     UnknownState::Unknown},
};

constexpr uint32_t kUnknownCount =
    static_cast<uint32_t>(sizeof(kUnknowns) / sizeof(kUnknowns[0]));

ProviderInputCheck Reject(std::string diagnostic) {
    ProviderInputCheck check;
    check.status = rhi::Status::InvalidArgument;
    check.complete = false;
    check.diagnostic = std::move(diagnostic);
    return check;
}

}  // namespace

const char* ToString(ProviderKind value) {
    switch (value) {
        case ProviderKind::None: return "None";
        case ProviderKind::NeuralReconstruction: return "NeuralReconstruction";
    }
    return "Unknown";
}

const char* ToString(ProviderSelection value) {
    switch (value) {
        case ProviderSelection::NoRequest: return "NoRequest";
        case ProviderSelection::DisabledByRequest: return "DisabledByRequest";
        case ProviderSelection::NotAuthorized: return "NotAuthorized";
        case ProviderSelection::NotBuilt: return "NotBuilt";
        case ProviderSelection::Unsupported: return "Unsupported";
        case ProviderSelection::Ready: return "Ready";
    }
    return "Unknown";
}

const char* ToString(UnknownState value) {
    switch (value) {
        case UnknownState::Unknown: return "Unknown";
        case UnknownState::Settled: return "Settled";
    }
    return "Unknown";
}

const CapabilityReport& ProviderCapability() {
    static const CapabilityReport report = [] {
        CapabilityReport value;
        value.supported = false;
        value.unknowns = kUnknowns;
        value.unknownCount = kUnknownCount;
        value.diagnostic =
            "no authorized provider contract is in hand: U1-U15 of the UNKNOWN register are "
            "unsettled, so no capability is claimed. Settling them needs T060 clearance (SDK, "
            "backend and distribution) followed by a create/evaluate on the selected backend; "
            "the off path is unaffected";
        return value;
    }();
    return report;
}

ProviderPlan PlanProvider(const ProviderRequest& request) {
    ProviderPlan plan;

    if (request.kind == ProviderKind::None) {
        plan.selection = ProviderSelection::NoRequest;
        plan.diagnostic = "provider not requested; the off path is the reference and is byte-exact";
        return plan;
    }

    plan.requested = true;

    // Precedence follows the order in the header: the earliest blocking gate is
    // reported, so the caller learns the upstream remedy first.
    if (request.disabledByUser) {
        plan.selection = ProviderSelection::DisabledByRequest;
        plan.diagnostic =
            "provider disabled by explicit request; the off path is selected and named, not a "
            "silent fallback";
        return plan;
    }

    if (!request.authorizationRecorded) {
        plan.selection = ProviderSelection::NotAuthorized;
        plan.diagnostic =
            "provider requested but no authorization is recorded; record T060 clearance first. "
            "A remembered preference is not an authorization. The off path is in use";
        return plan;
    }

    if (!request.adapterLinked) {
        plan.selection = ProviderSelection::NotBuilt;
        plan.diagnostic =
            "provider requested and authorized but no adapter is linked into this build; the "
            "authorized adapter is T061 and is not a build prerequisite. The off path is in use";
        return plan;
    }

    const CapabilityReport& capability = ProviderCapability();
    if (!capability.supported) {
        plan.selection = ProviderSelection::Unsupported;
        plan.diagnostic = capability.diagnostic + "; the off path is in use";
        return plan;
    }

    // Only reachable once an adapter exists and every register row is settled.
    plan.selection = ProviderSelection::Ready;
    plan.featureEnabled = true;
    plan.offPath = false;
    return plan;
}

ProviderInputCheck ValidateProviderInputs(const ProviderInputs& inputs) {
    if (inputs.renderWidth == 0 || inputs.renderHeight == 0 || inputs.displayWidth == 0 ||
        inputs.displayHeight == 0) {
        return Reject("provider input extents must be non-zero");
    }
    if (inputs.displayWidth < inputs.renderWidth || inputs.displayHeight < inputs.renderHeight) {
        return Reject("provider evaluation upscales; the display extent is smaller than the render "
                      "extent");
    }
    if (!inputs.overlayExcluded) {
        return Reject("reconstruction must not see the display overlay; compose the overlay after "
                      "reconstruction");
    }

    // A partial input set is never passed. Each lease is named so the caller can
    // fix the specific omission.
    if (!inputs.colourLease) {
        return Reject("provider input set is missing the colour lease; a partial input set is "
                      "never passed");
    }
    if (!inputs.depthLease) {
        return Reject("provider input set is missing the depth lease; a partial input set is "
                      "never passed");
    }
    if (!inputs.motionLease) {
        return Reject("provider input set is missing the motion-vector lease; a partial input set "
                      "is never passed");
    }
    if (!inputs.validityLease) {
        return Reject("provider input set is missing the validity lease; zero motion is not "
                      "validity and a partial input set is never passed");
    }

    // Mask semantics are never inferred (contract §6).
    if (inputs.maskSupplied && !inputs.maskSemanticRecorded) {
        return Reject("a mask was supplied without a recorded producer, semantic, format and "
                      "scale; the mask is not enabled and is not inferred");
    }

    // The conventions themselves are UNKNOWN; the renderer must at least record
    // which one it used so the evaluation is reproducible.
    if (!inputs.jitterReported) {
        return Reject("the frame's jitter offset and the jitter convention must be reported "
                      "before evaluation");
    }
    if (!inputs.exposureRecorded) {
        return Reject("whether exposure was applied before evaluation must be recorded");
    }
    if (!inputs.viewIdentity) {
        return Reject("stable frame/view identity and generation are required so provider history "
                      "can be invalidated");
    }

    ProviderInputCheck check;
    check.status = rhi::Status::Ok;
    check.complete = true;
    return check;
}

}  // namespace ext
}  // namespace doom3
