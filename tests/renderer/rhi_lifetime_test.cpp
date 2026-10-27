#include "q1a_test_support.h"

#include <string>

using doom3::rhi::ClearReadbackDesc;
using doom3::rhi::Device;
using doom3::rhi::DeviceOptions;
using doom3::rhi::ReadbackResult;
using doom3::rhi::ReleaseStatus;
using doom3::rhi::Status;
using doom3::rhi::Submission;
using doom3::rhi::SubmissionOptions;

namespace {

int RunLifetime(Device& device) {
    const ClearReadbackDesc desc = q1a_test::MakePatternDesc();
    bool ok = true;
    std::string error;

    SubmissionOptions gatedOptions;
    gatedOptions.waitForCpuGate = true;
    Submission gated;
    Status status = device.SubmitClearReadback(desc, gatedOptions, &gated, &error);
    q1a_test::Log("LIFETIME gated submit_status=%s reason=%s", doom3::rhi::ToString(status),
                  error.c_str());
    ok &= q1a_test::Require(status == Status::Ok, "gated SubmitClearReadback failed");
    if (status != Status::Ok) {
        return 1;
    }
    q1a_test::PrintSubmission("gated", gated);
    ok &= q1a_test::Require(gated.gated, "gated submission is not marked gated");
    ok &= q1a_test::Require(!device.IsCompleted(gated),
                            "gated submission completed before ReleaseGate");

    Submission ungated;
    status = device.SubmitClearReadback(desc, SubmissionOptions{}, &ungated, &error);
    q1a_test::Log("LIFETIME ungated submit_status=%s reason=%s", doom3::rhi::ToString(status),
                  error.c_str());
    ok &= q1a_test::Require(status == Status::Ok, "second SubmitClearReadback failed");
    if (status != Status::Ok) {
        return 1;
    }
    q1a_test::PrintSubmission("ungated", ungated);
    ok &= q1a_test::Require(ungated.id != gated.id, "two submissions reused an id");
    ok &= q1a_test::Require(ungated.completionSerial != gated.completionSerial,
                            "two submissions reused a completion serial");

    const uint64_t completedBefore = device.CompletedSerial(gated);
    q1a_test::Log("LIFETIME gated_serial=%llu completed_before_release=%llu",
                  static_cast<unsigned long long>(gated.completionSerial),
                  static_cast<unsigned long long>(completedBefore));
    ok &= q1a_test::Require(completedBefore < gated.completionSerial,
                            "completion serial is already reached while the gate is closed");

    ReleaseStatus release = device.TryRelease(gated, &error);
    q1a_test::Log("LIFETIME release_gated_outstanding=%s reason=%s",
                  doom3::rhi::ToString(release), error.c_str());
    ok &= q1a_test::Require(release == ReleaseStatus::RefusedOutstanding,
                            "gated release was not refused while the serial was outstanding");

    release = device.TryRelease(ungated, &error);
    q1a_test::Log("LIFETIME release_ungated_outstanding=%s reason=%s",
                  doom3::rhi::ToString(release), error.c_str());
    ok &= q1a_test::Require(release == ReleaseStatus::RefusedOutstanding,
                            "second release was not refused while queued behind the gate");

    status = device.Wait(gated, &error);
    q1a_test::Log("LIFETIME wait_before_gate_status=%s reason=%s",
                  doom3::rhi::ToString(status), error.c_str());
    ok &= q1a_test::Require(status == Status::InvalidArgument,
                            "Wait did not refuse a gated submission with a closed gate");

    status = device.ReleaseGate(gated, &error);
    q1a_test::Log("LIFETIME release_gate_status=%s reason=%s",
                  doom3::rhi::ToString(status), error.c_str());
    ok &= q1a_test::Require(status == Status::Ok, "ReleaseGate failed");
    ok &= q1a_test::Require(error.empty(),
                            "successful ReleaseGate left stale error text");

    status = device.Wait(gated, &error);
    q1a_test::Log("LIFETIME wait_gated_status=%s reason=%s", doom3::rhi::ToString(status),
                  error.c_str());
    ok &= q1a_test::Require(status == Status::Ok, "Wait after ReleaseGate failed");
    ok &= q1a_test::Require(error.empty(), "successful Wait left stale error text");
    ok &= q1a_test::Require(device.IsCompleted(gated),
                            "gated submission is not complete after Wait");

    ReadbackResult gatedReadback;
    status = device.Readback(gated, &gatedReadback, &error);
    ok &= q1a_test::Require(status == Status::Ok, "gated Readback failed");
    if (status == Status::Ok) {
        ok &= q1a_test::CompareTightReadback(gatedReadback, &error);
    }

    release = device.TryRelease(gated, &error);
    q1a_test::Log("LIFETIME release_gated_complete=%s reason=%s",
                  doom3::rhi::ToString(release), error.c_str());
    ok &= q1a_test::Require(release == ReleaseStatus::Released,
                            "gated release did not release a completed submission");

    status = device.Wait(ungated, &error);
    q1a_test::Log("LIFETIME wait_ungated_status=%s reason=%s", doom3::rhi::ToString(status),
                  error.c_str());
    ok &= q1a_test::Require(status == Status::Ok, "Wait on the second submission failed");
    ok &= q1a_test::Require(error.empty(), "successful Wait left stale error text");

    release = device.TryRelease(ungated, &error);
    q1a_test::Log("LIFETIME release_ungated_complete=%s reason=%s",
                  doom3::rhi::ToString(release), error.c_str());
    ok &= q1a_test::Require(release == ReleaseStatus::Released,
                            "second release did not release a completed submission");

    Submission bogus;
    bogus.id = 0xFFFFFFFFu;
    bogus.completionSerial = 0xFFFFFFFFFFFFFFFFull;
    release = device.TryRelease(bogus, &error);
    q1a_test::Log("LIFETIME release_bogus=%s reason=%s", doom3::rhi::ToString(release),
                  error.c_str());
    ok &= q1a_test::Require(release == ReleaseStatus::InvalidSubmission,
                            "TryRelease accepted an unknown submission");

    return ok ? 0 : 1;
}

int RunShutdownWithOutstanding(Device& device) {
    const ClearReadbackDesc desc = q1a_test::MakePatternDesc();
    SubmissionOptions gatedOptions;
    gatedOptions.waitForCpuGate = true;
    Submission gated;
    std::string error;
    const Status submit = device.SubmitClearReadback(desc, gatedOptions, &gated, &error);
    q1a_test::Log("SHUTDOWN submit_status=%s serial=%llu gated=%s reason=%s",
                  doom3::rhi::ToString(submit),
                  static_cast<unsigned long long>(gated.completionSerial),
                  gated.gated ? "true" : "false", error.c_str());
    bool ok = q1a_test::Require(submit == Status::Ok,
                                "shutdown test could not submit gated work");
    if (submit != Status::Ok) {
        return 1;
    }
    ok &= q1a_test::Require(!device.IsCompleted(gated),
                            "gated submission completed before shutdown");

    const Status shutdown = device.Shutdown();
    q1a_test::Log("SHUTDOWN status=%s", doom3::rhi::ToString(shutdown));
    ok &= q1a_test::Require(shutdown == Status::Ok,
                            "Shutdown with an unreleased gated submission failed");
    ok &= q1a_test::Require(!device.IsInitialized(),
                            "device reports initialized after Shutdown");
    const Status second = device.Shutdown();
    q1a_test::Log("SHUTDOWN second_status=%s", doom3::rhi::ToString(second));
    ok &= q1a_test::Require(second == Status::Ok, "second Shutdown failed");
    return ok ? 0 : 1;
}

int RunIncompleteInitShutdown() {
    bool ok = true;
    for (doom3::rhi::AdapterClass adapterClass :
         {doom3::rhi::AdapterClass::Hardware, doom3::rhi::AdapterClass::Software}) {
        Device device;
        DeviceOptions options;
        options.adapterClass = adapterClass;
        options.adapterIndex = 9999;
        std::string error;
        const Status status = device.Initialize(options, &error);
        q1a_test::Log("INCOMPLETE_INIT class=%s status=%s reason=%s",
                      doom3::rhi::ToString(adapterClass), doom3::rhi::ToString(status),
                      error.c_str());
        ok &= q1a_test::Require(status == Status::Unsupported,
                                "out-of-range adapter index did not return Unsupported");
        ok &= q1a_test::Require(!device.IsInitialized(),
                                "device reports initialized after a failed Initialize");
        const Status firstShutdown = device.Shutdown();
        const Status secondShutdown = device.Shutdown();
        q1a_test::Log("INCOMPLETE_INIT class=%s first_shutdown=%s second_shutdown=%s",
                      doom3::rhi::ToString(adapterClass),
                      doom3::rhi::ToString(firstShutdown),
                      doom3::rhi::ToString(secondShutdown));
        ok &= q1a_test::Require(firstShutdown == Status::Ok && secondShutdown == Status::Ok,
                                "Shutdown after a failed Initialize was not clean");
        ok &= q1a_test::Require(!device.IsInitialized(),
                                "device reports initialized after Shutdown");
    }
    return ok ? 0 : 1;
}

}  // namespace

int main() {
    const int lifetime = q1a_test::ForEachAdapter(RunLifetime);
    const int outstanding = q1a_test::ForEachAdapter(RunShutdownWithOutstanding);
    const int incomplete = RunIncompleteInitShutdown();
    if (lifetime == 1 || outstanding == 1 || incomplete == 1) {
        return 1;
    }
    if (lifetime == q1a_test::kSkipExitCode && outstanding == q1a_test::kSkipExitCode) {
        return q1a_test::kSkipExitCode;
    }
    return 0;
}
