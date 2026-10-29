// Public-seam frame-slot and upload ownership fixture.
//
// Four frames with different per-pass tints are in flight at once. Every
// completed frame must still show its own tint, proving that slot upload storage
// was not overwritten while in use. After completion and release the slots are
// reusable. The deterministic four-slot bound is asserted by the private gated
// companion, not by a timing-sensitive public probe.

#include "q1b_test_support.h"

namespace {

using doom3::rhi::ClearRow;
using doom3::rhi::Device;
using doom3::rhi::DrawFrameDesc;
using doom3::rhi::DrawPassDesc;
using doom3::rhi::Pipeline;
using doom3::rhi::ReadbackResult;
using doom3::rhi::ShaderModule;
using doom3::rhi::ShaderModuleDesc;
using doom3::rhi::ShaderStage;
using doom3::rhi::Status;
using doom3::rhi::Submission;

Status SubmitTinted(Device& device, Pipeline pipeline, float tint, Submission* submission,
                    std::string* error) {
    uint32_t indices[1] = {0};
    ClearRow clears[1];
    clears[0].alpha = 255;
    DrawPassDesc pass;
    pass.pipeline = pipeline;
    pass.colorIndices = indices;
    pass.colorCount = 1;
    pass.colorClears = clears;
    pass.tint[0] = tint;
    pass.tint[1] = tint;
    pass.tint[2] = tint;
    pass.tint[3] = 1.0f;
    DrawFrameDesc frame;
    frame.width = q1b_test::kWidth;
    frame.height = q1b_test::kHeight;
    frame.attachmentCount = 1;
    frame.passCount = 1;
    frame.passes = &pass;
    return device.SubmitDrawFrame(frame, submission, error);
}

int RunFrameResources(Device& device) {
    std::string error;
    ShaderModule vertex;
    ShaderModuleDesc vertexDesc;
    vertexDesc.stage = ShaderStage::Vertex;
    vertexDesc.source = q1b_test::kFullscreenVertex;
    vertexDesc.entryPoint = "VSMain";
    if (device.CreateShaderModule(vertexDesc, &vertex, &error) != Status::Ok) {
        q1b_test::Log("CREATE vertex failed: %s", error.c_str());
        return 1;
    }
    ShaderModule pixel;
    ShaderModuleDesc pixelDesc;
    pixelDesc.stage = ShaderStage::Pixel;
    pixelDesc.source = q1b_test::kPatternPixel;
    pixelDesc.entryPoint = "PSMain";
    if (device.CreateShaderModule(pixelDesc, &pixel, &error) != Status::Ok) {
        q1b_test::Log("CREATE pixel failed: %s", error.c_str());
        return 1;
    }
    doom3::rhi::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.vertexShader = vertex;
    pipelineDesc.pixelShader = pixel;
    pipelineDesc.colorAttachmentCount = 1;
    Pipeline pipeline;
    if (device.CreateGraphicsPipeline(pipelineDesc, &pipeline, &error) != Status::Ok) {
        q1b_test::Log("CREATE pipeline failed: %s", error.c_str());
        return 1;
    }

    const float tints[4] = {1.0f, 0.5f, 0.25f, 0.75f};
    Submission submissions[4];
    for (uint32_t i = 0; i < 4; ++i) {
        if (SubmitTinted(device, pipeline, tints[i], &submissions[i], &error) !=
            Status::Ok) {
            q1b_test::Log("SUBMIT frame %u failed: %s", i, error.c_str());
            return 1;
        }
        q1b_test::Log("SLOT frame %u submitted tint=%.2f", i, tints[i]);
    }
    for (uint32_t i = 0; i < 4; ++i) {
        if (!q1b_test::WaitComplete(device, submissions[i])) {
            q1b_test::Log("WAIT frame %u failed", i);
            return 1;
        }
        ReadbackResult image;
        if (device.ReadbackAttachment(submissions[i], 0, &image, &error) != Status::Ok) {
            q1b_test::Log("READBACK frame %u failed: %s", i, error.c_str());
            return 1;
        }
        if (!q1b_test::VerifyPattern(image, i, nullptr)) {
            q1b_test::Log("FRAME %u does not carry its own tint %.2f", i, tints[i]);
            return 1;
        }
        q1b_test::Log("SLOT frame %u kept tint %.2f after four submissions", i,
                      tints[i]);
    }
    q1b_test::Log("SLOT upload storage isolated across four in-flight frames");

    for (uint32_t i = 0; i < 4; ++i) {
        if (device.TryRelease(submissions[i], &error) !=
            doom3::rhi::ReleaseStatus::Released) {
            q1b_test::Log("RELEASE frame %u failed: %s", i, error.c_str());
            return 1;
        }
    }
    Submission reused;
    if (SubmitTinted(device, pipeline, 0.5f, &reused, &error) != Status::Ok) {
        q1b_test::Log("SUBMIT after completion failed: %s", error.c_str());
        return 1;
    }
    if (!q1b_test::WaitComplete(device, reused)) {
        return 1;
    }
    ReadbackResult image;
    if (device.ReadbackAttachment(reused, 0, &image, &error) != Status::Ok) {
        q1b_test::Log("READBACK reused slot failed: %s", error.c_str());
        return 1;
    }
    if (!q1b_test::VerifyPattern(image, 1, nullptr)) {
        return 1;
    }
    q1b_test::Log("SLOT reuse after completion verified");
    if (device.TryRelease(reused, &error) != doom3::rhi::ReleaseStatus::Released) {
        return 1;
    }
    return 0;
}

}  // namespace

int main() {
    return q1b_test::ForEachAdapter(
        [](Device& device) { return RunFrameResources(device); });
}
