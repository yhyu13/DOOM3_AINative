#include "d3d12/private_test_support.h"

#include <string>

using doom3::rhi::ClearReadbackDesc;
using doom3::rhi::Device;
using doom3::rhi::ReadbackResult;
using doom3::rhi::ReleaseStatus;
using doom3::rhi::Status;
using doom3::rhi::Submission;
using doom3::rhi::d3d12::DiagnosticRequest;
using doom3::rhi::d3d12::FenceObservation;

namespace {

int RunGatedOutstanding(Device& device) {
    bool ok = true;
    std::string error;
    const ClearReadbackDesc desc = q1a_test::MakePatternDesc();

    Submission submission;
    Status status =
        doom3::rhi::d3d12::SubmitWithGate(device, desc, &submission, &error);
    q1a_test::Log("PRIVATE_LIFETIME submit=%s reason=%s", doom3::rhi::ToString(status),
                  error.c_str());
    ok &= q1a_test::Require(status == Status::Ok, "gated submit failed");
    if (status != Status::Ok) {
        return 1;
    }
    q1a_test::PrintSubmission("gated", submission);

    const bool completePublic = device.IsComplete(submission);
    const FenceObservation observation =
        doom3::rhi::d3d12::ObserveFence(device, submission);
    q1a_test::Log("PRIVATE_LIFETIME completed_value=%llu signal_value=%llu "
                  "device_removed=%s public_is_complete=%s",
                  static_cast<unsigned long long>(observation.completedValue),
                  static_cast<unsigned long long>(observation.signalValue),
                  observation.deviceRemoved ? "true" : "false",
                  completePublic ? "true" : "false");
    ok &= q1a_test::Require(!observation.deviceRemoved,
                            "the device was removed before the in-flight observation");
    ok &= q1a_test::Require(!completePublic,
                            "public completion token reported complete before the gate");
    ok &= q1a_test::Require(
        observation.completedValue < observation.signalValue,
        "GetCompletedValue() is not below signalValue at the in-flight release attempt");

    const ReleaseStatus release = device.TryRelease(submission, &error);
    q1a_test::Log("PRIVATE_LIFETIME in_flight_release=%s reason=%s",
                  doom3::rhi::ToString(release), error.c_str());
    ok &= q1a_test::Require(release == ReleaseStatus::RefusedOutstanding,
                            "release was not refused while the completion was outstanding");

    status = doom3::rhi::d3d12::ReleaseGate(device, submission, &error);
    q1a_test::Log("PRIVATE_LIFETIME release_gate=%s reason=%s",
                  doom3::rhi::ToString(status), error.c_str());
    ok &= q1a_test::Require(status == Status::Ok, "gate release failed");
    ok &= q1a_test::Require(q1a_test::WaitComplete(device, submission),
                            "completion token was not reached after the gate release");

    ReadbackResult readback;
    status = device.Readback(submission, &readback, &error);
    q1a_test::Log("PRIVATE_LIFETIME readback=%s reason=%s",
                  doom3::rhi::ToString(status), error.c_str());
    ok &= q1a_test::Require(status == Status::Ok, "Readback failed");
    if (status == Status::Ok) {
        ok &= q1a_test::CompareRows(readback, q1a_test::kHeight, &error);
    }
    ok &= q1a_test::Require(
        device.TryRelease(submission, &error) == ReleaseStatus::Released,
        "completed submission was not released");
    return ok ? 0 : 1;
}

// Negative control: a wait-idle-before-release shortcut loses the in-flight
// observation. If this ever stops being true, the main assertion above is not
// discriminating and this control fails.
int RunWaitIdleControl(Device& device) {
    bool ok = true;
    std::string error;
    const ClearReadbackDesc desc = q1a_test::MakePatternDesc();
    Submission submission;
    Status status =
        doom3::rhi::d3d12::SubmitWithGate(device, desc, &submission, &error);
    ok &= q1a_test::Require(status == Status::Ok, "control gated submit failed");
    if (status != Status::Ok) {
        return 1;
    }
    status = doom3::rhi::d3d12::ReleaseGate(device, submission, &error);
    ok &= q1a_test::Require(status == Status::Ok, "control gate release failed");
    ok &= q1a_test::Require(q1a_test::WaitComplete(device, submission),
                            "control completion token was not reached");
    const bool completeBeforeRelease = device.IsComplete(submission);
    const ReleaseStatus release = device.TryRelease(submission, &error);
    q1a_test::Log("WAIT_IDLE_CONTROL complete_before_release=%s release=%s "
                  "would_observe_outstanding=false",
                  completeBeforeRelease ? "true" : "false",
                  doom3::rhi::ToString(release));
    ok &= q1a_test::Require(
        completeBeforeRelease && release == ReleaseStatus::Released,
        "wait-idle-before-release did not lose the outstanding observation");
    return ok ? 0 : 1;
}

}  // namespace

int main() {
    const DiagnosticRequest noDiagnostics;
    const int main = q1a_private::ForEachAdapterPrivate(noDiagnostics,
                                                        RunGatedOutstanding);
    const int control = q1a_private::ForEachAdapterPrivate(noDiagnostics,
                                                           RunWaitIdleControl);
    if (main == 1 || control == 1) {
        return 1;
    }
    if (main == q1a_test::kSkipExitCode && control == q1a_test::kSkipExitCode) {
        return q1a_test::kSkipExitCode;
    }
    return 0;
}
