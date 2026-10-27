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
    const ClearReadbackDesc desc = q1a_test::MakePatternDesc();
    bool ok = true;
    uint64_t firstSerial = 0;
    uint64_t secondSerial = 0;

    for (int index = 0; index < 2; ++index) {
        Submission submission;
        std::string error;
        const Status submit =
            device.SubmitClearReadback(desc, doom3::rhi::SubmissionOptions{}, &submission, &error);
        q1a_test::Log("CLEAR submission=%d submit_status=%s reason=%s", index,
                      doom3::rhi::ToString(submit), error.c_str());
        ok &= q1a_test::Require(submit == Status::Ok, "SubmitClearReadback failed");
        if (submit != Status::Ok) {
            return 1;
        }
        q1a_test::PrintSubmission("clear", submission);
        ok &= q1a_test::Require(submission.id != 0, "submission id is zero");
        ok &= q1a_test::Require(submission.completionSerial != 0,
                                "completion serial is zero");
        ok &= q1a_test::Require(!submission.gated, "clear submission unexpectedly gated");
        q1a_test::Log("CLEAR submission=%d completed_before_wait=%s", index,
                      device.IsCompleted(submission) ? "true" : "false");

        const Status wait = device.Wait(submission, &error);
        q1a_test::Log("CLEAR submission=%d wait_status=%s reason=%s", index,
                      doom3::rhi::ToString(wait), error.c_str());
        ok &= q1a_test::Require(wait == Status::Ok, "Wait failed");
        ok &= q1a_test::Require(device.IsCompleted(submission),
                                "submission is not complete after Wait");

        ReadbackResult readback;
        const Status read = device.Readback(submission, &readback, &error);
        q1a_test::Log("CLEAR submission=%d readback_status=%s reason=%s row_pitch=%u tight=%u staging_bytes=%zu",
                      index, doom3::rhi::ToString(read), error.c_str(), readback.rowPitch,
                      readback.tightRowPitch, readback.staging.size());
        ok &= q1a_test::Require(read == Status::Ok, "Readback failed");
        if (read == Status::Ok) {
            ok &= q1a_test::CompareTightReadback(readback, &error);
            ok &= q1a_test::Require(readback.rowPitch >= q1a_test::kTightRowPitch,
                                    "GPU row pitch is smaller than the tight row pitch");
            ok &= q1a_test::Require(
                readback.staging.size() ==
                    static_cast<size_t>(readback.rowPitch) * readback.height,
                "staging bytes do not cover rowPitch * height");
            ok &= q1a_test::Require(readback.adapter.name == device.adapter().name,
                                    "readback adapter does not match device adapter");

            const std::string fileName =
                std::string("q1a-clear-") +
                doom3::rhi::ToString(device.adapter().adapterClass) + "-" +
                std::to_string(index) + ".bmp";
            const std::string path = q1a_test::EvidencePath(fileName);
            const bool wrote = q1a_test::WriteBmp(path, readback, 16);
            q1a_test::Log("SCREENSHOT path=%s wrote=%s", path.c_str(), wrote ? "true" : "false");
            ok &= q1a_test::Require(wrote, "failed to write the readback BMP");
        }

        const ReleaseStatus release = device.TryRelease(submission, &error);
        q1a_test::Log("CLEAR submission=%d release_status=%s reason=%s", index,
                      doom3::rhi::ToString(release), error.c_str());
        ok &= q1a_test::Require(release == ReleaseStatus::Released,
                                "TryRelease did not release a completed submission");

        if (index == 0) {
            firstSerial = submission.completionSerial;
        } else {
            secondSerial = submission.completionSerial;
        }
    }

    q1a_test::Log("CLEAR serials first=%llu second=%llu",
                  static_cast<unsigned long long>(firstSerial),
                  static_cast<unsigned long long>(secondSerial));
    ok &= q1a_test::Require(firstSerial != 0 && secondSerial > firstSerial,
                            "two submissions did not advance the completion serial");
    return ok ? 0 : 1;
}

