#include "d3d12/private_test_support.h"

#include <string>

using doom3::rhi::AdapterClass;
using doom3::rhi::ClearReadbackDesc;
using doom3::rhi::Device;
using doom3::rhi::DeviceOptions;
using doom3::rhi::ReadbackResult;
using doom3::rhi::ReleaseStatus;
using doom3::rhi::Status;
using doom3::rhi::Submission;
using doom3::rhi::d3d12::CheckState;
using doom3::rhi::d3d12::DiagnosticReport;
using doom3::rhi::d3d12::DiagnosticRequest;

namespace {

void LogMessages(const char* label, const DiagnosticReport& report) {
    q1a_test::Log("DIAGNOSTICS %s captured=%zu message_count=%u", label,
                  report.messages.size(), report.messageCount);
    for (size_t index = 0; index < report.messages.size(); ++index) {
        q1a_test::Log("DIAGNOSTICS %s message[%zu]=%s", label, index,
                      report.messages[index].c_str());
    }
}

int RunCase(AdapterClass adapterClass, bool requestDebug, bool requestGpuValidation,
            const char* combo) {
    Device device;
    DeviceOptions options;
    options.adapterClass = adapterClass;
    DiagnosticRequest request;
    request.enableDebugLayer = requestDebug;
    request.enableGpuBasedValidation = requestGpuValidation;
    std::string error;
    const Status initialize =
        doom3::rhi::d3d12::InitializeWithDiagnostics(device, options, request, &error);
    if (initialize == Status::Unsupported) {
        q1a_test::Log("DIAGNOSTICS class=%s combo=%s SKIPPED reason=%s",
                      doom3::rhi::ToString(adapterClass), combo, error.c_str());
        return q1a_test::kSkipExitCode;
    }
    bool ok = q1a_test::Require(initialize == Status::Ok, "diagnostics Initialize failed");
    if (initialize != Status::Ok) {
        device.Shutdown();
        return 1;
    }

    const DiagnosticReport requested = doom3::rhi::d3d12::ReadDiagnostics(device);
    q1a_test::Log("DIAGNOSTICS class=%s combo=%s debug_requested=%s "
                  "gpu_validation_requested=%s debug=%s gpu_validation=%s "
                  "messages=%u errors=%u warnings=%u",
                  doom3::rhi::ToString(adapterClass), combo,
                  requestDebug ? "true" : "false",
                  requestGpuValidation ? "true" : "false",
                  doom3::rhi::d3d12::ToString(requested.debugLayer),
                  doom3::rhi::d3d12::ToString(requested.gpuBasedValidation),
                  requested.messageCount, requested.errorCount, requested.warningCount);
    q1a_test::Log("DIAGNOSTICS debug_detail=%s", requested.debugLayerDetail.c_str());
    q1a_test::Log("DIAGNOSTICS gpu_validation_detail=%s",
                  requested.gpuBasedValidationDetail.c_str());
    LogMessages("requested", requested);

    if (requestDebug) {
        ok &= q1a_test::Require(
            q1a_private::AllowedWhenRequested(requested.debugLayer),
            "debug layer was requested but is NOT_RUN");
    } else if (requestGpuValidation) {
        ok &= q1a_test::Require(
            requested.debugLayer == CheckState::NotRequested ||
                requested.debugLayer == CheckState::Run ||
                requested.debugLayer == CheckState::Skipped,
            "GPU-based validation alone left the debug layer unattempted");
    } else {
        ok &= q1a_test::Require(requested.debugLayer == CheckState::NotRequested,
                                "debug layer was not requested but is not NOT_REQUESTED");
    }

    if (requestGpuValidation) {
        ok &= q1a_test::Require(
            q1a_private::AllowedWhenRequested(requested.gpuBasedValidation),
            "GPU-based validation was requested but is NOT_RUN");
        ok &= q1a_test::Require(
            requested.gpuBasedValidation != CheckState::Run ||
                requested.debugLayer == CheckState::Run,
            "GPU-based validation reported RUN while the debug layer is not RUN");
    } else {
        ok &= q1a_test::Require(
            requested.gpuBasedValidation == CheckState::NotRequested,
            "GPU-based validation was not requested but is not NOT_REQUESTED");
    }

    const ClearReadbackDesc desc = q1a_test::MakePatternDesc();
    Submission submission;
    Status status = device.SubmitClearReadback(desc, &submission, &error);
    q1a_test::Log("DIAGNOSTICS work_submit=%s reason=%s",
                  doom3::rhi::ToString(status), error.c_str());
    ok &= q1a_test::Require(status == Status::Ok,
                            "diagnostic device could not submit the clear");
    if (status == Status::Ok) {
        ok &= q1a_test::Require(q1a_test::WaitComplete(device, submission),
                                "diagnostic completion token was not reached");
        ReadbackResult readback;
        status = device.Readback(submission, &readback, &error);
        q1a_test::Log("DIAGNOSTICS work_readback=%s reason=%s",
                      doom3::rhi::ToString(status), error.c_str());
        ok &= q1a_test::Require(status == Status::Ok,
                                "diagnostic device could not read the result back");
        if (status == Status::Ok) {
            ok &= q1a_test::CompareRows(readback, q1a_test::kHeight, &error);
        }
        ok &= q1a_test::Require(
            device.TryRelease(submission, &error) == ReleaseStatus::Released,
            "diagnostic submission was not released");
    }

    if (requested.debugLayer == CheckState::Run) {
        const DiagnosticReport after = doom3::rhi::d3d12::ReadDiagnostics(device);
        q1a_test::Log("DIAGNOSTICS after messages=%u errors=%u warnings=%u",
                      after.messageCount, after.errorCount, after.warningCount);
        LogMessages("after", after);
        ok &= q1a_test::Require(after.errorCount == 0,
                                "debug layer reported errors during the Q1a work");
    } else if (requestDebug || requestGpuValidation) {
        q1a_test::Log("DIAGNOSTICS validation=SKIPPED debug=%s gpu_validation=%s",
                      doom3::rhi::d3d12::ToString(requested.debugLayer),
                      doom3::rhi::d3d12::ToString(requested.gpuBasedValidation));
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
        {false, true, "gpu_validation_only"},
        {true, true, "both"},
    };
    int passes = 0;
    int failures = 0;
    int skips = 0;
    for (AdapterClass adapterClass : {AdapterClass::Hardware, AdapterClass::Software}) {
        for (const Combination& combination : combinations) {
            const int result = RunCase(adapterClass, combination.debug,
                                       combination.gpuValidation, combination.name);
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
    q1a_test::Log("DIAGNOSTICS_SUMMARY pass=%d fail=%d skip=%d", passes, failures,
                  skips);
    if (failures > 0) {
        return 1;
    }
    if (passes > 0) {
        return 0;
    }
    return q1a_test::kSkipExitCode;
}
