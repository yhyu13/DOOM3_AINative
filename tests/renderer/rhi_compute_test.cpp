// Public-seam compute/UAV readback fixture.
//
// The program writes sixteen 32-bit values. The expected table below is a
// literal; it is not recomputed with the program's expression at run time.

#include "q1b_test_support.h"

#include <cstring>

namespace {

// Row-major, four values per row. Written out by hand from the published
// coordinate rule and retained as independent expected data.
constexpr uint32_t kExpected[16] = {
    162, 178, 130, 146,
    175, 191, 143, 159,
    168, 184, 136, 152,
    181, 133, 149, 229,
};

int RunCompute(doom3::rhi::Device& device) {
    using namespace doom3::rhi;
    std::string error;

    ShaderModule compute;
    ShaderModuleDesc shaderDesc;
    shaderDesc.stage = ShaderStage::Compute;
    shaderDesc.source = q1b_test::kComputeProgram;
    shaderDesc.entryPoint = "CSMain";
    if (device.CreateShaderModule(shaderDesc, &compute, &error) != Status::Ok) {
        q1b_test::Log("CREATE compute shader failed: %s", error.c_str());
        return 1;
    }
    ComputePipelineDesc pipelineDesc;
    pipelineDesc.computeShader = compute;
    Pipeline pipeline;
    if (device.CreateComputePipeline(pipelineDesc, &pipeline, &error) != Status::Ok) {
        q1b_test::Log("CREATE compute pipeline failed: %s", error.c_str());
        return 1;
    }

    ComputeDispatchDesc dispatch;
    dispatch.pipeline = pipeline;
    dispatch.groupsX = 1;
    dispatch.groupsY = 1;
    dispatch.groupsZ = 1;
    dispatch.elementCount = 16;
    Submission submission;
    if (device.SubmitCompute(dispatch, &submission, &error) != Status::Ok) {
        q1b_test::Log("SUBMIT compute failed: %s", error.c_str());
        return 1;
    }
    if (!q1b_test::WaitComplete(device, submission)) {
        q1b_test::Log("WAIT compute failed");
        return 1;
    }
    BufferResult result;
    if (device.ReadbackBuffer(submission, &result, &error) != Status::Ok) {
        q1b_test::Log("READBACK compute failed: %s", error.c_str());
        return 1;
    }
    if (!q1b_test::Require(result.elementCount == 16 && result.elementSize == 4,
                           "compute result shape is not sixteen 32-bit elements")) {
        return 1;
    }
    if (!q1b_test::Require(result.bytes.size() == 64,
                           "compute result byte count is not 64")) {
        return 1;
    }
    uint32_t actual[16] = {};
    std::memcpy(actual, result.bytes.data(), sizeof(actual));
    bool ok = true;
    for (uint32_t i = 0; i < 16; ++i) {
        q1b_test::Log("COMPUTE index=%u expected=%u actual=%u %s", i, kExpected[i],
                      actual[i], actual[i] == kExpected[i] ? "OK" : "MISMATCH");
        if (actual[i] != kExpected[i]) {
            ok = false;
        }
    }
    if (!ok) {
        return 1;
    }

    // A transposed expectation must be rejected by the same comparison.
    bool transposedAccepted = true;
    for (uint32_t y = 0; y < 4; ++y) {
        for (uint32_t x = 0; x < 4; ++x) {
            if (actual[y * 4 + x] != kExpected[x * 4 + y]) {
                transposedAccepted = false;
            }
        }
    }
    if (transposedAccepted) {
        q1b_test::Log("CONTROL transposed expectation was accepted");
        return 1;
    }
    q1b_test::Log("CONTROL transposed expectation rejected");

    ComputeDispatchDesc zeroGroups = dispatch;
    zeroGroups.groupsX = 0;
    Submission rejected;
    if (device.SubmitCompute(zeroGroups, &rejected, &error) != Status::InvalidArgument) {
        q1b_test::Log("CONTROL zero workgroup count was not rejected");
        return 1;
    }
    q1b_test::Log("CONTROL zero workgroup count rejected: %s", error.c_str());

    // A graphics pipeline must not be accepted as a compute pipeline.
    ShaderModule vertex;
    ShaderModuleDesc vertexDesc;
    vertexDesc.stage = ShaderStage::Vertex;
    vertexDesc.source = q1b_test::kFullscreenVertex;
    vertexDesc.entryPoint = "VSMain";
    ShaderModule pixel;
    ShaderModuleDesc pixelDesc;
    pixelDesc.stage = ShaderStage::Pixel;
    pixelDesc.source = q1b_test::kPatternPixel;
    pixelDesc.entryPoint = "PSMain";
    if (device.CreateShaderModule(vertexDesc, &vertex, &error) != Status::Ok ||
        device.CreateShaderModule(pixelDesc, &pixel, &error) != Status::Ok) {
        q1b_test::Log("CREATE graphics modules for the wrong-kind control failed: %s",
                      error.c_str());
        return 1;
    }
    doom3::rhi::GraphicsPipelineDesc graphicsDesc;
    graphicsDesc.vertexShader = vertex;
    graphicsDesc.pixelShader = pixel;
    graphicsDesc.colorAttachmentCount = 1;
    Pipeline graphics;
    if (device.CreateGraphicsPipeline(graphicsDesc, &graphics, &error) != Status::Ok) {
        return 1;
    }
    ComputeDispatchDesc wrongKind = dispatch;
    wrongKind.pipeline = graphics;
    if (device.SubmitCompute(wrongKind, &rejected, &error) != Status::InvalidArgument) {
        q1b_test::Log("CONTROL graphics pipeline was accepted by dispatch");
        return 1;
    }
    q1b_test::Log("CONTROL graphics pipeline rejected by dispatch: %s", error.c_str());

    if (device.TryRelease(submission, &error) != ReleaseStatus::Released) {
        q1b_test::Log("RELEASE compute failed: %s", error.c_str());
        return 1;
    }
    return 0;
}

}  // namespace

int main() {
    return q1b_test::ForEachAdapter([](doom3::rhi::Device& device) {
        return RunCompute(device);
    });
}
