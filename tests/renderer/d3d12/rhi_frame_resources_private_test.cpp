// D3D12-private frame-slot fixture.
//
// Four draw submissions are held on a backend-private gate so they are
// deterministically in flight. A fifth public submission must be refused with
// NotReady. The private hook records the slot index of every submission; the
// four indices must be distinct. Releasing the gates lets all four complete;
// after release the slots are reusable.

#include "q1b_test_support.h"

#include "d3d12/d3d12_private.h"

#include <set>

namespace {

using doom3::rhi::BackendAccess;
using doom3::rhi::ClearRow;
using doom3::rhi::Device;
using doom3::rhi::DrawFrameDesc;
using doom3::rhi::DrawPassDesc;
using doom3::rhi::Pipeline;
using doom3::rhi::ShaderModule;
using doom3::rhi::ShaderModuleDesc;
using doom3::rhi::ShaderStage;
using doom3::rhi::Status;
using doom3::rhi::Submission;

int RunGated(Device& device) {
    std::string error;
    ShaderModule vertex;
    ShaderModuleDesc vertexDesc;
    vertexDesc.stage = ShaderStage::Vertex;
    vertexDesc.source = q1b_test::kFullscreenVertex;
    vertexDesc.entryPoint = "VSMain";
    if (device.CreateShaderModule(vertexDesc, &vertex, &error) != Status::Ok) {
        return 1;
    }
    ShaderModule pixel;
    ShaderModuleDesc pixelDesc;
    pixelDesc.stage = ShaderStage::Pixel;
    pixelDesc.source = q1b_test::kPatternPixel;
    pixelDesc.entryPoint = "PSMain";
    if (device.CreateShaderModule(pixelDesc, &pixel, &error) != Status::Ok) {
        return 1;
    }
    doom3::rhi::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.vertexShader = vertex;
    pipelineDesc.pixelShader = pixel;
    pipelineDesc.colorAttachmentCount = 1;
    Pipeline pipeline;
    if (device.CreateGraphicsPipeline(pipelineDesc, &pipeline, &error) != Status::Ok) {
        return 1;
    }

    const float tints[4] = {1.0f, 0.5f, 0.25f, 0.75f};
    Submission submissions[4];
    std::set<uint32_t> slotIndices;
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t indices[1] = {0};
        ClearRow clears[1];
        clears[0].alpha = 255;
        DrawPassDesc pass;
        pass.pipeline = pipeline;
        pass.colorIndices = indices;
        pass.colorCount = 1;
        pass.colorClears = clears;
        pass.tint[0] = tints[i];
        pass.tint[1] = tints[i];
        pass.tint[2] = tints[i];
        pass.tint[3] = 1.0f;
        DrawFrameDesc frame;
        frame.width = q1b_test::kWidth;
        frame.height = q1b_test::kHeight;
        frame.attachmentCount = 1;
        frame.passCount = 1;
        frame.passes = &pass;
        if (BackendAccess::SubmitDrawGated(device, frame, &submissions[i], &error) !=
            Status::Ok) {
            q1b_test::Log("PRIVATE_SLOT gated submit %u failed: %s", i, error.c_str());
            return 1;
        }
        const doom3::rhi::d3d12::SlotObservation observation =
            BackendAccess::Slot(device, submissions[i]);
        q1b_test::Log("PRIVATE_SLOT submission=%u slot=%u completed_at_acquire=%llu "
                      "serial=%llu in_use=%s",
                      i, observation.slotIndex,
                      static_cast<unsigned long long>(observation.completedAtAcquire),
                      static_cast<unsigned long long>(observation.serial),
                      observation.inUse ? "true" : "false");
        slotIndices.insert(observation.slotIndex);
    }
    if (slotIndices.size() != 4) {
        q1b_test::Require(false, "four in-flight submissions did not use four slots");
        return 1;
    }
    q1b_test::Log("PRIVATE_SLOT four distinct slot indices observed");

    // The empty frame is rejected as InvalidArgument before slot acquisition, so
    // use the real frame once more instead.
    uint32_t indices[1] = {0};
    ClearRow clears[1];
    clears[0].alpha = 255;
    DrawPassDesc pass;
    pass.pipeline = pipeline;
    pass.colorIndices = indices;
    pass.colorCount = 1;
    pass.colorClears = clears;
    DrawFrameDesc frame;
    frame.width = q1b_test::kWidth;
    frame.height = q1b_test::kHeight;
    frame.attachmentCount = 1;
    frame.passCount = 1;
    frame.passes = &pass;
    Submission fifth;
    const Status overflow = device.SubmitDrawFrame(frame, &fifth, &error);
    if (overflow != Status::NotReady) {
        q1b_test::Log("PRIVATE_SLOT fifth gated frame was not refused: status=%s",
                      doom3::rhi::ToString(overflow));
        return 1;
    }
    q1b_test::Log("PRIVATE_SLOT fifth frame refused: %s", error.c_str());

    doom3::rhi::ReadbackResult early;
    if (device.ReadbackAttachment(submissions[0], 0, &early, &error) !=
        Status::NotReady) {
        q1b_test::Require(false, "readback of an in-flight frame was not NotReady");
        return 1;
    }
    q1b_test::Log("PRIVATE_SLOT in-flight readback refused: %s", error.c_str());

    for (uint32_t i = 0; i < 4; ++i) {
        if (BackendAccess::ReleaseGate(device, submissions[i], &error) != Status::Ok) {
            q1b_test::Log("PRIVATE_SLOT gate release %u failed: %s", i, error.c_str());
            return 1;
        }
    }
    for (uint32_t i = 0; i < 4; ++i) {
        if (!q1b_test::WaitComplete(device, submissions[i])) {
            return 1;
        }
        doom3::rhi::ReadbackResult image;
        if (device.ReadbackAttachment(submissions[i], 0, &image, &error) != Status::Ok) {
            return 1;
        }
        if (!q1b_test::VerifyPattern(image, i, nullptr)) {
            return 1;
        }
        if (device.TryRelease(submissions[i], &error) !=
            doom3::rhi::ReleaseStatus::Released) {
            return 1;
        }
    }
    Submission after;
    if (device.SubmitDrawFrame(frame, &after, &error) != Status::Ok) {
        q1b_test::Log("PRIVATE_SLOT submit after release failed: %s", error.c_str());
        return 1;
    }
    if (!q1b_test::WaitComplete(device, after)) {
        return 1;
    }
    if (device.TryRelease(after, &error) != doom3::rhi::ReleaseStatus::Released) {
        return 1;
    }
    q1b_test::Log("PRIVATE_SLOT slots reusable after release");
    return 0;
}

}  // namespace

int main() {
    return q1b_test::ForEachAdapter([](Device& device) { return RunGated(device); });
}
