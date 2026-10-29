#include "d3d12/private_test_support.h"

#include <string>

using doom3::rhi::ClearReadbackDesc;
using doom3::rhi::Device;
using doom3::rhi::ReadbackResult;
using doom3::rhi::ReleaseStatus;
using doom3::rhi::Status;
using doom3::rhi::Submission;
using doom3::rhi::d3d12::DiagnosticRequest;
using doom3::rhi::d3d12::RawReadback;

namespace {

Submission SubmitAndWait(Device& device, bool* ok, std::string* error) {
    Submission submission;
    const ClearReadbackDesc desc = q1a_test::MakePatternDesc();
    const Status status = device.SubmitClearReadback(desc, &submission, error);
    *ok &= q1a_test::Require(status == Status::Ok, "SubmitClearReadback failed");
    if (status != Status::Ok) {
        return submission;
    }
    *ok &= q1a_test::Require(q1a_test::WaitComplete(device, submission),
                             "completion token was not reached");
    return submission;
}

int RunStateJournal(Device& device) {
    bool ok = true;
    std::string error;
    const uint32_t expectedFacts =
        static_cast<uint32_t>(doom3::rhi::AdapterFact::VendorId) |
        static_cast<uint32_t>(doom3::rhi::AdapterFact::DeviceId) |
        static_cast<uint32_t>(doom3::rhi::AdapterFact::DedicatedMemory);
    q1a_test::Log("PRIVATE_ADAPTER reported_facts=0x%x expected=0x%x",
                  device.adapter().reportedFacts, expectedFacts);
    ok &= q1a_test::Require(
        (device.adapter().reportedFacts & expectedFacts) == expectedFacts,
        "the native adapter did not report its identity and memory facts");
    const Submission submission = SubmitAndWait(device, &ok, &error);
    if (!ok) {
        return 1;
    }
    const auto journal = doom3::rhi::d3d12::ReadJournal(device, submission);
    q1a_private::LogJournal(journal);
    ok &= q1a_private::JournalMatches(journal);
    ok &= q1a_test::Require(
        device.TryRelease(submission, &error) == ReleaseStatus::Released,
        "the completed submission was not released");
    return ok ? 0 : 1;
}

int RunStagingPitch(Device& device) {
    bool ok = true;
    std::string error;
    const Submission submission = SubmitAndWait(device, &ok, &error);
    if (!ok) {
        return 1;
    }
    const RawReadback raw =
        doom3::rhi::d3d12::ReadRawReadback(device, submission, &error);
    q1a_test::Log("STAGING pitch=%u tight=%u bytes=%zu reason=%s", raw.stagingPitch,
                  raw.tightRowStride, raw.staging.size(), error.c_str());
    ok &= q1a_test::Require(!raw.staging.empty(), "raw private readback was empty");
    if (!raw.staging.empty()) {
        ok &= q1a_private::WalkRawRowsMatch(raw, &error);
        const q1a_test::ControlResult control =
            q1a_private::StagingPitchControl(raw);
        q1a_test::PrintControl("staging_pitch", control);
        ok &= q1a_test::Require(control.applicable,
                                "the staging-pitch control was not applicable");
        ok &= q1a_test::Require(control.detected,
                                "the staging-pitch control was not detected");
    }
    ok &= q1a_test::Require(
        device.TryRelease(submission, &error) == ReleaseStatus::Released,
        "the completed submission was not released");
    return ok ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "state_journal";
    const DiagnosticRequest noDiagnostics;
    if (mode == "state_journal") {
        return q1a_private::ForEachAdapterPrivate(noDiagnostics, RunStateJournal);
    }
    if (mode == "staging_pitch") {
        return q1a_private::ForEachAdapterPrivate(noDiagnostics, RunStagingPitch);
    }
    q1a_test::Log("unknown rhi_d3d12_test mode: %s", mode.c_str());
    return 2;
}
