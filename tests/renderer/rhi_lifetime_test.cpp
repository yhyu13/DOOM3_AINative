#include "q1a_test_support.h"

#include <chrono>
#include <string>
#include <thread>

using doom3::rhi::ClearReadbackDesc;
using doom3::rhi::Device;
using doom3::rhi::DeviceOptions;
using doom3::rhi::ReadbackResult;
using doom3::rhi::ReleaseStatus;
using doom3::rhi::Status;
using doom3::rhi::Submission;
using doom3::rhi::SubmissionOptions;

namespace {

// Releasing a later gate must not open an earlier gate. The bounded observation
// interval lets an incorrectly signalled GPU queue make progress; teardown always
// opens the first gate before waiting, including when an assertion fails.
int RunReverseGate(Device& device) {
    const ClearReadbackDesc desc = q1a_test::MakePatternDesc();
    SubmissionOptions options;
    options.waitForCpuGate = true;
    Submission first, second;
    std::string error;
    bool ok = true;
    Status status = device.SubmitClearReadback(desc, options, &first, &error);
    ok &= q1a_test::Require(status == Status::Ok, "first gated submit failed: " + error);
    if (status != Status::Ok) {
        return 1;
    }
    status = device.SubmitClearReadback(desc, options, &second, &error);
    ok &= q1a_test::Require(status == Status::Ok, "second gated submit failed: " + error);
    if (status != Status::Ok) {
        return 1;  // ForEachAdapter shuts the device down, opening the first gate.
    }
    status = device.ReleaseGate(second, &error);
    ok &= q1a_test::Require(status == Status::Ok, "release second gate failed: " + error);
    if (status == Status::Ok) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        while (!device.IsCompleted(first) && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        const bool firstCompleted = device.IsCompleted(first);
        const bool secondCompleted = device.IsCompleted(second);
        ReadbackResult output;
        const Status read = device.Readback(first, &output, &error);
        q1a_test::Log("REVERSE_GATE first_completed=%s second_completed=%s first_readback=%s",
                      firstCompleted ? "true" : "false", secondCompleted ? "true" : "false",
                      doom3::rhi::ToString(read));
        ok &= q1a_test::Require(!firstCompleted, "second gate opened first submission");
        ok &= q1a_test::Require(!secondCompleted, "second submission passed the closed first gate");
        ok &= q1a_test::Require(read == Status::InvalidArgument,
                                "readback succeeded while first gate remained closed");
        const ReleaseStatus release = device.TryRelease(first, &error);
        ok &= q1a_test::Require(release == ReleaseStatus::RefusedOutstanding,
                                "first outstanding submission was released");
    }
    status = device.ReleaseGate(first, &error);
    ok &= q1a_test::Require(status == Status::Ok, "release first gate failed: " + error);
    if (status == Status::Ok) {
        status = device.Wait(first, &error);
        ok &= q1a_test::Require(status == Status::Ok, "wait first failed: " + error);
        if (status == Status::Ok) {
            ok &= q1a_test::Require(device.TryRelease(first, &error) == ReleaseStatus::Released,
                                    "completed first submission was not released");
        }
        status = device.Wait(second, &error);
        ok &= q1a_test::Require(status == Status::Ok, "wait second failed: " + error);
        if (status == Status::Ok) {
            ok &= q1a_test::Require(device.TryRelease(second, &error) == ReleaseStatus::Released,
                                    "completed second submission was not released");
        }
    }
    return ok ? 0 : 1;
}

// Check every operation that accepts a submission, including the non-mutating
// queries. Invalid handles must not be able to open or retire a live submission.
bool RejectOtherHandle(Device& device, const Submission& other, const char* label) {
    std::string error;
    bool ok = true;
    Status gate = device.ReleaseGate(other, &error);
    Status wait = device.Wait(other, &error);
    ReadbackResult readback;
    Status read = device.Readback(other, &readback, &error);
    const uint64_t completed = device.CompletedSerial(other);
    const bool isCompleted = device.IsCompleted(other);
    ReleaseStatus release = device.TryRelease(other, &error);
    q1a_test::Log("HANDLE %s gate=%s wait=%s readback=%s completed=%llu is_completed=%s release=%s",
                  label, doom3::rhi::ToString(gate), doom3::rhi::ToString(wait),
                  doom3::rhi::ToString(read), static_cast<unsigned long long>(completed),
                  isCompleted ? "true" : "false", doom3::rhi::ToString(release));
    ok &= q1a_test::Require(gate == Status::InvalidArgument, std::string(label) + " accepted by ReleaseGate");
    ok &= q1a_test::Require(wait == Status::InvalidArgument, std::string(label) + " accepted by Wait");
    ok &= q1a_test::Require(read == Status::InvalidArgument, std::string(label) + " accepted by Readback");
    ok &= q1a_test::Require(completed == 0, std::string(label) + " accepted by CompletedSerial");
    ok &= q1a_test::Require(!isCompleted, std::string(label) + " accepted by IsCompleted");
    ok &= q1a_test::Require(release == ReleaseStatus::InvalidSubmission, std::string(label) + " accepted by TryRelease");
    return ok;
}

int RunHandleIdentity(Device& device) {
    const ClearReadbackDesc desc = q1a_test::MakePatternDesc();
    SubmissionOptions gated;
    gated.waitForCpuGate = true;
    DeviceOptions options;
    options.adapterClass = device.adapter().adapterClass;
    std::string error;
    bool ok = true;
    Submission current, foreign;
    Status status = device.SubmitClearReadback(desc, gated, &current, &error);
    if (!q1a_test::Require(status == Status::Ok, "identity current submit failed: " + error)) {
        return 1;
    }
    Device other;
    status = other.Initialize(options, &error);
    if (!q1a_test::Require(status == Status::Ok, "identity foreign device initialization failed: " + error)) {
        return 1;
    }
    status = other.SubmitClearReadback(desc, gated, &foreign, &error);
    if (!q1a_test::Require(status == Status::Ok, "identity foreign submit failed: " + error)) {
        return 1;
    }
    q1a_test::PrintSubmission("identity-current", current);
    q1a_test::PrintSubmission("identity-foreign", foreign);
    ok &= RejectOtherHandle(device, foreign, "foreign");
    status = device.ReleaseGate(current, &error);
    if (status == Status::Ok) {
        status = device.Wait(current, &error);
        if (status == Status::Ok) {
            device.TryRelease(current, &error);
        }
    }
    ok &= q1a_test::Require(other.Shutdown() == Status::Ok, "foreign cleanup failed");
    ok &= q1a_test::Require(device.Shutdown() == Status::Ok, "pre-reinitialize cleanup failed");
    status = device.Initialize(options, &error);
    if (!q1a_test::Require(status == Status::Ok, "identity reinitialize failed: " + error)) {
        return 1;
    }
    Submission fresh;
    status = device.SubmitClearReadback(desc, gated, &fresh, &error);
    if (!q1a_test::Require(status == Status::Ok, "identity fresh submit failed: " + error)) {
        return 1;
    }
    q1a_test::PrintSubmission("identity-old", current);
    q1a_test::PrintSubmission("identity-fresh", fresh);
    ok &= RejectOtherHandle(device, current, "stale");
    status = device.ReleaseGate(fresh, &error);
    if (status == Status::Ok) {
        status = device.Wait(fresh, &error);
        if (status == Status::Ok) {
            device.TryRelease(fresh, &error);
        }
    }
    return ok ? 0 : 1;
}

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

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "reverse_gate") {
        return q1a_test::ForEachAdapter(RunReverseGate);
    }
    if (argc == 2 && std::string(argv[1]) == "handle_identity") {
        return q1a_test::ForEachAdapter(RunHandleIdentity);
    }
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