int RunStateJournal(Device& device) {
    const ClearReadbackDesc desc = q1a_test::MakePatternDesc();
    Submission submission;
    std::string error;
    Status status =
        device.SubmitClearReadback(desc, doom3::rhi::SubmissionOptions{}, &submission, &error);
    q1a_test::Log("STATE submit_status=%s reason=%s", doom3::rhi::ToString(status),
                  error.c_str());
    bool ok = q1a_test::Require(status == Status::Ok, "SubmitClearReadback failed");
    if (status == Status::Ok) {
        status = device.Wait(submission, &error);
        q1a_test::Log("STATE wait_status=%s reason=%s", doom3::rhi::ToString(status),
                      error.c_str());
        ok &= q1a_test::Require(status == Status::Ok, "Wait failed");
    }
    if (status == Status::Ok) {
        ReadbackResult readback;
        status = device.Readback(submission, &readback, &error);
        q1a_test::Log("STATE readback_status=%s reason=%s", doom3::rhi::ToString(status),
                      error.c_str());
        ok &= q1a_test::Require(status == Status::Ok, "Readback failed");
        if (status == Status::Ok) {
            ok &= q1a_test::CompareTightReadback(readback, &error);
        }
        const ReleaseStatus release = device.TryRelease(submission, &error);
        q1a_test::Log("STATE release_status=%s reason=%s", doom3::rhi::ToString(release),
                      error.c_str());
        ok &= q1a_test::Require(release == ReleaseStatus::Released,
                                "TryRelease did not release a completed submission");
    }

    bool unknownToRenderTarget = false;
    bool renderTargetToCopySource = false;
    bool readbackToCopyDest = false;
    bool renderTargetWronglyCopied = false;
    const auto& journal = device.stateJournal();
    q1a_test::Log("STATE journal_size=%zu", journal.size());
    for (const auto& transition : journal) {
        q1a_test::Log("STATE resource=%s from=%s to=%s reason=%s",
                      transition.resource.c_str(),
                      doom3::rhi::ToString(transition.from),
                      doom3::rhi::ToString(transition.to), transition.reason.c_str());
        if (transition.resource == "render-target" &&
            transition.from == doom3::rhi::ResourceState::Unknown &&
            transition.to == doom3::rhi::ResourceState::RenderTarget) {
            unknownToRenderTarget = true;
        }
        if (transition.resource == "render-target" &&
            transition.from == doom3::rhi::ResourceState::RenderTarget &&
            transition.to == doom3::rhi::ResourceState::CopySource) {
            renderTargetToCopySource = true;
        }
        if (transition.resource == "readback-buffer" &&
            transition.to == doom3::rhi::ResourceState::CopyDest) {
            readbackToCopyDest = true;
        }
        if (transition.resource == "render-target" &&
            transition.to == doom3::rhi::ResourceState::CopyDest) {
            renderTargetWronglyCopied = true;
        }
    }
    // One submission must record exactly the three Q1a transitions, in order.
    // Counting matters: a journal that is missing an entry, duplicates one or
    // appends an unrelated transition must not pass on presence checks alone.
    ok &= q1a_test::Require(journal.size() == 3,
                            "journal does not contain exactly three transitions");
    const bool exactSequence =
        journal.size() == 3 &&
        journal[0].resource == "render-target" &&
        journal[0].from == doom3::rhi::ResourceState::Unknown &&
        journal[0].to == doom3::rhi::ResourceState::RenderTarget &&
        journal[1].resource == "render-target" &&
        journal[1].from == doom3::rhi::ResourceState::RenderTarget &&
        journal[1].to == doom3::rhi::ResourceState::CopySource &&
        journal[2].resource == "readback-buffer" &&
        journal[2].from == doom3::rhi::ResourceState::Unknown &&
        journal[2].to == doom3::rhi::ResourceState::CopyDest;
    ok &= q1a_test::Require(
        exactSequence,
        "journal is not UNKNOWN->RENDER_TARGET, RENDER_TARGET->COPY_SOURCE, "
        "UNKNOWN->COPY_DEST in that order");
    ok &= q1a_test::Require(unknownToRenderTarget,
                            "journal lacks UNKNOWN -> RENDER_TARGET for the render target");
    ok &= q1a_test::Require(renderTargetToCopySource,
                            "journal lacks RENDER_TARGET -> COPY_SOURCE");
    ok &= q1a_test::Require(readbackToCopyDest,
                            "journal lacks COPY_DEST placement for the readback buffer");
    ok &= q1a_test::Require(!renderTargetWronglyCopied,
                            "journal records the render target as a copy destination");
    return ok ? 0 : 1;
}

int RunNegativeControls(Device& device) {
    const ClearReadbackDesc desc = q1a_test::MakePatternDesc();
    Submission submission;
    std::string error;
    Status status =
        device.SubmitClearReadback(desc, doom3::rhi::SubmissionOptions{}, &submission, &error);
    bool ok = q1a_test::Require(status == Status::Ok, "SubmitClearReadback failed");
    if (status != Status::Ok) {
        return 1;
    }
    q1a_test::PrintSubmission("negative", submission);
    status = device.Wait(submission, &error);
    ok &= q1a_test::Require(status == Status::Ok, "Wait failed");
    if (status != Status::Ok) {
        device.TryRelease(submission, nullptr);
        return 1;
    }

    ReadbackResult readback;
    status = device.Readback(submission, &readback, &error);
    ok &= q1a_test::Require(status == Status::Ok, "Readback failed");
    if (status == Status::Ok) {
        ok &= q1a_test::CompareTightReadback(readback, &error);
        ok &= q1a_test::Require(readback.rowPitch >= q1a_test::kTightRowPitch,
                                "GPU row pitch is smaller than the tight row pitch");
        ok &= q1a_test::Require(
            readback.staging.size() ==
                static_cast<size_t>(readback.rowPitch) * readback.height,
            "staging bytes do not cover rowPitch * height");

        const q1a_test::ControlResult wrongStride =
            q1a_test::WrongStrideControl(readback);
        const q1a_test::ControlResult offByOne = q1a_test::OffByOneControl(readback);
        const q1a_test::ControlResult repeatRow0 =
            q1a_test::RepeatRowZeroControl(readback);
        q1a_test::PrintControl("wrong_stride", wrongStride);
        q1a_test::PrintControl("off_by_one_row", offByOne);
        q1a_test::PrintControl("repeat_row_0", repeatRow0);

        const q1a_test::ControlResult controls[] = {wrongStride, offByOne, repeatRow0};
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
        ok &= q1a_test::Require(applicable > 0,
                                "no negative control was applicable; nothing was proved");
        ok &= q1a_test::Require(allDetected,
                                "an applicable negative control was not detected");
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
    if (mode == "state_journal") {
        return q1a_test::ForEachAdapter(RunStateJournal);
    }
    if (mode == "negative_controls") {
        return q1a_test::ForEachAdapter(RunNegativeControls);
    }
    q1a_test::Log("unknown rhi_clear_test mode: %s", mode.c_str());
    return 2;
}
