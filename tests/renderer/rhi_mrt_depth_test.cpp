// Public-seam multi-attachment and sampled-depth fixture.
//
// Pass one writes three distinct colours and a left/right depth split. Pass two
// samples that depth and writes it into a fourth attachment. Every expected
// byte is an independent literal.

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

bool CheckColour(const ReadbackResult& image, uint8_t r, uint8_t g, uint8_t b,
                 uint8_t a) {
    for (uint32_t y = 0; y < image.height; ++y) {
        for (uint32_t x = 0; x < image.width; ++x) {
            const size_t offset = static_cast<size_t>(y) * image.rowStride + x * 4;
            if (offset + 4 > image.pixels.size()) {
                return false;
            }
            // The x = 0 fragment writes depth 1.0 against a 1.0 clear, so the
            // LESS test discards it and the clear colour survives.
            const uint8_t wantR = (x == 0) ? 0 : r;
            const uint8_t wantG = (x == 0) ? 0 : g;
            const uint8_t wantB = (x == 0) ? 0 : b;
            if (std::abs(image.pixels[offset + 0] - wantR) > q1b_test::kTolerance ||
                std::abs(image.pixels[offset + 1] - wantG) > q1b_test::kTolerance ||
                std::abs(image.pixels[offset + 2] - wantB) > q1b_test::kTolerance ||
                std::abs(image.pixels[offset + 3] - a) > q1b_test::kTolerance) {
                q1b_test::Log("MRT_MISMATCH (%u,%u) actual=%u,%u,%u,%u expected=%u,%u,%u,%u",
                              x, y, image.pixels[offset + 0], image.pixels[offset + 1],
                              image.pixels[offset + 2], image.pixels[offset + 3], wantR,
                              wantG, wantB, a);
                return false;
            }
        }
    }
    return true;
}

bool CheckDepthSample(const ReadbackResult& image) {
    for (uint32_t y = 0; y < image.height; ++y) {
        for (uint32_t x = 0; x < image.width; ++x) {
            const uint8_t expectedRed = (x == 0) ? 255 : ((x < 4) ? 64 : 191);
            const size_t offset = static_cast<size_t>(y) * image.rowStride + x * 4;
            if (offset + 4 > image.pixels.size()) {
                return false;
            }
            if (std::abs(image.pixels[offset + 0] - expectedRed) >
                    q1b_test::kTolerance ||
                image.pixels[offset + 1] > q1b_test::kTolerance ||
                image.pixels[offset + 2] > q1b_test::kTolerance ||
                std::abs(image.pixels[offset + 3] - 255) > q1b_test::kTolerance) {
                q1b_test::Log("DEPTH_MISMATCH (%u,%u) actual=%u,%u,%u,%u expected=%u,0,0,255",
                              x, y, image.pixels[offset + 0], image.pixels[offset + 1],
                              image.pixels[offset + 2], image.pixels[offset + 3],
                              expectedRed);
                return false;
            }
        }
    }
    return true;
}

