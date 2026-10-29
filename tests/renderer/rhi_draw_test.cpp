// Public-seam programmable draw fixture.
//
// One oversized triangle writes a pattern recoverable from the pixel
// coordinate. Expected bytes come from literal channel rules, not from the
// readback. A per-pass uniform tint exercises the slot upload path.

#include "q1b_test_support.h"

namespace {

int RunDraw(doom3::rhi::Device& device) {
    using namespace doom3::rhi;
    std::string error;

    ShaderModule vertex;
    ShaderModuleDesc vertexDesc;
    vertexDesc.stage = ShaderStage::Vertex;
    vertexDesc.source = q1b_test::kFullscreenVertex;
    vertexDesc.entryPoint = "VSMain";
    if (device.CreateShaderModule(vertexDesc, &vertex, &error) != Status::Ok) {
        q1b_test::Log("CREATE vertex shader failed: %s", error.c_str());
        return 1;
    }
    ShaderModule pixel;
    ShaderModuleDesc pixelDesc;
    pixelDesc.stage = ShaderStage::Pixel;
    pixelDesc.source = q1b_test::kPatternPixel;
    pixelDesc.entryPoint = "PSMain";
    if (device.CreateShaderModule(pixelDesc, &pixel, &error) != Status::Ok) {
        q1b_test::Log("CREATE pixel shader failed: %s", error.c_str());
        return 1;
    }
    GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.vertexShader = vertex;
    pipelineDesc.pixelShader = pixel;
    pipelineDesc.colorAttachmentCount = 1;
    Pipeline pipeline;
    if (device.CreateGraphicsPipeline(pipelineDesc, &pipeline, &error) != Status::Ok) {
        q1b_test::Log("CREATE graphics pipeline failed: %s", error.c_str());
        return 1;
    }

    auto submit_pattern = [&](float tint, Submission* submission) {
        uint32_t indices[1] = {0};
        ClearRow clears[1];
        clears[0].red = 0;
        clears[0].green = 0;
        clears[0].blue = 0;
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
        return device.SubmitDrawFrame(frame, submission, &error);
    };

    Submission full;
    if (submit_pattern(1.0f, &full) != Status::Ok) {
        q1b_test::Log("SUBMIT draw failed: %s", error.c_str());
        return 1;
    }
    if (!q1b_test::WaitComplete(device, full)) {
        q1b_test::Log("WAIT draw failed");
        return 1;
    }
    ReadbackResult pattern;
    if (device.ReadbackAttachment(full, 0, &pattern, &error) != Status::Ok) {
        q1b_test::Log("READBACK draw failed: %s", error.c_str());
        return 1;
    }
    q1b_test::PrintPixel("PATTERN", 0, 0, pattern);
    q1b_test::PrintPixel("PATTERN", 7, 7, pattern);
    if (!q1b_test::VerifyPattern(pattern, 0, nullptr)) {
        return 1;
    }
    q1b_test::Log("DRAW pattern verified for %u pixels", pattern.width * pattern.height);
    q1b_test::WriteBmp(q1a_test::EvidencePath("q1b-draw-pattern.bmp"), pattern);

    if (!q1b_test::Require(q1b_test::AdjacentPixelControl(pattern),
                           "adjacent-pixel control did not detect the pattern")) {
        return 1;
    }

    Submission half;
    if (submit_pattern(0.5f, &half) != Status::Ok) {
        q1b_test::Log("SUBMIT tinted draw failed: %s", error.c_str());
        return 1;
    }
    if (!q1b_test::WaitComplete(device, half)) {
        return 1;
    }
    ReadbackResult tinted;
    if (device.ReadbackAttachment(half, 0, &tinted, &error) != Status::Ok) {
        q1b_test::Log("READBACK tinted draw failed: %s", error.c_str());
        return 1;
    }
    if (!q1b_test::VerifyPattern(tinted, 1, nullptr)) {
        return 1;
    }
    if (q1b_test::VerifyPattern(tinted, 0, nullptr)) {
        q1b_test::Log("CONTROL wrong tint was accepted");
        return 1;
    }
    q1b_test::Log("DRAW tinted upload verified; wrong-tint control rejected");

    // Out-of-range attachment and a pipeline/attachment mismatch must be
    // actionable rejections, not silent successes.
    ReadbackResult outOfRange;
    if (device.ReadbackAttachment(full, 1, &outOfRange, &error) !=
        Status::InvalidArgument) {
        q1b_test::Log("CONTROL out-of-range attachment was not rejected");
        return 1;
    }
    q1b_test::Log("CONTROL out-of-range attachment rejected: %s", error.c_str());

    uint32_t doubleIndices[2] = {0, 1};
    ClearRow doubleClears[2] = {};
    DrawPassDesc mismatchPass;
    mismatchPass.pipeline = pipeline;
    mismatchPass.colorIndices = doubleIndices;
    mismatchPass.colorCount = 2;
    mismatchPass.colorClears = doubleClears;
    DrawFrameDesc mismatch;
    mismatch.width = q1b_test::kWidth;
    mismatch.height = q1b_test::kHeight;
    mismatch.attachmentCount = 2;
    mismatch.passCount = 1;
    mismatch.passes = &mismatchPass;
    Submission rejected;
    if (device.SubmitDrawFrame(mismatch, &rejected, &error) != Status::InvalidArgument) {
        q1b_test::Log("CONTROL pipeline/attachment mismatch was not rejected");
        return 1;
    }
    q1b_test::Log("CONTROL pipeline/attachment mismatch rejected: %s", error.c_str());

    if (device.TryRelease(full, &error) != ReleaseStatus::Released) {
        q1b_test::Log("RELEASE full failed: %s", error.c_str());
        return 1;
    }
    if (device.TryRelease(half, &error) != ReleaseStatus::Released) {
        q1b_test::Log("RELEASE tinted failed: %s", error.c_str());
        return 1;
    }
    return 0;
}

}  // namespace

int main() {
    return q1b_test::ForEachAdapter([](doom3::rhi::Device& device) {
        return RunDraw(device);
    });
}
