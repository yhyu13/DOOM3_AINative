// T034 — ordered command-stream fixtures (spec 004).
//
// Red before src/renderer/rhi/engine_scene_adapter.cpp existed as a real implementation.
// These cases assert the observable order of classic-parity.md §4 O-02/O-11/O-12 and the
// mid-frame split in engine-integration.md §6 rule 6:
//
//   * view, draw, set-buffer, copy and swap execute in FIFO order;
//   * CaptureRenderToFile executes the pending stream and clears it mid-frame;
//   * CaptureRenderToImage appends a copy and does not flush.
//
// A good-looking still with a reordered stream is a failure, so every case compares an
// exact sequence and one case proves the comparator rejects a reorder.

#include "q2_test_support.h"

namespace {

using doom3::rhi::ColorTargetSelector;
using doom3::rhi::SceneAdapter;
using doom3::rhi::SceneAdapterDesc;
using doom3::rhi::SceneCommandKind;
using doom3::rhi::SceneCommandQueue;
using doom3::rhi::SceneExecuteResult;
using doom3::rhi::Status;

int RunFifoOrder(SceneAdapter& adapter) {
    SceneCommandQueue queue;
    queue.BeginView(q2_test::MakeSceneView(11, 1280, 720));
    queue.Draw(q2_test::MakePacket(1, 3));
    queue.SetColorTarget(ColorTargetSelector::Offscreen);
    queue.CopyToImage(7);
    queue.SwapBuffers();

    SceneExecuteResult result;
    std::string error;
    if (adapter.Execute(&queue, &result, &error) != Status::Ok) {
        q2_test::Log("EXEC FIFO failed: %s", error.c_str());
        return 1;
    }
    const std::vector<SceneCommandKind> expected = {
        SceneCommandKind::BeginView, SceneCommandKind::DrawPacket,
        SceneCommandKind::SetColorTarget, SceneCommandKind::CopyToImage,
        SceneCommandKind::SwapBuffers};
    if (!q2_test::RequireSequence(result.executed, expected, "fifo")) {
        return 1;
    }
    if (!q2_test::RequireEq(static_cast<uint32_t>(queue.Size()), 0, "queue drained")) {
        return 1;
    }
    if (!q2_test::RequireEq(result.drawsExtracted, 1, "draw packets extracted")) {
        return 1;
    }
    return 0;
}

int RunCaptureToFileFlush(SceneAdapter& adapter) {
    SceneCommandQueue queue;
    queue.BeginView(q2_test::MakeSceneView(21, 1280, 720));
    queue.Draw(q2_test::MakePacket(2, 3));
    if (!queue.CaptureToFile(9)) {
        q2_test::Log("CaptureToFile rejected a valid destination");
        return 1;
    }
    if (!q2_test::Require(queue.HasPendingFlush(), "CaptureToFile must request a flush")) {
        return 1;
    }
    if (!q2_test::RequireEq(static_cast<uint32_t>(queue.FlushMark()), 3,
                            "flush mark covers the copy")) {
        return 1;
    }
    queue.Draw(q2_test::MakePacket(3, 3));
    if (!q2_test::RequireEq(static_cast<uint32_t>(queue.Size()), 4,
                            "commands recorded after the capture stay queued")) {
        return 1;
    }

    SceneExecuteResult midFrame;
    std::string error;
    if (adapter.Execute(&queue, &midFrame, &error) != Status::Ok) {
        q2_test::Log("EXEC capture-to-file failed: %s", error.c_str());
        return 1;
    }
    const std::vector<SceneCommandKind> expectedPrefix = {
        SceneCommandKind::BeginView, SceneCommandKind::DrawPacket,
        SceneCommandKind::CopyToImage};
    if (!q2_test::RequireSequence(midFrame.executed, expectedPrefix, "capture-to-file prefix")) {
        return 1;
    }
    if (!q2_test::RequireEq(static_cast<uint32_t>(queue.Size()), 1,
                            "flush clears the executed prefix mid-frame")) {
        return 1;
    }
    if (!q2_test::Require(!queue.HasPendingFlush(), "flush mark cleared after execution")) {
        return 1;
    }

    SceneExecuteResult rest;
    if (adapter.Execute(&queue, &rest, &error) != Status::Ok) {
        q2_test::Log("EXEC remainder failed: %s", error.c_str());
        return 1;
    }
    const std::vector<SceneCommandKind> expectedRest = {SceneCommandKind::DrawPacket};
    if (!q2_test::RequireSequence(rest.executed, expectedRest, "remainder after flush")) {
        return 1;
    }
    return 0;
}

int RunCaptureToImageAppends(SceneAdapter& adapter) {
    SceneCommandQueue queue;
    queue.BeginView(q2_test::MakeSceneView(31, 1280, 720));
    queue.Draw(q2_test::MakePacket(4, 3));
    if (!queue.CopyToImage(13)) {
        q2_test::Log("CaptureToImage rejected a valid destination");
        return 1;
    }
    if (!q2_test::Require(!queue.HasPendingFlush(),
                          "CaptureToImage must not flush the queue")) {
        return 1;
    }
    if (!q2_test::RequireEq(static_cast<uint32_t>(queue.Size()), 3,
                            "CaptureToImage keeps the queue")) {
        return 1;
    }
    queue.Draw(q2_test::MakePacket(5, 3));

    SceneExecuteResult result;
    std::string error;
    if (adapter.Execute(&queue, &result, &error) != Status::Ok) {
        q2_test::Log("EXEC capture-to-image failed: %s", error.c_str());
        return 1;
    }
    const std::vector<SceneCommandKind> expected = {
        SceneCommandKind::BeginView, SceneCommandKind::DrawPacket,
        SceneCommandKind::CopyToImage, SceneCommandKind::DrawPacket};
    if (!q2_test::RequireSequence(result.executed, expected, "capture-to-image")) {
        return 1;
    }
    if (!q2_test::RequireEq(static_cast<uint32_t>(queue.Size()), 0, "queue drained")) {
        return 1;
    }
    return 0;
}

int RunRejections(SceneAdapter& adapter) {
    std::string error;
    ColorTargetSelector selector = ColorTargetSelector::Unsupported;
    if (doom3::rhi::TranslateColorTarget(0x0404u, &selector, &error) != Status::Ok ||
        selector != ColorTargetSelector::Current) {
        q2_test::Log("TranslateColorTarget front failed");
        return 1;
    }
    if (doom3::rhi::TranslateColorTarget(0x0405u, &selector, &error) != Status::Ok ||
        selector != ColorTargetSelector::Offscreen) {
        q2_test::Log("TranslateColorTarget back failed");
        return 1;
    }
    error.clear();
    if (doom3::rhi::TranslateColorTarget(0x1234u, &selector, &error) != Status::Unsupported) {
        q2_test::Log("TranslateColorTarget accepted an unknown selector");
        return 1;
    }
    if (error.empty()) {
        q2_test::Log("TranslateColorTarget gave no actionable error");
        return 1;
    }
    q2_test::Log("CONTROL unknown set-buffer value rejected: %s", error.c_str());

    SceneCommandQueue queue;
    if (queue.CopyToImage(0)) {
        q2_test::Log("CONTROL copy with no destination identity was accepted");
        return 1;
    }
    if (!q2_test::RequireEq(static_cast<uint32_t>(queue.Size()), 0,
                            "rejected copy does not enqueue")) {
        return 1;
    }

    // EndView without BeginView is a structurally invalid stream; it must be fatal and
    // must consume nothing rather than half-executing the frame.
    queue.EndView();
    SceneExecuteResult result;
    error.clear();
    if (adapter.Execute(&queue, &result, &error) != Status::InvalidArgument) {
        q2_test::Log("CONTROL unbalanced EndView was not rejected");
        return 1;
    }
    if (!result.executed.empty()) {
        q2_test::Log("CONTROL invalid stream was partially executed");
        return 1;
    }
    return 0;
}

int RunStructuralModeDisclosure(SceneAdapter& adapter) {
    SceneCommandQueue queue;
    queue.BeginView(q2_test::MakeSceneView(41, 1280, 720));
    queue.Draw(q2_test::MakePacket(6, 0));
    SceneExecuteResult result;
    std::string error;
    if (adapter.Execute(&queue, &result, &error) != Status::Ok) {
        q2_test::Log("EXEC structural disclosure failed: %s", error.c_str());
        return 1;
    }
    if (adapter.desc().structuralOnly) {
        if (!q2_test::Require(result.drawSubmissionUnavailable,
                              "structural mode must disclose that nothing was drawn")) {
            return 1;
        }
        if (!q2_test::RequireEq(result.drawsSubmitted, 0,
                                "structural mode submits no draw")) {
            return 1;
        }
        q2_test::Log("DISCLOSURE structural mode: extracted=%u submitted=%u refused=%u",
                     result.drawsExtracted, result.drawsSubmitted, result.drawsRefused);
    }
    if (!q2_test::RequireEq(result.drawsRefused, 1,
                            "a packet with no resolvable stage is refused")) {
        return 1;
    }
    return 0;
}

}  // namespace

int main() {
    if (!q2_test::SequenceControlRejectsReorder()) {
        return 1;
    }

    SceneAdapterDesc desc;
    desc.displayWidth = 1280;
    desc.displayHeight = 720;
    desc.structuralOnly = true;

    SceneAdapter adapter;
    std::string error;
    if (adapter.Initialize(desc, &error) != Status::Ok) {
        q2_test::Log("INIT adapter failed: %s", error.c_str());
        return 1;
    }

    if (RunFifoOrder(adapter) != 0) return 1;
    if (RunCaptureToFileFlush(adapter) != 0) return 1;
    if (RunCaptureToImageAppends(adapter) != 0) return 1;
    if (RunRejections(adapter) != 0) return 1;
    if (RunStructuralModeDisclosure(adapter) != 0) return 1;

    q2_test::Log("T034 PASS");
    return 0;
}
