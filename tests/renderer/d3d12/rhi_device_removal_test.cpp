// D3D12-private fault-injected device-removal fixture.
//
// A gated draw is in flight when the private hook reports the device removed.
// Completion, readback, release and a new submission must fail with actionable
// results; teardown must return within a bounded time instead of waiting for a
// completion that can never arrive; a fresh device must work afterwards.

#include "q1b_test_support.h"

#include "d3d12/d3d12_private.h"

#include <chrono>

namespace {

using doom3::rhi::BackendAccess;
using doom3::rhi::ClearRow;
using doom3::rhi::Device;
using doom3::rhi::DeviceOptions;
using doom3::rhi::DrawFrameDesc;
using doom3::rhi::DrawPassDesc;
using doom3::rhi::Pipeline;
using doom3::rhi::ShaderModule;
using doom3::rhi::ShaderModuleDesc;
using doom3::rhi::ShaderStage;
using doom3::rhi::Status;
using doom3::rhi::Submission;

bool MakePipeline(Device& device, Pipeline* pipeline, std::string* error) {
    ShaderModule vertex;
    ShaderModuleDesc vertexDesc;
    vertexDesc.stage = ShaderStage::Vertex;
    vertexDesc.source = q1b_test::kFullscreenVertex;
    vertexDesc.entryPoint = "VSMain";
    if (device.CreateShaderModule(vertexDesc, &vertex, error) != Status::Ok) {
        return false;
    }
    ShaderModule pixel;
    ShaderModuleDesc pixelDesc;
    pixelDesc.stage = ShaderStage::Pixel;
    pixelDesc.source = q1b_test::kPatternPixel;
    pixelDesc.entryPoint = "PSMain";
    if (device.CreateShaderModule(pixelDesc, &pixel, error) != Status::Ok) {
        return false;
    }
    doom3::rhi::GraphicsPipelineDesc desc;
    desc.vertexShader = vertex;
    desc.pixelShader = pixel;
    desc.colorAttachmentCount = 1;
    return device.CreateGraphicsPipeline(desc, pipeline, error) == Status::Ok;
}

DrawFrameDesc MakeFrame(Pipeline pipeline, uint32_t* index, ClearRow* clear,
                        DrawPassDesc* pass) {
    *index = 0;
    clear->alpha = 255;
    pass->pipeline = pipeline;
    pass->colorIndices = index;
    pass->colorCount = 1;
    pass->colorClears = clear;
    DrawFrameDesc frame;
    frame.width = q1b_test::kWidth;
    frame.height = q1b_test::kHeight;
    frame.attachmentCount = 1;
    frame.passCount = 1;
    frame.passes = pass;
    return frame;
}

int RunRemoval(Device& device) {
    std::string error;
    Pipeline pipeline;
    if (!MakePipeline(device, &pipeline, &error)) {
        q1b_test::Log("REMOVAL pipeline creation failed: %s", error.c_str());
        return 1;
    }
    uint32_t index = 0;
    ClearRow clear{};
    DrawPassDesc pass{};
    const DrawFrameDesc frame = MakeFrame(pipeline, &index, &clear, &pass);

    Submission gated;
    if (BackendAccess::SubmitDrawGated(device, frame, &gated, &error) != Status::Ok) {
        q1b_test::Log("REMOVAL gated submit failed: %s", error.c_str());
        return 1;
    }
    BackendAccess::InjectDeviceRemoval(device, true);
    q1b_test::Log("REMOVAL injected while gated submission is in flight");

    if (device.IsComplete(gated)) {
        q1b_test::Require(false, "simulated removal reported completion");
        return 1;
    }
    doom3::rhi::ReadbackResult image;
    if (device.ReadbackAttachment(gated, 0, &image, &error) != Status::DeviceLost) {
        q1b_test::Require(false, "readback after removal was not DeviceLost");
        return 1;
    }
    q1b_test::Log("REMOVAL readback refused: %s", error.c_str());
    if (device.TryRelease(gated, &error) != doom3::rhi::ReleaseStatus::RefusedOutstanding) {
        q1b_test::Require(false, "release after removal did not refuse ownership");
        return 1;
    }
    q1b_test::Log("REMOVAL release refused: %s", error.c_str());

    Submission rejected;
    const Status submit = device.SubmitDrawFrame(frame, &rejected, &error);
    if (submit != Status::DeviceLost) {
        q1b_test::Log("REMOVAL new submission returned %s instead of DeviceLost",
                      doom3::rhi::ToString(submit));
        return 1;
    }
    q1b_test::Log("REMOVAL new submission rejected: %s", error.c_str());

    const auto start = std::chrono::steady_clock::now();
    const Status shutdown = device.Shutdown();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - start)
                             .count();
    q1b_test::Log("REMOVAL teardown status=%s elapsed_ms=%lld",
                  doom3::rhi::ToString(shutdown), static_cast<long long>(elapsed));
    if (shutdown == Status::Ok) {
        q1b_test::Require(false, "teardown claimed success after an unproven completion");
        return 1;
    }
    if (elapsed > 5000) {
        q1b_test::Require(false, "teardown exceeded the 5 s safety bound");
        return 1;
    }

    DeviceOptions options;
    options.adapterClass = device.adapter().adapterClass;
    if (device.Initialize(options, &error) != Status::Ok) {
        q1b_test::Log("REMOVAL restart failed: %s", error.c_str());
        return 1;
    }
    Pipeline restartedPipeline;
    if (!MakePipeline(device, &restartedPipeline, &error)) {
        return 1;
    }
    uint32_t restartedIndex = 0;
    ClearRow restartedClear{};
    DrawPassDesc restartedPass{};
    const DrawFrameDesc restartedFrame =
        MakeFrame(restartedPipeline, &restartedIndex, &restartedClear, &restartedPass);
    Submission after;
    if (device.SubmitDrawFrame(restartedFrame, &after, &error) != Status::Ok) {
        q1b_test::Log("REMOVAL submit after restart failed: %s", error.c_str());
        return 1;
    }
    if (!q1b_test::WaitComplete(device, after)) {
        return 1;
    }
    doom3::rhi::ReadbackResult restored;
    if (device.ReadbackAttachment(after, 0, &restored, &error) != Status::Ok) {
        return 1;
    }
    if (!q1b_test::VerifyPattern(restored, 0, nullptr)) {
        return 1;
    }
    if (device.TryRelease(after, &error) != doom3::rhi::ReleaseStatus::Released) {
        return 1;
    }
    q1b_test::Log("REMOVAL fresh device rendered and released after restart");
    return 0;
}

}  // namespace

int main() {
    return q1b_test::ForEachAdapter([](Device& device) { return RunRemoval(device); });
}
