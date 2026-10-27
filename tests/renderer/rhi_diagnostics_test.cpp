#include "q1a_test_support.h"

#include <string>

using doom3::rhi::AdapterClass;
using doom3::rhi::ClearReadbackDesc;
using doom3::rhi::Device;
using doom3::rhi::DeviceOptions;
using doom3::rhi::DiagnosticState;
using doom3::rhi::ReadbackResult;
using doom3::rhi::Status;

namespace {

bool AllowedDiagnostic(DiagnosticState state) {
    return state == DiagnosticState::Run || state == DiagnosticState::Skipped;
}

void LogCapturedMessages(const char* label, const doom3::rhi::Diagnostics& diagnostics) {
    q1a_test::Log("DIAGNOSTICS %s captured=%zu message_count=%u", label,
                  diagnostics.messages.size(), diagnostics.messageCount);
    for (size_t index = 0; index < diagnostics.messages.size(); ++index) {
        q1a_test::Log("DIAGNOSTICS %s message[%zu]=%s", label, index,
                      diagnostics.messages[index].c_str());
    }
}

// Each opt-in combination gets its own device so a regression in one request
// pattern cannot hide behind another. 0 = pass, 77 = adapter unavailable.
int RunDiagnosticsCase(AdapterClass adapterClass, bool wantDebug, bool wantGbv,
                       const char* combo) {
    Device device;
    DeviceOptions options;
    options.adapterClass = adapterClass;
    options.enableDebugLayer = wantDebug;
    options.enableGpuBasedValidation = wantGbv;
    std::string error;
    const Status initialize = device.Initialize(options, &error);
    if (initialize == Status::Unsupported) {
        q1a_test::Log("DIAGNOSTICS class=%s combo=%s SKIPPED reason=%s",
                      doom3::rhi::ToString(adapterClass), combo, error.c_str());
        return q1a_test::kSkipExitCode;
    }
    bool ok = q1a_test::Require(initialize == Status::Ok,
                                "diagnostics Initialize failed");
    if (initialize != Status::Ok) {
        device.Shutdown();
        return 1;
    }

    const doom3::rhi::Diagnostics& requested = device.diagnostics();
    q1a_test::Log("DIAGNOSTICS class=%s combo=%s debug_requested=%s gbv_requested=%s "
                  "debug=%s gpu_validation=%s messages=%u errors=%u warnings=%u",
                  doom3::rhi::ToString(adapterClass), combo,
                  wantDebug ? "true" : "false", wantGbv ? "true" : "false",
                  doom3::rhi::ToString(requested.debugLayer),
                  doom3::rhi::ToString(requested.gpuBasedValidation),
                  requested.messageCount, requested.errorCount, requested.warningCount);
    q1a_test::Log("DIAGNOSTICS debug_detail=%s", requested.debugLayerDetail.c_str());
    q1a_test::Log("DIAGNOSTICS gpu_validation_detail=%s",
                  requested.gpuBasedValidationDetail.c_str());
    LogCapturedMessages("requested", requested);

    if (wantDebug) {
        ok &= q1a_test::Require(AllowedDiagnostic(requested.debugLayer),
                                "debug layer was requested but is NOT_RUN");
    } else if (wantGbv) {
        // A GBV-only request may enable the debug layer implicitly (recorded
        // as RUN) or fail before enablement (NOT_REQUESTED). It must never
        // report an un-attempted skip.
        ok &= q1a_test::Require(
            requested.debugLayer == DiagnosticState::NotRequested ||
                requested.debugLayer == DiagnosticState::Run,
            "GBV-only request left the debug layer in an unattempted state");
    } else {
        ok &= q1a_test::Require(requested.debugLayer == DiagnosticState::NotRequested,
                                "debug layer was not requested but is not NOT_REQUESTED");
    }

    if (wantGbv) {
        ok &= q1a_test::Require(AllowedDiagnostic(requested.gpuBasedValidation),
                                "GPU-based validation was requested but is NOT_RUN");
        ok &= q1a_test::Require(
            requested.gpuBasedValidation != DiagnosticState::Run ||
                requested.debugLayer == DiagnosticState::Run,
            "GPU-based validation reported RUN while the debug layer is not RUN");
    } else {
        ok &= q1a_test::Require(
            requested.gpuBasedValidation == DiagnosticState::NotRequested,
            "GPU-based validation was not requested but is not NOT_REQUESTED");
    }

    if (wantDebug || wantGbv) {
        const ClearReadbackDesc desc = q1a_test::MakePatternDesc();
        ReadbackResult readback;
        const Status work = device.ClearAndReadback(desc, &readback, &error);
        q1a_test::Log("DIAGNOSTICS work_status=%s reason=%s",
                      doom3::rhi::ToString(work), error.c_str());
        ok &= q1a_test::Require(work == Status::Ok,
                                "diagnostic device could not clear and read back");
        if (work == Status::Ok) {
            ok &= q1a_test::CompareTightReadback(readback, &error);
        }
    }

    if (requested.debugLayer == DiagnosticState::Run) {
        const doom3::rhi::Diagnostics& after = device.diagnostics();
        q1a_test::Log("DIAGNOSTICS after_debug_run messages=%u errors=%u warnings=%u",
                      after.messageCount, after.errorCount, after.warningCount);
        LogCapturedMessages("after", after);
        ok &= q1a_test::Require(after.errorCount == 0,
                                "debug layer reported errors during the Q1a clear/readback");
    }
    if (requested.gpuBasedValidation == DiagnosticState::Run) {
        q1a_test::Log("DIAGNOSTICS gpu_validation=ACTIVE");
    }

    const Status shutdown = device.Shutdown();
    ok &= q1a_test::Require(shutdown == Status::Ok,
                            "diagnostics device did not shut down cleanly");
    return ok ? 0 : 1;
}

}  // namespace

int main() {
    struct Combination {
        bool debug;
        bool gpuValidation;
        const char* name;
    };
    const Combination combinations[] = {
        {false, false, "none"},
        {true, false, "debug_only"},
        {false, true, "gbv_only"},
        {true, true, "both"},
    };
    int passes = 0;
    int failures = 0;
    int skips = 0;
    for (AdapterClass adapterClass : {AdapterClass::Hardware, AdapterClass::Software}) {
        for (const Combination& combination : combinations) {
            const int result = RunDiagnosticsCase(
                adapterClass, combination.debug, combination.gpuValidation,
                combination.name);
            if (result == 0) {
                ++passes;
            } else if (result == q1a_test::kSkipExitCode) {
                ++skips;
            } else {
                ++failures;
            }
            q1a_test::Log("DIAGNOSTICS_CASE class=%s combo=%s result=%s",
                          doom3::rhi::ToString(adapterClass), combination.name,
                          result == 0 ? "PASS"
                                      : (result == q1a_test::kSkipExitCode ? "SKIPPED"
                                                                           : "FAIL"));
        }
    }
    q1a_test::Log("DIAGNOSTICS_SUMMARY pass=%d fail=%d skip=%d", passes, failures, skips);
    if (failures > 0) {
        return 1;
    }
    if (passes > 0) {
        return 0;
    }
    return q1a_test::kSkipExitCode;
}
