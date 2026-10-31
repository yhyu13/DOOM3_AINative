// T059 off/missing-provider behavioural test.
//
// Purpose: prove that the optional-provider boundary is inert without a
// provider. Absent, disabled, unauthorized, unbuilt or unsupported all resolve
// to the named off path with a non-empty actionable diagnostic, `featureEnabled`
// false and `providerRequired` false; nothing crashes and nothing switches
// silently.
//
// The capability register is asserted to be the contract's 15 UNKNOWN rows in
// order, all Unknown. This test never asserts an SDK fact, because no SDK
// contract is in hand (provider-contract.md §7).
//
// The input gate is asserted to refuse a partial set, an overlay input and an
// unspecified mask rather than infer them.

#include "q4_test_support.h"

#include "ext/provider.h"

#include <string>

namespace {

namespace ext = doom3::ext;
namespace rhi = doom3::rhi;

bool Contains(const std::string& text, const char* needle) {
    return text.find(needle) != std::string::npos;
}

ext::ProviderInputs CompleteInputs() {
    ext::ProviderInputs inputs;
    inputs.colourLease = true;
    inputs.depthLease = true;
    inputs.motionLease = true;
    inputs.validityLease = true;
    inputs.jitterReported = true;
    inputs.exposureRecorded = true;
    inputs.viewIdentity = true;
    inputs.overlayExcluded = true;
    inputs.renderWidth = 960;
    inputs.renderHeight = 540;
    inputs.displayWidth = 1920;
    inputs.displayHeight = 1080;
    return inputs;
}

void RunSelection(q4_test::Runner* runner) {
    // 1. Default request: nothing asked for, nothing enabled.
    const ext::ProviderPlan absent = ext::PlanProvider(ext::ProviderRequest{});
    runner->Check(absent.selection == ext::ProviderSelection::NoRequest,
                  "T059 no request selects NoRequest");
    runner->Check(!absent.requested, "T059 no request is not reported as requested");
    runner->Check(!absent.featureEnabled, "T059 no request keeps the feature off");
    runner->Check(!absent.providerRequired, "T059 no request requires no provider");
    runner->Check(absent.offPath, "T059 no request uses the off path");
    runner->Check(!absent.diagnostic.empty(), "T059 no request states the off path");

    // 2. A present provider does not auto-enable: Off is always selectable.
    ext::ProviderRequest present;
    present.kind = ext::ProviderKind::NeuralReconstruction;
    present.authorizationRecorded = true;
    present.adapterLinked = true;
    ext::ProviderRequest notAsked = present;
    notAsked.kind = ext::ProviderKind::None;
    const ext::ProviderPlan stillOff = ext::PlanProvider(notAsked);
    runner->Check(stillOff.selection == ext::ProviderSelection::NoRequest && !stillOff.featureEnabled,
                  "T059 an available provider never switches on without a request");
    runner->Check(stillOff.offPath, "T059 Off stays selectable when a provider is present");

    // 3. Explicit disable wins over authorization and linkage.
    ext::ProviderRequest disabled = present;
    disabled.disabledByUser = true;
    const ext::ProviderPlan off = ext::PlanProvider(disabled);
    runner->Check(off.selection == ext::ProviderSelection::DisabledByRequest,
                  "T059 explicit disable selects DisabledByRequest");
    runner->Check(off.requested, "T059 an explicit disable still reports the request");
    runner->Check(!off.featureEnabled, "T059 explicit disable keeps the feature off");
    runner->Check(off.offPath && Contains(off.diagnostic, "off path"),
                  "T059 explicit disable names the off path");
    runner->Check(!off.providerRequired, "T059 explicit disable requires no provider");

    // 4. Requested but unauthorized: the upstream gate is named.
    ext::ProviderRequest unauthorized;
    unauthorized.kind = ext::ProviderKind::NeuralReconstruction;
    const ext::ProviderPlan noAuth = ext::PlanProvider(unauthorized);
    runner->Check(noAuth.selection == ext::ProviderSelection::NotAuthorized,
                  "T059 an unauthorized request selects NotAuthorized");
    runner->Check(!noAuth.featureEnabled, "T059 an unauthorized request keeps the feature off");
    runner->Check(noAuth.offPath, "T059 an unauthorized request uses the off path");
    runner->Check(Contains(noAuth.diagnostic, "authorization"),
                  "T059 the unauthorized diagnostic names authorization");
    runner->Check(Contains(noAuth.diagnostic, "off path"),
                  "T059 the unauthorized diagnostic names the off path");
    runner->Check(noAuth.diagnostic.find("silent") == std::string::npos,
                  "T059 an authorized request is not described as a silent fallback");

    // 5. Authorized but no adapter linked: this is the default build.
    ext::ProviderRequest authorized;
    authorized.kind = ext::ProviderKind::NeuralReconstruction;
    authorized.authorizationRecorded = true;
    const ext::ProviderPlan notBuilt = ext::PlanProvider(authorized);
    runner->Check(notBuilt.selection == ext::ProviderSelection::NotBuilt,
                  "T059 an unbuilt provider selects NotBuilt");
    runner->Check(!notBuilt.featureEnabled, "T059 an unbuilt provider keeps the feature off");
    runner->Check(notBuilt.offPath, "T059 an unbuilt provider uses the off path");
    runner->Check(Contains(notBuilt.diagnostic, "T061"),
                  "T059 the unbuilt diagnostic names the authorized adapter task");
    runner->Check(Contains(notBuilt.diagnostic, "not a build prerequisite"),
                  "T059 the unbuilt diagnostic states no build dependency");

    // 6. Linked and authorized, but no capability row is settled.
    const ext::ProviderPlan unsupported = ext::PlanProvider(present);
    runner->Check(unsupported.selection == ext::ProviderSelection::Unsupported,
                  "T059 an unsettled capability selects Unsupported");
    runner->Check(!unsupported.featureEnabled,
                  "T059 an unsettled capability keeps the feature off");
    runner->Check(unsupported.offPath, "T059 an unsettled capability uses the off path");
    runner->Check(Contains(unsupported.diagnostic, "UNKNOWN"),
                  "T059 the unsupported diagnostic names the UNKNOWN register");
    runner->Check(Contains(unsupported.diagnostic, "off path"),
                  "T059 the unsupported diagnostic names the off path");

    // 7. Ready is unreachable for every authority/link combination in this build.
    bool anyReady = false;
    bool anyRequired = false;
    bool anyDiagnosticMissing = false;
    for (int authority = 0; authority < 2; ++authority) {
        for (int linked = 0; linked < 2; ++linked) {
            ext::ProviderRequest request;
            request.kind = ext::ProviderKind::NeuralReconstruction;
            request.authorizationRecorded = authority != 0;
            request.adapterLinked = linked != 0;
            const ext::ProviderPlan plan = ext::PlanProvider(request);
            anyReady = anyReady || plan.selection == ext::ProviderSelection::Ready;
            anyRequired = anyRequired || plan.providerRequired;
            anyDiagnosticMissing = anyDiagnosticMissing || plan.diagnostic.empty();
        }
    }
    runner->Check(!anyReady, "T059 no authority/link combination reaches Ready");
    runner->Check(!anyRequired, "T059 no plan ever requires a provider");
    runner->Check(!anyDiagnosticMissing, "T059 every unavailable plan carries a diagnostic");

    // 8. The plan is deterministic and repeatable (no hidden state, no crash).
    const ext::ProviderPlan again = ext::PlanProvider(present);
    runner->Check(again.selection == unsupported.selection &&
                      again.diagnostic == unsupported.diagnostic,
                  "T059 repeated planning is identical");

    // 9. Names are stable, non-null and distinct.
    runner->Check(ext::ToString(ext::ProviderSelection::NoRequest) != nullptr &&
                      Contains(ext::ToString(ext::ProviderSelection::NoRequest), "NoRequest"),
                  "T059 ProviderSelection names are readable");
    runner->Check(Contains(ext::ToString(ext::ProviderSelection::NotBuilt), "NotBuilt") &&
                      Contains(ext::ToString(ext::ProviderSelection::Unsupported), "Unsupported") &&
                      std::string(ext::ToString(ext::ProviderSelection::NotBuilt)) !=
                          ext::ToString(ext::ProviderSelection::Unsupported),
                  "T059 ProviderSelection names are distinct");
    runner->Check(Contains(ext::ToString(ext::ProviderKind::NeuralReconstruction),
                           "NeuralReconstruction"),
                  "T059 ProviderKind names are readable");
    runner->Check(Contains(ext::ToString(ext::UnknownState::Unknown), "Unknown"),
                  "T059 UnknownState names are readable");
}

void RunCapability(q4_test::Runner* runner) {
    const ext::CapabilityReport& report = ext::ProviderCapability();
    runner->Check(!report.supported, "T059 capability is unsupported while rows are UNKNOWN");
    runner->Check(report.unknownCount == 15, "T059 the register reports 15 rows");
    runner->Check(report.unknowns != nullptr, "T059 the register exposes its rows");
    runner->Check(!report.diagnostic.empty(), "T059 the capability answer is actionable");
    runner->Check(Contains(report.diagnostic, "T060"),
                  "T059 the capability answer names the clearance gate");

    bool idsInOrder = true;
    bool allUnknown = true;
    for (uint32_t index = 0; index < report.unknownCount; ++index) {
        const ext::UnknownFact& fact = report.unknowns[index];
        const std::string expected = "U" + std::to_string(index + 1u);
        idsInOrder = idsInOrder && fact.id != nullptr && expected == fact.id;
        allUnknown = allUnknown && fact.state == ext::UnknownState::Unknown;
        runner->Check(fact.label != nullptr && fact.label[0] != '\0',
                      "T059 register row " + expected + " has a label");
    }
    runner->Check(idsInOrder, "T059 register ids are U1..U15 in order");
    runner->Check(allUnknown, "T059 every register row is Unknown, none inferred");

    // Stable storage: the report is immutable and not rebuilt per call.
    const ext::CapabilityReport& second = ext::ProviderCapability();
    runner->Check(&second == &report && second.unknowns == report.unknowns,
                  "T059 the capability report is stable across calls");
}

void RunInputGate(q4_test::Runner* runner) {
    // 1. The complete set is accepted.
    const ext::ProviderInputCheck complete = ext::ValidateProviderInputs(CompleteInputs());
    runner->Check(complete.status == rhi::Status::Ok, "T059 a complete input set is accepted");
    runner->Check(complete.complete, "T059 a complete input set reports complete");
    runner->Check(complete.diagnostic.empty(), "T059 a complete input set has no diagnostic");

    // 2. Extent errors are refused before anything else.
    ext::ProviderInputs zero = CompleteInputs();
    zero.renderWidth = 0;
    const ext::ProviderInputCheck zeroCheck = ext::ValidateProviderInputs(zero);
    runner->Check(zeroCheck.status != rhi::Status::Ok && !zeroCheck.complete,
                  "T059 a zero render extent is refused");
    runner->Check(Contains(zeroCheck.diagnostic, "extents"),
                  "T059 the zero-extent diagnostic names the extents");

    ext::ProviderInputs smaller = CompleteInputs();
    smaller.displayWidth = smaller.renderWidth - 1;
    const ext::ProviderInputCheck smallerCheck = ext::ValidateProviderInputs(smaller);
    runner->Check(smallerCheck.status != rhi::Status::Ok,
                  "T059 a smaller display extent is refused");
    runner->Check(Contains(smallerCheck.diagnostic, "display extent"),
                  "T059 the smaller-display diagnostic names the display extent");

    // 3. The overlay is never an input.
    ext::ProviderInputs overlay = CompleteInputs();
    overlay.overlayExcluded = false;
    const ext::ProviderInputCheck overlayCheck = ext::ValidateProviderInputs(overlay);
    runner->Check(overlayCheck.status != rhi::Status::Ok,
                  "T059 an overlay input is refused");
    runner->Check(Contains(overlayCheck.diagnostic, "overlay"),
                  "T059 the overlay diagnostic names the overlay");

    // 4. A partial lease set is never passed.
    struct LeaseCase {
        bool ext::ProviderInputs::*member;
        const char* label;
    };
    const LeaseCase leases[] = {
        {&ext::ProviderInputs::colourLease, "colour"},
        {&ext::ProviderInputs::depthLease, "depth"},
        {&ext::ProviderInputs::motionLease, "motion vector"},
        {&ext::ProviderInputs::validityLease, "validity"},
    };
    for (const LeaseCase& lease : leases) {
        ext::ProviderInputs partial = CompleteInputs();
        partial.*(lease.member) = false;
        const ext::ProviderInputCheck check = ext::ValidateProviderInputs(partial);
        runner->Check(check.status != rhi::Status::Ok && !check.complete,
                      std::string("T059 a missing ") + lease.label + " lease is refused");
        runner->Check(Contains(check.diagnostic, "partial input set"),
                      std::string("T059 the missing ") + lease.label +
                          " diagnostic names the partial set");
    }

    // 5. A mask is enabled explicitly or not at all; it is never inferred.
    ext::ProviderInputs mask = CompleteInputs();
    mask.maskSupplied = true;
    const ext::ProviderInputCheck maskCheck = ext::ValidateProviderInputs(mask);
    runner->Check(maskCheck.status != rhi::Status::Ok,
                  "T059 an unspecified mask is refused");
    runner->Check(Contains(maskCheck.diagnostic, "not inferred"),
                  "T059 the mask diagnostic refuses to infer the semantic");
    mask.maskSemanticRecorded = true;
    const ext::ProviderInputCheck maskOk = ext::ValidateProviderInputs(mask);
    runner->Check(maskOk.status == rhi::Status::Ok && maskOk.complete,
                  "T059 a recorded mask semantic is accepted");

    // 6. The UNKNOWN conventions must be recorded, not guessed.
    ext::ProviderInputs jitter = CompleteInputs();
    jitter.jitterReported = false;
    const ext::ProviderInputCheck jitterCheck = ext::ValidateProviderInputs(jitter);
    runner->Check(jitterCheck.status != rhi::Status::Ok &&
                      Contains(jitterCheck.diagnostic, "jitter"),
                  "T059 an unreported jitter offset is refused");

    ext::ProviderInputs exposure = CompleteInputs();
    exposure.exposureRecorded = false;
    const ext::ProviderInputCheck exposureCheck = ext::ValidateProviderInputs(exposure);
    runner->Check(exposureCheck.status != rhi::Status::Ok &&
                      Contains(exposureCheck.diagnostic, "exposure"),
                  "T059 an unrecorded exposure point is refused");

    ext::ProviderInputs identity = CompleteInputs();
    identity.viewIdentity = false;
    const ext::ProviderInputCheck identityCheck = ext::ValidateProviderInputs(identity);
    runner->Check(identityCheck.status != rhi::Status::Ok &&
                      Contains(identityCheck.diagnostic, "identity"),
                  "T059 a missing view identity is refused");

    // 7. Failure never reports complete, and success is repeatable.
    const ext::ProviderInputCheck repeat = ext::ValidateProviderInputs(CompleteInputs());
    runner->Check(repeat.status == rhi::Status::Ok && repeat.complete &&
                      repeat.diagnostic == complete.diagnostic,
                  "T059 the input gate is deterministic and stateless");
}

}  // namespace

int main() {
    q4_test::Runner runner;
    std::printf("T059 optional-provider off path\n");
    RunSelection(&runner);
    RunCapability(&runner);
    RunInputGate(&runner);
    return runner.Finish("T059 provider off path");
}
