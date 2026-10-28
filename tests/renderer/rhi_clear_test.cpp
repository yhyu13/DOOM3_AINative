#include "q1a_test_support.h"

#include <string>

using doom3::rhi::ClearReadbackDesc;
using doom3::rhi::Device;
using doom3::rhi::ReadbackResult;
using doom3::rhi::ReleaseStatus;
using doom3::rhi::Status;
using doom3::rhi::Submission;

namespace {

int RunClearReadback(Device& device) {
    bool ok = true;
    std::string error;

    ok &= q1a_test::Require(q1a_test::VerifyExpectedData(&error), error);
    ok &= q1a_test::Require(q1a_test::TolerancePolicyControl(&error), error);

    const ClearReadbackDesc desc = q1a_test::MakePatternDesc();
    Submission submission;
    const Status submit = device.SubmitClearReadback(desc, &submission, &error);
    q1a_test::Log("CLEAR submit_status=%s reason=%s", doom3::rhi::ToString(submit),
                  error.c_str());
    ok &= q1a_test::Require(submit == Status::Ok, "SubmitClearReadback failed");
    if (submit != Status::Ok) {
        return 1;
    }
    q1a_test::PrintSubmission("clear", submission);
    ok &= q1a_test::Require(submission.id != 0, "submission id is zero");
    ok &= q1a_test::Require(submission.completionToken != 0,
                            "completion token is zero");
    q1a_test::Log("CLEAR completed_before_poll=%s",
                  device.IsComplete(submission) ? "true" : "false");

    const bool complete = q1a_test::WaitComplete(device, submission);
    q1a_test::Log("CLEAR completed_after_poll=%s", complete ? "true" : "false");
    ok &= q1a_test::Require(complete, "the completion token was not reached in time");

    ReadbackResult readback;
    const Status read = device.Readback(submission, &readback, &error);
    q1a_test::Log("CLEAR readback_status=%s reason=%s row_stride=%u bytes=%zu",
                  doom3::rhi::ToString(read), error.c_str(), readback.rowStride,
                  readback.pixels.size());
    ok &= q1a_test::Require(read == Status::Ok, "Readback failed");
    if (read == Status::Ok) {
        ok &= q1a_test::CompareRows(readback, q1a_test::kHeight, &error);
        ok &= q1a_test::Require(readback.adapter.name == device.adapter().name,
                                "readback adapter does not match the device adapter");

        const std::string fileName =
            std::string("q1a-clear-") +
            doom3::rhi::ToString(device.adapter().adapterClass) + ".bmp";
        const std::string path = q1a_test::EvidencePath(fileName);
        const bool wrote = q1a_test::WriteBmp(path, readback, 16);
        q1a_test::Log("SCREENSHOT path=%s wrote=%s", path.c_str(),
                      wrote ? "true" : "false");
        ok &= q1a_test::Require(wrote, "failed to write the readback image");
    }

    const ReleaseStatus release = device.TryRelease(submission, &error);
    q1a_test::Log("CLEAR release_status=%s reason=%s", doom3::rhi::ToString(release),
                  error.c_str());
    ok &= q1a_test::Require(release == ReleaseStatus::Released,
                            "TryRelease did not release a completed submission");

    // A released token is invalid for every public operation.
    const bool stillComplete = device.IsComplete(submission);
    ReadbackResult afterRelease;
    const Status afterRead = device.Readback(submission, &afterRelease, &error);
    const ReleaseStatus afterReleaseStatus = device.TryRelease(submission, &error);
    q1a_test::Log("CLEAR after_release complete=%s readback=%s release=%s",
                  stillComplete ? "true" : "false", doom3::rhi::ToString(afterRead),
                  doom3::rhi::ToString(afterReleaseStatus));
    ok &= q1a_test::Require(!stillComplete, "a released token still reports completion");
    ok &= q1a_test::Require(afterRead == Status::InvalidArgument,
                            "Readback accepted a released token");
    ok &= q1a_test::Require(afterReleaseStatus == ReleaseStatus::InvalidSubmission,
                            "TryRelease accepted a released token");
    return ok ? 0 : 1;
}

int RunNegativeControls(Device& device) {
    std::string error;
    bool ok = q1a_test::Require(q1a_test::VerifyExpectedData(&error), error);

    const ClearReadbackDesc desc = q1a_test::MakePatternDesc();
    Submission submission;
    Status status = device.SubmitClearReadback(desc, &submission, &error);
    ok &= q1a_test::Require(status == Status::Ok, "SubmitClearReadback failed");
    if (status != Status::Ok) {
        return 1;
    }
    q1a_test::PrintSubmission("negative", submission);
    ok &= q1a_test::Require(q1a_test::WaitComplete(device, submission),
                            "the completion token was not reached in time");

    ReadbackResult readback;
    status = device.Readback(submission, &readback, &error);
    ok &= q1a_test::Require(status == Status::Ok, "Readback failed");
    if (status == Status::Ok) {
        ok &= q1a_test::CompareRows(readback, q1a_test::kHeight, &error);

        const q1a_test::ControlResult offByOne =
            q1a_test::OffByOneRowControl(readback);
        const q1a_test::ControlResult repeatRow0 =
            q1a_test::RepeatRowZeroControl(readback);
        const q1a_test::ControlResult shortRow =
            q1a_test::PackedRowLengthControl(readback);
        q1a_test::PrintControl("off_by_one_row", offByOne);
        q1a_test::PrintControl("repeat_row_0", repeatRow0);
        q1a_test::PrintControl("packed_row_length_misread", shortRow);

        const q1a_test::ControlResult controls[] = {offByOne, repeatRow0, shortRow};
        int applicable = 0;
        bool allDetected = true;
        for (const auto& control : controls) {
            if (control.applicable) {
                ++applicable;
                if (!control.detected) {
                    allDetected = false;
                }
            }
        }
        q1a_test::Log("NEGATIVE_CONTROL_SUMMARY applicable=%d all_detected=%s", applicable,
                      allDetected ? "true" : "false");
        ok &= q1a_test::Require(applicable == 3,
                                "a required public negative control was not applicable");
        ok &= q1a_test::Require(allDetected,
                                "an applicable public negative control was not detected");
    }

    const ReleaseStatus release = device.TryRelease(submission, &error);
    ok &= q1a_test::Require(release == ReleaseStatus::Released,
                            "TryRelease did not release a completed submission");
    return ok ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "clear_readback";
    if (mode == "clear_readback") {
        return q1a_test::ForEachAdapter(RunClearReadback);
    }
    if (mode == "negative_controls") {
        return q1a_test::ForEachAdapter(RunNegativeControls);
    }
    q1a_test::Log("unknown rhi_clear_test mode: %s", mode.c_str());
    return 2;
}