int RunMrtDepth(Device& device) {
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
    ShaderModule mrtPixel;
    ShaderModuleDesc mrtDesc;
    mrtDesc.stage = ShaderStage::Pixel;
    mrtDesc.source = q1b_test::kMrtPixel;
    mrtDesc.entryPoint = "PSMain";
    if (device.CreateShaderModule(mrtDesc, &mrtPixel, &error) != Status::Ok) {
        q1b_test::Log("CREATE mrt pixel failed: %s", error.c_str());
        return 1;
    }
    ShaderModule samplePixel;
    ShaderModuleDesc sampleDesc;
    sampleDesc.stage = ShaderStage::Pixel;
    sampleDesc.source = q1b_test::kDepthSamplePixel;
    sampleDesc.entryPoint = "PSMain";
    if (device.CreateShaderModule(sampleDesc, &samplePixel, &error) != Status::Ok) {
        q1b_test::Log("CREATE depth-sample pixel failed: %s", error.c_str());
        return 1;
    }
    doom3::rhi::GraphicsPipelineDesc firstDesc;
    firstDesc.vertexShader = vertex;
    firstDesc.pixelShader = mrtPixel;
    firstDesc.colorAttachmentCount = 3;
    firstDesc.depthTest = true;
    firstDesc.depthWrite = true;
    Pipeline first;
    if (device.CreateGraphicsPipeline(firstDesc, &first, &error) != Status::Ok) {
        q1b_test::Log("CREATE mrt pipeline failed: %s", error.c_str());
        return 1;
    }
    doom3::rhi::GraphicsPipelineDesc secondDesc;
    secondDesc.vertexShader = vertex;
    secondDesc.pixelShader = samplePixel;
    secondDesc.colorAttachmentCount = 1;
    secondDesc.sampleDepth = true;
    Pipeline second;
    if (device.CreateGraphicsPipeline(secondDesc, &second, &error) != Status::Ok) {
        q1b_test::Log("CREATE depth-sample pipeline failed: %s", error.c_str());
        return 1;
    }

    uint32_t firstIndices[3] = {0, 1, 2};
    ClearRow firstClears[3] = {};
    firstClears[0].alpha = 255;
    firstClears[1].alpha = 255;
    firstClears[2].alpha = 255;
    uint32_t secondIndices[1] = {3};
    ClearRow secondClears[1] = {};
    secondClears[0].alpha = 255;
    DrawPassDesc passes[2];
    passes[0].pipeline = first;
    passes[0].colorIndices = firstIndices;
    passes[0].colorCount = 3;
    passes[0].colorClears = firstClears;
    passes[1].pipeline = second;
    passes[1].colorIndices = secondIndices;
    passes[1].colorCount = 1;
    passes[1].colorClears = secondClears;
    DrawFrameDesc frame;
    frame.width = q1b_test::kWidth;
    frame.height = q1b_test::kHeight;
    frame.attachmentCount = 4;
    frame.depthEnabled = true;
    frame.depthClear = 1.0f;
    frame.passCount = 2;
    frame.passes = passes;

    Submission submission;
    if (device.SubmitDrawFrame(frame, &submission, &error) != Status::Ok) {
        q1b_test::Log("SUBMIT mrt/depth failed: %s", error.c_str());
        return 1;
    }
    if (!q1b_test::WaitComplete(device, submission)) {
        q1b_test::Log("WAIT mrt/depth failed");
        return 1;
    }
    ReadbackResult attachments[4];
    for (uint32_t i = 0; i < 4; ++i) {
        if (device.ReadbackAttachment(submission, i, &attachments[i], &error) !=
            Status::Ok) {
            q1b_test::Log("READBACK attachment %u failed: %s", i, error.c_str());
            return 1;
        }
        q1b_test::WriteBmp(
            q1a_test::EvidencePath("q1b-attachment-" + std::to_string(i) + ".bmp"),
            attachments[i]);
    }
    if (!CheckColour(attachments[0], 10, 20, 30, 255)) {
        return 1;
    }
    if (!CheckColour(attachments[1], 40, 50, 60, 255)) {
        return 1;
    }
    if (!CheckColour(attachments[2], 70, 80, 90, 255)) {
        return 1;
    }
    if (!CheckDepthSample(attachments[3])) {
        return 1;
    }
    q1b_test::Log("MRT three distinct attachments and sampled depth verified");

    if (CheckColour(attachments[1], 10, 20, 30, 255)) {
        q1b_test::Log("CONTROL attachment 1 accepted attachment 0's colour");
        return 1;
    }
    if (CheckDepthSample(attachments[2])) {
        q1b_test::Log("CONTROL attachment 2 accepted the depth sample");
        return 1;
    }
    q1b_test::Log("CONTROLs rejected cross-attachment expectations");

    DrawFrameDesc withoutDepth = frame;
    withoutDepth.depthEnabled = false;
    Submission rejected;
    if (device.SubmitDrawFrame(withoutDepth, &rejected, &error) !=
        Status::InvalidArgument) {
        q1b_test::Log("CONTROL depth use without a depth attachment was not rejected");
        return 1;
    }
    q1b_test::Log("CONTROL depth without attachment rejected: %s", error.c_str());

    uint32_t duplicateIndices[1] = {0};
    ClearRow duplicateClears[1] = {};
    DrawPassDesc duplicatePass = passes[1];
    duplicatePass.colorIndices = duplicateIndices;
    duplicatePass.colorClears = duplicateClears;
    DrawFrameDesc duplicate = frame;
    duplicate.passCount = 2;
    DrawPassDesc duplicatePasses[2] = {passes[0], duplicatePass};
    duplicate.passes = duplicatePasses;
    if (device.SubmitDrawFrame(duplicate, &rejected, &error) !=
        Status::InvalidArgument) {
        q1b_test::Log("CONTROL duplicate colour write was not rejected");
        return 1;
    }
    q1b_test::Log("CONTROL duplicate colour write rejected: %s", error.c_str());

    if (device.TryRelease(submission, &error) != doom3::rhi::ReleaseStatus::Released) {
        q1b_test::Log("RELEASE mrt/depth failed: %s", error.c_str());
        return 1;
    }
    return 0;
}

}  // namespace

int main() {
    return q1b_test::ForEachAdapter([](Device& device) { return RunMrtDepth(device); });
}
