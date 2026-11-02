// Q2 production-path fixtures (spec 004 T080/T081/T082).
//
// geometry: vertex/index buffers, per-item transform/tint, ordered draws.
// texture:  RGBA8 / sRGB / R8 / BC4 / BC5 / BC7 upload and sampling, plus the
//           material stage binding rule.
// stencil:  CP-07 front/back forced-stencil shadow volume and the exclusion
//           plan (no chain, no stencil work).
// scene:    the engine_scene_adapter submits ordered surfaces and light
//           packets from a synthetic geometry source.
//
// Every expectation is a literal derived from the fixture, never from the
// readback. The adapter is exercised with a real device; no retail data, no
// engine submodule and no NVIDIA SDK is involved.

#include "q2_gpu_test_support.h"

#include "rhi/engine_scene_adapter.h"
#include "rhi/lighting.h"
#include "rhi/materials.h"

#include <cstring>
#include <utility>

namespace {

using namespace doom3::rhi;

std::vector<Vertex> FullScreenTriangles(float left, float right, float top,
                                        float bottom, float depth) {
    const std::vector<Vertex> quad =
        q2_gpu_test::QuadVertices(left, right, top, bottom, depth);
    return {quad[0], quad[1], quad[2], quad[0], quad[2], quad[3]};
}

uint32_t ProgramId(void*, const char*) { return 1; }

bool TestMaterialBinding() {
    MaterialInput input;
    input.name = "fixture/diffuse";
    input.hasDiffuseMap = true;
    ProgramGate gate;
    gate.resolve = &ProgramId;
    MaterialPlan plan;
    std::string error;
    if (PlanMaterial(input, gate, &plan, &error) != Status::Ok || plan.stageCount != 1 ||
        plan.stages[0].kind != StageClass::Diffuse || !plan.needsTexture[0]) {
        q1b_test::Log("MATERIAL plan failed: %s", error.c_str());
        return false;
    }
    Texture diffuse;
    diffuse.id = 42;
    MaterialStageTextures textures;
    textures.diffuse = diffuse;
    BoundMaterialPlan bound;
    if (BindMaterialTextures(plan, textures, &bound, &error) != Status::Ok ||
        bound.stageCount != 1 || bound.stages[0].texture.id != 42) {
        q1b_test::Log("MATERIAL binding failed: %s", error.c_str());
        return false;
    }
    MaterialStageTextures missing;
    BoundMaterialPlan refused;
    if (BindMaterialTextures(plan, missing, &refused, &error) != Status::InvalidArgument ||
        refused.stageCount != 0) {
        q1b_test::Log("MATERIAL control: a declared map without a texture was accepted");
        return false;
    }
    q1b_test::Log("MATERIAL control rejected an unbound declared map: %s", error.c_str());
    return true;
}

int RunGeometry(Device& device) {
    std::string error;
    const ShaderModule vertex =
        q2_gpu_test::MakeShader(device, ShaderStage::Vertex, q2_gpu_test::kItemVertex,
                                "VSMain", &error);
    const ShaderModule pixel =
        q2_gpu_test::MakeShader(device, ShaderStage::Pixel, q2_gpu_test::kTintPixel,
                                "PSMain", &error);
    if (vertex.id == 0 || pixel.id == 0) {
        q1b_test::Log("GEOMETRY shader creation failed: %s", error.c_str());
        return 1;
    }
    Pipeline pipeline;
    if (!q2_gpu_test::CreateSurfacePipeline(device, vertex, pixel, false, &pipeline,
                                            &error)) {
        q1b_test::Log("GEOMETRY pipeline creation failed: %s", error.c_str());
        return 1;
    }

    Buffer redFull;
    Buffer greenLeft;
    Buffer greenLeftIndex;
    if (!q2_gpu_test::CreateVertexBuffer(device,
                                         FullScreenTriangles(-1.0f, 1.0f, 1.0f, -1.0f, 0.5f),
                                         &redFull, &error) ||
        !q2_gpu_test::CreateVertexBuffer(device,
                                         q2_gpu_test::QuadVertices(-1.0f, 0.0f, 1.0f,
                                                                   -1.0f, 0.5f),
                                         &greenLeft, &error) ||
        !q2_gpu_test::CreateIndexBuffer(device, {0, 1, 2, 0, 2, 3}, &greenLeftIndex,
                                        &error)) {
        q1b_test::Log("GEOMETRY buffer creation failed: %s", error.c_str());
        return 1;
    }

    auto makeRed = [&]() {
        DrawItem item;
        item.vertexBuffer = redFull;
        item.vertexCount = 6;
        item.tint[0] = 1.0f;
        item.tint[1] = 0.0f;
        item.tint[2] = 0.0f;
        item.tint[3] = 1.0f;
        return item;
    };
    auto makeGreen = [&]() {
        DrawItem item;
        item.vertexBuffer = greenLeft;
        item.vertexCount = 4;
        item.indexBuffer = greenLeftIndex;
        item.indexCount = 6;
        item.tint[0] = 0.0f;
        item.tint[1] = 1.0f;
        item.tint[2] = 0.0f;
        item.tint[3] = 1.0f;
        return item;
    };
    auto submit = [&](std::vector<DrawItem> items) {
        uint32_t colourIndex = 0;
        ClearRow clear;
        clear.alpha = 255;
        DrawPassDesc pass;
        pass.pipeline = pipeline;
        pass.colorIndices = &colourIndex;
        pass.colorCount = 1;
        pass.colorClears = &clear;
        pass.items = items.data();
        pass.itemCount = static_cast<uint32_t>(items.size());
        DrawFrameDesc frame;
        frame.width = q2_gpu_test::kWidth;
        frame.height = q2_gpu_test::kHeight;
        frame.attachmentCount = 1;
        frame.passCount = 1;
        frame.passes = &pass;
        Submission submission;
        const Status status = device.SubmitDrawFrame(frame, &submission, &error);
        if (status != Status::Ok) {
            q1b_test::Log("GEOMETRY submit failed: %s", error.c_str());
            return Submission{};
        }
        return submission;
    };

    {
        Submission submission = submit({makeRed(), makeGreen()});
        if (submission.id == 0 || !q1b_test::WaitComplete(device, submission)) {
            return 1;
        }
        ReadbackResult image;
        if (device.ReadbackAttachment(submission, 0, &image, &error) != Status::Ok) {
            q1b_test::Log("GEOMETRY readback failed: %s", error.c_str());
            return 1;
        }
        q2_gpu_test::PrintPixel("ORDER red-then-green", 1, 1, image);
        q2_gpu_test::PrintPixel("ORDER red-then-green", 6, 6, image);
        if (!q2_gpu_test::PixelIs(image, 1, 1, 0, 255, 0, 255) ||
            !q2_gpu_test::PixelIs(image, 6, 6, 255, 0, 0, 255)) {
            q1b_test::Log("ORDER failed: the later green quad must cover the earlier red");
            return 1;
        }
        q1a_test::WriteBmp(q1a_test::EvidencePath("q2-draw-order-red-green.bmp"), image);
        if (device.TryRelease(submission, &error) != ReleaseStatus::Released) {
            q1b_test::Log("GEOMETRY release failed: %s", error.c_str());
            return 1;
        }
    }

    {
        Submission submission = submit({makeGreen(), makeRed()});
        if (submission.id == 0 || !q1b_test::WaitComplete(device, submission)) {
            return 1;
        }
        ReadbackResult image;
        if (device.ReadbackAttachment(submission, 0, &image, &error) != Status::Ok) {
            q1b_test::Log("GEOMETRY readback failed: %s", error.c_str());
            return 1;
        }
        q2_gpu_test::PrintPixel("ORDER green-then-red", 1, 1, image);
        if (!q2_gpu_test::PixelIs(image, 1, 1, 255, 0, 0, 255)) {
            q1b_test::Log("ORDER control failed: reversing the item order must reverse the winner");
            return 1;
        }
        q1a_test::WriteBmp(q1a_test::EvidencePath("q2-draw-order-green-red.bmp"), image);
        if (device.TryRelease(submission, &error) != ReleaseStatus::Released) {
            q1b_test::Log("GEOMETRY release failed: %s", error.c_str());
            return 1;
        }
    }

    // A vertex-input pipeline without items, and an item without a buffer, are
    // both refused before any GPU work.
    {
        uint32_t colourIndex = 0;
        ClearRow clear;
        clear.alpha = 255;
        DrawPassDesc pass;
        pass.pipeline = pipeline;
        pass.colorIndices = &colourIndex;
        pass.colorCount = 1;
        pass.colorClears = &clear;
        DrawFrameDesc frame;
        frame.width = q2_gpu_test::kWidth;
        frame.height = q2_gpu_test::kHeight;
        frame.attachmentCount = 1;
        frame.passCount = 1;
        frame.passes = &pass;
        Submission rejected;
        if (device.SubmitDrawFrame(frame, &rejected, &error) != Status::InvalidArgument) {
            q1b_test::Log("GEOMETRY control: a vertex-input pass without items was accepted");
            return 1;
        }
        q1b_test::Log("GEOMETRY control rejected an item-less vertex pass: %s",
                      error.c_str());
    }

    q1b_test::Log("T080 geometry PASS");
    return 0;
}

int RunTexture(Device& device) {
    if (!TestMaterialBinding()) {
        return 1;
    }
    std::string error;
    const ShaderModule vertex =
        q2_gpu_test::MakeShader(device, ShaderStage::Vertex, q2_gpu_test::kItemVertex,
                                "VSMain", &error);
    const ShaderModule texturedPixel =
        q2_gpu_test::MakeShader(device, ShaderStage::Pixel,
                                q2_gpu_test::kTexturedPixel, "PSMain", &error);
    const ShaderModule channelPixel =
        q2_gpu_test::MakeShader(device, ShaderStage::Pixel, q2_gpu_test::kChannelPixel,
                                "PSMain", &error);
    const ShaderModule twoChannelPixel =
        q2_gpu_test::MakeShader(device, ShaderStage::Pixel,
                                q2_gpu_test::kTwoChannelPixel, "PSMain", &error);
    if (vertex.id == 0 || texturedPixel.id == 0 || channelPixel.id == 0 ||
        twoChannelPixel.id == 0) {
        q1b_test::Log("TEXTURE shader creation failed: %s", error.c_str());
        return 1;
    }
    Pipeline texturedPipeline;
    Pipeline channelPipeline;
    Pipeline twoChannelPipeline;
    if (!q2_gpu_test::CreateSurfacePipeline(device, vertex, texturedPixel, true,
                                            &texturedPipeline, &error) ||
        !q2_gpu_test::CreateSurfacePipeline(device, vertex, channelPixel, true,
                                            &channelPipeline, &error) ||
        !q2_gpu_test::CreateSurfacePipeline(device, vertex, twoChannelPixel, true,
                                            &twoChannelPipeline, &error)) {
        q1b_test::Log("TEXTURE pipeline creation failed: %s", error.c_str());
        return 1;
    }

    Buffer quad;
    Buffer quadIndex;
    if (!q2_gpu_test::CreateVertexBuffer(device,
                                         q2_gpu_test::QuadVertices(-1.0f, 1.0f, 1.0f,
                                                                   -1.0f, 0.5f),
                                         &quad, &error) ||
        !q2_gpu_test::CreateIndexBuffer(device, {0, 1, 2, 0, 2, 3}, &quadIndex, &error)) {
        q1b_test::Log("TEXTURE buffer creation failed: %s", error.c_str());
        return 1;
    }

    auto sample = [&](const Pipeline& pipeline, const Texture& texture,
                      ReadbackResult* image, const char* label) {
        DrawItem item;
        item.vertexBuffer = quad;
        item.vertexCount = 4;
        item.indexBuffer = quadIndex;
        item.indexCount = 6;
        item.texture = texture;
        const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        std::memcpy(item.tint, white, sizeof(item.tint));
        uint32_t colourIndex = 0;
        ClearRow clear;
        clear.alpha = 255;
        DrawPassDesc pass;
        pass.pipeline = pipeline;
        pass.colorIndices = &colourIndex;
        pass.colorCount = 1;
        pass.colorClears = &clear;
        pass.items = &item;
        pass.itemCount = 1;
        DrawFrameDesc frame;
        frame.width = q2_gpu_test::kWidth;
        frame.height = q2_gpu_test::kHeight;
        frame.attachmentCount = 1;
        frame.passCount = 1;
        frame.passes = &pass;
        Submission submission;
        if (device.SubmitDrawFrame(frame, &submission, &error) != Status::Ok) {
            q1b_test::Log("%s submit failed: %s", label, error.c_str());
            return false;
        }
        if (!q1b_test::WaitComplete(device, submission) ||
            device.ReadbackAttachment(submission, 0, image, &error) != Status::Ok) {
            q1b_test::Log("%s readback failed: %s", label, error.c_str());
            return false;
        }
        (void)label;
        device.TryRelease(submission, &error);        return true;
    };

    // 2x2 RGBA8: red, green / blue, white. Point sampling maps each texel to a
    // 4x4 quadrant of the 8x8 target.
    const std::vector<uint8_t> rgba = {
        255, 0,   0,   255, 0,   255, 0, 255,
        0,   0,   255, 255, 255, 255, 255, 255,
    };
    const Texture rgbaTexture = q2_gpu_test::MakeRgba8Texture(
        device, 2, 2, rgba, TextureFormat::RGBA8Unorm, &error);
    if (rgbaTexture.id == 0) {
        q1b_test::Log("TEXTURE RGBA8 upload failed: %s", error.c_str());
        return 1;
    }
    ReadbackResult rgbaImage;
    if (!sample(texturedPipeline, rgbaTexture, &rgbaImage, "RGBA8")) {
        return 1;
    }
    q2_gpu_test::PrintPixel("RGBA8", 1, 1, rgbaImage);
    q2_gpu_test::PrintPixel("RGBA8", 5, 1, rgbaImage);
    q2_gpu_test::PrintPixel("RGBA8", 1, 5, rgbaImage);
    q2_gpu_test::PrintPixel("RGBA8", 5, 5, rgbaImage);
    if (!q2_gpu_test::PixelIs(rgbaImage, 1, 1, 255, 0, 0, 255) ||
        !q2_gpu_test::PixelIs(rgbaImage, 5, 1, 0, 255, 0, 255) ||
        !q2_gpu_test::PixelIs(rgbaImage, 1, 5, 0, 0, 255, 255) ||
        !q2_gpu_test::PixelIs(rgbaImage, 5, 5, 255, 255, 255, 255)) {
        q1b_test::Log("TEXTURE RGBA8 sampling/orientation failed");
        return 1;
    }
    q1a_test::WriteBmp(q1a_test::EvidencePath("q2-texture-rgba8.bmp"), rgbaImage);

    // sRGB decode: 128/255 sRGB is 0.21586 linear = 55/255.
    {
        const Texture srgb = q2_gpu_test::MakeRgba8Texture(
            device, 1, 1, {128, 128, 128, 255}, TextureFormat::RGBA8Srgb, &error);
        ReadbackResult image;
        if (srgb.id == 0 || !sample(texturedPipeline, srgb, &image,
                                    "SRGB")) {
            return 1;
        }
        q2_gpu_test::PrintPixel("SRGB", 0, 0, image);
        if (!q2_gpu_test::PixelIs(image, 0, 0, 55, 55, 55, 255, 2)) {
            q1b_test::Log("TEXTURE sRGB decode failed: expected 55,55,55");
            return 1;
        }
        const Texture unorm = q2_gpu_test::MakeRgba8Texture(
            device, 1, 1, {128, 128, 128, 255}, TextureFormat::RGBA8Unorm, &error);
        ReadbackResult control;
        if (unorm.id == 0 || !sample(texturedPipeline, unorm, &control,
                                     "SRGB control")) {
            return 1;
        }
        if (!q2_gpu_test::PixelIs(control, 0, 0, 128, 128, 128, 255)) {
            q1b_test::Log("TEXTURE RGBA8 control changed by the sRGB test");
            return 1;
        }
    }

    // R8 single channel, replicated by the program.
    {
        const Texture r8 = q2_gpu_test::MakeRgba8Texture(
            device, 1, 1, {200}, TextureFormat::R8Unorm, &error);
        ReadbackResult image;
        if (r8.id == 0 ||
            !sample(channelPipeline, r8, &image, "R8")) {
            return 1;
        }
        q2_gpu_test::PrintPixel("R8", 0, 0, image);
        if (!q2_gpu_test::PixelIs(image, 0, 0, 200, 200, 200, 255)) {
            q1b_test::Log("TEXTURE R8 sampling failed");
            return 1;
        }
    }

    // BC4 blocks: endpoint 0 -> 0, endpoint 255 -> 255. Indices are all zero,
    // so every texel is the first endpoint.
    {
        const std::vector<uint8_t> dark = {0, 255, 0, 0, 0, 0, 0, 0};
        const std::vector<uint8_t> bright = {255, 0, 0, 0, 0, 0, 0, 0};
        const Texture darkTexture = q2_gpu_test::MakeRgba8Texture(
            device, 4, 4, dark, TextureFormat::BC4Unorm, &error);
        const Texture brightTexture = q2_gpu_test::MakeRgba8Texture(
            device, 4, 4, bright, TextureFormat::BC4Unorm, &error);
        ReadbackResult darkImage;
        ReadbackResult brightImage;
        if (darkTexture.id == 0 || brightTexture.id == 0 ||
            !sample(channelPipeline, darkTexture, &darkImage, "BC4 dark") ||
            !sample(channelPipeline, brightTexture, &brightImage,
                    "BC4 bright")) {
            q1b_test::Log("TEXTURE BC4 creation failed: %s", error.c_str());
            return 1;
        }
        if (!q2_gpu_test::PixelIs(darkImage, 2, 2, 0, 0, 0, 255) ||
            !q2_gpu_test::PixelIs(brightImage, 2, 2, 255, 255, 255, 255)) {
            q1b_test::Log("TEXTURE BC4 sampling failed");
            return 1;
        }
        q1b_test::Log("TEXTURE BC4 endpoints sampled at 0 and 255");
    }

    // BC5 normal XY: red block 255, green block 0.
    {
        const std::vector<uint8_t> block = {
            255, 0, 0, 0, 0, 0, 0, 0,
            0, 255, 0, 0, 0, 0, 0, 0,
        };
        const Texture bc5 = q2_gpu_test::MakeRgba8Texture(
            device, 4, 4, block, TextureFormat::BC5Unorm, &error);
        ReadbackResult image;
        if (bc5.id == 0 ||
            !sample(twoChannelPipeline, bc5, &image, "BC5")) {
            q1b_test::Log("TEXTURE BC5 creation failed: %s", error.c_str());
            return 1;
        }
        q2_gpu_test::PrintPixel("BC5", 2, 2, image);
        if (!q2_gpu_test::PixelIs(image, 2, 2, 255, 0, 0, 255)) {
            q1b_test::Log("TEXTURE BC5 XY sampling failed");
            return 1;
        }
    }

    // BC7 sRGB with a full 8x8 + 4x4 chain, the Q4 pin. The encoded block is a
    // fixed test vector; the fixture asserts the resource accepts the format,
    // the mip chain and samples as one uniform opaque colour, and logs the
    // decoded RGB rather than inventing a texel decoder here.
    {
        std::vector<uint8_t> blocks(5 * 16, 0xFF);
        TextureDesc desc;
        desc.format = TextureFormat::BC7UnormSrgb;
        TextureMip mip0;
        mip0.width = 8;
        mip0.height = 8;
        mip0.bytes.assign(blocks.begin(), blocks.begin() + 64);
        TextureMip mip1;
        mip1.width = 4;
        mip1.height = 4;
        mip1.bytes.assign(blocks.begin() + 64, blocks.begin() + 80);
        desc.mips = {mip0, mip1};
        Texture bc7;
        if (device.CreateTexture(desc, &bc7, &error) != Status::Ok) {
            q1b_test::Log("TEXTURE BC7 creation failed: %s", error.c_str());
            return 1;
        }
        ReadbackResult image;
        if (!sample(texturedPipeline, bc7, &image, "BC7")) {
            return 1;
        }
        q2_gpu_test::PrintPixel("BC7", 1, 1, image);
        q2_gpu_test::PrintPixel("BC7", 6, 6, image);
        if (!q2_gpu_test::PixelIs(image, 1, 1, 255, 255, 255, 255) ||
            !q2_gpu_test::PixelIs(image, 6, 6, 255, 255, 255, 255)) {
            q1b_test::Log("TEXTURE BC7 sampling failed: the uniform 0xFF block must decode "
                          "to opaque white");
            return 1;
        }
        if (image.pixels.size() < static_cast<size_t>(image.height) * image.rowStride) {
            q1b_test::Log("TEXTURE BC7 readback is short");
            return 1;
        }
        if (image.pixels[3] != 255) {
            q1b_test::Log("TEXTURE BC7 alpha is not opaque");
            return 1;
        }
        // Invalid mip chains are refused before upload.
        TextureDesc broken = desc;
        broken.mips[1].width = 8;
        Texture rejected;
        if (device.CreateTexture(broken, &rejected, &error) != Status::InvalidArgument) {
            q1b_test::Log("TEXTURE control: a malformed mip chain was accepted");
            return 1;
        }
        q1b_test::Log("TEXTURE control rejected a malformed mip chain");
    }

    q1b_test::Log("T081 texture PASS");
    return 0;
}

int RunStencil(Device& device) {
    std::string error;
    const ShaderModule vertex =
        q2_gpu_test::MakeShader(device, ShaderStage::Vertex, q2_gpu_test::kItemVertex,
                                "VSMain", &error);
    const ShaderModule tint = q2_gpu_test::MakeShader(
        device, ShaderStage::Pixel, q2_gpu_test::kTintPixel, "PSMain", &error);
    const ShaderModule fullscreen = q2_gpu_test::MakeShader(
        device, ShaderStage::Vertex, q1b_test::kFullscreenVertex, "VSMain", &error);
    const ShaderModule uniform = q2_gpu_test::MakeShader(
        device, ShaderStage::Pixel, q2_gpu_test::kUniformPixel, "PSMain", &error);
    if (vertex.id == 0 || tint.id == 0 || fullscreen.id == 0 || uniform.id == 0) {
        q1b_test::Log("STENCIL shader creation failed: %s", error.c_str());
        return 1;
    }
    Pipeline backInc;
    Pipeline frontDec;
    Pipeline shade;
    if (!q2_gpu_test::CreateStencilPipeline(device, vertex, tint, 0, CullMode::Front,
                                            StencilOp::IncrementSaturate, true, 0xFF,
                                            &backInc, &error) ||
        !q2_gpu_test::CreateStencilPipeline(device, vertex, tint, 0, CullMode::Back,
                                            StencilOp::DecrementSaturate, true, 0xFF,
                                            &frontDec, &error) ||
        !q2_gpu_test::CreateShadePipeline(device, fullscreen, uniform, &shade, &error)) {
        q1b_test::Log("STENCIL pipeline creation failed: %s", error.c_str());
        return 1;
    }

    Buffer backVertices;
    Buffer backIndices;
    Buffer frontVertices;
    Buffer frontIndices;
    if (!q2_gpu_test::CreateVertexBuffer(
            device, q2_gpu_test::QuadVertices(-0.5f, 0.5f, 1.0f, -1.0f, 0.5f),
            &backVertices, &error) ||
        !q2_gpu_test::CreateIndexBuffer(device, q2_gpu_test::QuadIndicesBothWindings(),
                                        &backIndices, &error) ||
        !q2_gpu_test::CreateVertexBuffer(
            device, q2_gpu_test::QuadVertices(-0.5f, -0.1f, 1.0f, -1.0f, 0.5f),
            &frontVertices, &error) ||
        !q2_gpu_test::CreateIndexBuffer(device, q2_gpu_test::QuadIndicesBothWindings(),
                                        &frontIndices, &error)) {
        q1b_test::Log("STENCIL buffer creation failed: %s", error.c_str());
        return 1;
    }

    auto submitShadow = [&](const Pipeline& backPipeline,
                            const Pipeline& frontPipeline, ReadbackResult* image) {
        DrawItem backItem;
        backItem.vertexBuffer = backVertices;
        backItem.vertexCount = 4;
        backItem.indexBuffer = backIndices;
        backItem.indexCount = 12;
        DrawItem frontItem;
        frontItem.vertexBuffer = frontVertices;
        frontItem.vertexCount = 4;
        frontItem.indexBuffer = frontIndices;
        frontItem.indexCount = 12;

        uint32_t colourIndex = 0;
        ClearRow clear;
        clear.red = 10;
        clear.green = 10;
        clear.blue = 10;
        clear.alpha = 255;
        DrawPassDesc passes[3];
        passes[0].pipeline = backPipeline;
        passes[0].items = &backItem;
        passes[0].itemCount = 1;
        passes[1].pipeline = frontPipeline;
        passes[1].items = &frontItem;
        passes[1].itemCount = 1;
        passes[2].pipeline = shade;
        passes[2].colorIndices = &colourIndex;
        passes[2].colorCount = 1;
        passes[2].colorClears = &clear;
        passes[2].tint[0] = 200.0f / 255.0f;
        passes[2].tint[1] = 180.0f / 255.0f;
        passes[2].tint[2] = 40.0f / 255.0f;
        passes[2].tint[3] = 1.0f;

        DrawFrameDesc frame;
        frame.width = q2_gpu_test::kWidth;
        frame.height = q2_gpu_test::kHeight;
        frame.attachmentCount = 1;
        frame.depthEnabled = true;
        frame.depthClear = 1.0f;
        frame.passCount = 3;
        frame.passes = passes;
        Submission submission;
        if (device.SubmitDrawFrame(frame, &submission, &error) != Status::Ok) {
            q1b_test::Log("STENCIL submit failed: %s", error.c_str());
            return false;
        }
        if (!q1b_test::WaitComplete(device, submission) ||
            device.ReadbackAttachment(submission, 0, image, &error) != Status::Ok) {
            q1b_test::Log("STENCIL readback failed: %s", error.c_str());
            return false;
        }
        device.TryRelease(submission, &error);
        return true;
    };

    ReadbackResult image;
    if (!submitShadow(backInc, frontDec, &image)) {
        return 1;
    }
    q2_gpu_test::PrintPixel("STENCIL", 1, 1, image);
    q2_gpu_test::PrintPixel("STENCIL", 3, 1, image);
    q2_gpu_test::PrintPixel("STENCIL", 4, 1, image);
    q2_gpu_test::PrintPixel("STENCIL", 6, 1, image);
    // Back-only region (columns 4-5) stays at the clear colour; the overlap
    // (columns 2-3) and the outside are lit.
    if (!q2_gpu_test::PixelIs(image, 4, 1, 10, 10, 10, 255) ||
        !q2_gpu_test::PixelIs(image, 5, 1, 10, 10, 10, 255) ||
        !q2_gpu_test::PixelIs(image, 3, 1, 200, 180, 40, 255) ||
        !q2_gpu_test::PixelIs(image, 1, 1, 200, 180, 40, 255) ||
        !q2_gpu_test::PixelIs(image, 6, 1, 200, 180, 40, 255)) {
        q1b_test::Log("STENCIL CP-07 pattern failed");
        return 1;
    }
    q1a_test::WriteBmp(q1a_test::EvidencePath("q2-stencil-shadow.bmp"), image);
    q1b_test::Log("STENCIL CP-07 back-increment/front-decrement PASS");

    // Negative control: swapping the ops flips the shadowed columns.
    Pipeline backDec;
    Pipeline frontInc;
    if (!q2_gpu_test::CreateStencilPipeline(device, vertex, tint, 0, CullMode::Front,
                                            StencilOp::DecrementSaturate, true, 0xFF,
                                            &backDec, &error) ||
        !q2_gpu_test::CreateStencilPipeline(device, vertex, tint, 0, CullMode::Back,
                                            StencilOp::IncrementSaturate, true, 0xFF,
                                            &frontInc, &error)) {
        q1b_test::Log("STENCIL control pipeline creation failed: %s", error.c_str());
        return 1;
    }
    ReadbackResult flipped;
    if (!submitShadow(backDec, frontInc, &flipped)) {
        return 1;
    }
    if (!q2_gpu_test::PixelIs(flipped, 4, 1, 200, 180, 40, 255) ||
        !q2_gpu_test::PixelIs(flipped, 3, 1, 10, 10, 10, 255)) {
        q1b_test::Log("STENCIL control: swapping the stencil ops did not flip the result");
        return 1;
    }
    q1b_test::Log("STENCIL control flipped with swapped ops");

    // Exclusion plan: a no-shadows light emits no volume pass at all.
    {
        LightInput light;
        light.kind = LightKind::Point;
        light.pointLight = true;
        light.falloffRadius = 128.0f;
        light.noShadows = false;
        MaterialShadowInput material;
        material.noShadows = true;
        LightPlan plan;
        ShadowVolumePlan volume;
        if (PlanLight(light, material, false, &plan, &error) != Status::Ok ||
            PlanShadowVolume(plan, &volume, &error) != Status::Ok || volume.enabled ||
            volume.facePassCount != 0) {
            q1b_test::Log("STENCIL exclusion control: a no-shadows light still emits volumes");
            return 1;
        }
        MaterialShadowInput casting;
        if (PlanLight(light, casting, true, &plan, &error) != Status::Ok ||
            PlanShadowVolume(plan, &volume, &error) != Status::Ok || !volume.enabled ||
            volume.facePassCount != 2 || volume.depthClampUsed ||
            !volume.requiresClosedVolume) {
            q1b_test::Log("STENCIL plan: a single-chain light did not produce two face passes");
            return 1;
        }
        LightPlan suppressed;
        ShadowVolumePlan suppressedVolume;
        LightInput ambient;
        ambient.kind = LightKind::Ambient;
        if (PlanLight(ambient, casting, false, &suppressed, &error) != Status::Ok ||
            PlanShadowVolume(suppressed, &suppressedVolume, &error) != Status::Ok ||
            suppressedVolume.enabled) {
            q1b_test::Log("STENCIL plan: the ambient pass owns a shadow volume");
            return 1;
        }
    }

    q1b_test::Log("T082 stencil PASS");
    return 0;
}

class DeviceBackend : public SceneGeometryBackend {
public:
    explicit DeviceBackend(Device& device) : device_(device) {}

    bool Upload(uint32_t, const SceneSurfaceGeometry& geometry, SceneMeshHandles* handles,
                std::string* error) override {
        if (!q2_gpu_test::CreateVertexBuffer(device_, geometry.vertices,
                                             &handles->vertexBuffer, error)) {
            return false;
        }
        handles->vertexCount = static_cast<uint32_t>(geometry.vertices.size());
        if (!geometry.indices.empty()) {
            if (!q2_gpu_test::CreateIndexBuffer(device_, geometry.indices,
                                                &handles->indexBuffer, error)) {
                return false;
            }
            handles->indexCount = static_cast<uint32_t>(geometry.indices.size());
        }
        return true;
    }

    Status Submit(const DrawFrameDesc& frame, Submission* submission,
                  std::string* error) override {
        return device_.SubmitDrawFrame(frame, submission, error);
    }

private:
    Device& device_;
};

class SyntheticScene : public SceneGeometrySource {
public:
    SyntheticScene(Texture white, std::vector<Vertex> fullScreen,
                   std::vector<Vertex> leftHalf, std::vector<Vertex> backVolume,
                   std::vector<Vertex> frontVolume)
        : white_(white),
          fullScreen_(std::move(fullScreen)),
          leftHalf_(std::move(leftHalf)),
          backVolume_(std::move(backVolume)),
          frontVolume_(std::move(frontVolume)) {}

    bool ResolveSurface(uint32_t surfaceId, SceneSurfaceGeometry* geometry) override {
        if (surfaceId == 1) {
            geometry->vertices = fullScreen_;
            geometry->indices.clear();
            geometry->tint[0] = 1.0f;
            geometry->tint[1] = 0.0f;
            geometry->tint[2] = 0.0f;
        } else if (surfaceId == 2) {
            geometry->vertices = leftHalf_;
            geometry->indices = {0, 1, 2, 0, 2, 3};
            geometry->tint[0] = 0.0f;
            geometry->tint[1] = 1.0f;
            geometry->tint[2] = 0.0f;
        } else {
            return false;
        }
        geometry->texture = white_;
        return true;
    }

    bool ResolveLight(const LightPacket& packet, SceneLightGeometry* geometry) override {
        if (packet.lightId != 7) {
            return false;
        }
        geometry->volumes[0].vertices = backVolume_;
        geometry->volumes[0].indices = QuadIndices();
        geometry->volumes[1].vertices = frontVolume_;
        geometry->volumes[1].indices = QuadIndices();
        geometry->shade.vertices.clear();
        geometry->tint[0] = 200.0f / 255.0f;
        geometry->tint[1] = 180.0f / 255.0f;
        geometry->tint[2] = 40.0f / 255.0f;
        geometry->tint[3] = 1.0f;
        return true;
    }

private:
    static std::vector<uint16_t> QuadIndices() {
        return q2_gpu_test::QuadIndicesBothWindings();
    }

    Texture white_;
    std::vector<Vertex> fullScreen_;
    std::vector<Vertex> leftHalf_;
    std::vector<Vertex> backVolume_;
    std::vector<Vertex> frontVolume_;
};

int RunScene(Device& device) {
    std::string error;
    const ShaderModule vertex =
        q2_gpu_test::MakeShader(device, ShaderStage::Vertex, q2_gpu_test::kItemVertex,
                                "VSMain", &error);
    const ShaderModule textured =
        q2_gpu_test::MakeShader(device, ShaderStage::Pixel,
                                q2_gpu_test::kTexturedPixel, "PSMain", &error);
    const ShaderModule tint = q2_gpu_test::MakeShader(
        device, ShaderStage::Pixel, q2_gpu_test::kTintPixel, "PSMain", &error);
    const ShaderModule fullscreen = q2_gpu_test::MakeShader(
        device, ShaderStage::Vertex, q1b_test::kFullscreenVertex, "VSMain", &error);
    const ShaderModule uniform = q2_gpu_test::MakeShader(
        device, ShaderStage::Pixel, q2_gpu_test::kUniformPixel, "PSMain", &error);
    if (vertex.id == 0 || textured.id == 0 || tint.id == 0 || fullscreen.id == 0 ||
        uniform.id == 0) {
        q1b_test::Log("SCENE shader creation failed: %s", error.c_str());
        return 1;
    }
    Pipeline surface;
    Pipeline backInc;
    Pipeline frontDec;
    Pipeline shade;
    if (!q2_gpu_test::CreateSurfacePipeline(device, vertex, textured, true, &surface,
                                            &error) ||
        !q2_gpu_test::CreateStencilPipeline(device, vertex, tint, 0, CullMode::Front,
                                            StencilOp::IncrementSaturate, true, 0xFF,
                                            &backInc, &error) ||
        !q2_gpu_test::CreateStencilPipeline(device, vertex, tint, 0, CullMode::Back,
                                            StencilOp::DecrementSaturate, true, 0xFF,
                                            &frontDec, &error) ||
        !q2_gpu_test::CreateShadePipeline(device, fullscreen, uniform, &shade, &error)) {
        q1b_test::Log("SCENE pipeline creation failed: %s", error.c_str());
        return 1;
    }

    const Texture white = q2_gpu_test::MakeRgba8Texture(
        device, 1, 1, {255, 255, 255, 255}, TextureFormat::RGBA8Unorm, &error);
    if (white.id == 0) {
        q1b_test::Log("SCENE white texture failed: %s", error.c_str());
        return 1;
    }
    const std::vector<Vertex> fullScreen =
        FullScreenTriangles(-1.0f, 1.0f, 1.0f, -1.0f, 0.5f);
    const std::vector<Vertex> leftHalf =
        q2_gpu_test::QuadVertices(-1.0f, 0.0f, 1.0f, -1.0f, 0.5f);
    const std::vector<Vertex> backVolume =
        q2_gpu_test::QuadVertices(-0.5f, 0.5f, 1.0f, -1.0f, 0.5f);
    const std::vector<Vertex> frontVolume =
        q2_gpu_test::QuadVertices(-0.5f, -0.1f, 1.0f, -1.0f, 0.5f);

    SyntheticScene geometry(white, fullScreen, leftHalf, backVolume, frontVolume);
    DeviceBackend backend(device);

    SceneCommandQueue queue;
    if (!queue.BeginView([] {
            ViewDesc view;
            view.identity = 1;
            view.width = q2_gpu_test::kWidth;
            view.height = q2_gpu_test::kHeight;
            return view;
        }())) {
        q1b_test::Log("SCENE BeginView failed");
        return 1;
    }
    DrawSurfPacket first;
    first.surfaceId = 1;
    first.vertexCount = 6;
    first.stageCount = 1;
    DrawSurfPacket second;
    second.surfaceId = 2;
    second.vertexCount = 4;
    second.stageCount = 1;
    if (!queue.Draw(first) || !queue.Draw(second)) {
        q1b_test::Log("SCENE Draw failed");
        return 1;
    }
    LightPacket casting;
    casting.lightId = 7;
    casting.light.kind = LightKind::Point;
    casting.light.pointLight = true;
    casting.light.falloffRadius = 128.0f;
    casting.sameEntity = true;
    LightPacket excluded = casting;
    excluded.lightId = 8;
    excluded.material.noShadows = true;
    if (!queue.DrawLight(casting) || !queue.DrawLight(excluded) || !queue.EndView()) {
        q1b_test::Log("SCENE light queue failed");
        return 1;
    }

    SceneAdapterDesc desc;
    desc.displayWidth = q2_gpu_test::kWidth;
    desc.displayHeight = q2_gpu_test::kHeight;
    desc.structuralOnly = false;
    desc.backend = &backend;
    desc.geometry = &geometry;
    desc.surfacePipeline = surface;
    desc.shadowBackPipeline = backInc;
    desc.shadowFrontPipeline = frontDec;
    desc.lightShadePipeline = shade;
    SceneAdapter adapter;
    if (adapter.Initialize(desc, &error) != Status::Ok) {
        q1b_test::Log("SCENE initialize failed: %s", error.c_str());
        return 1;
    }
    SceneExecuteResult result;
    if (adapter.Execute(&queue, &result, &error) != Status::Ok) {
        q1b_test::Log("SCENE execute failed: %s", error.c_str());
        return 1;
    }
    q1b_test::Log(
        "SCENE draws extracted=%u submitted=%u refused=%u lights=%u excluded=%u "
        "refused=%u passes=%u submissions=%u unavailable=%d",
        result.drawsExtracted, result.drawsSubmitted, result.drawsRefused,
        result.lightsExtracted, result.lightsExcluded, result.lightsRefused,
        result.lightPassesSubmitted, static_cast<unsigned>(result.submissions.size()),
        result.drawSubmissionUnavailable ? 1 : 0);
    if (result.drawsExtracted != 2 || result.drawsSubmitted != 2 ||
        result.drawsRefused != 0 || result.lightsExtracted != 2 ||
        result.lightsExcluded != 1 || result.lightsRefused != 0 ||
        result.lightPassesSubmitted != 3 || result.submissions.size() != 2 ||
        result.drawSubmissionUnavailable) {
        q1b_test::Log("SCENE counts do not match the expected submission plan");
        return 1;
    }
    if (result.submissionAttachments[0] != 0 || result.submissionAttachments[1] != 0) {
        q1b_test::Log("SCENE submission attachment identity changed");
        return 1;
    }
    ReadbackResult surfaces;
    ReadbackResult light;
    if (!q1b_test::WaitComplete(device, result.submissions[0]) ||
        !q1b_test::WaitComplete(device, result.submissions[1]) ||
        device.ReadbackAttachment(result.submissions[0], 0, &surfaces, &error) !=
            Status::Ok ||
        device.ReadbackAttachment(result.submissions[1], 0, &light, &error) != Status::Ok) {
        q1b_test::Log("SCENE readback failed: %s", error.c_str());
        return 1;
    }
    q2_gpu_test::PrintPixel("SCENE surface", 1, 1, surfaces);
    q2_gpu_test::PrintPixel("SCENE surface", 6, 6, surfaces);
    q2_gpu_test::PrintPixel("SCENE light", 4, 1, light);
    q2_gpu_test::PrintPixel("SCENE light", 1, 1, light);
    if (!q2_gpu_test::PixelIs(surfaces, 1, 1, 0, 255, 0, 255) ||
        !q2_gpu_test::PixelIs(surfaces, 6, 6, 255, 0, 0, 255)) {
        q1b_test::Log("SCENE surface order failed");
        return 1;
    }
    if (!q2_gpu_test::PixelIs(light, 4, 1, 0, 0, 0, 255) ||
        !q2_gpu_test::PixelIs(light, 1, 1, 200, 180, 40, 255)) {
        q1b_test::Log("SCENE light shadow frame failed");
        return 1;
    }
    q1a_test::WriteBmp(q1a_test::EvidencePath("q2-scene-surfaces.bmp"), surfaces);
    q1a_test::WriteBmp(q1a_test::EvidencePath("q2-scene-light.bmp"), light);
    q1b_test::Log("SCENE PASS: two ordered surface submissions then one light frame");
    return 0;
}

int Dispatch(const std::string& mode, Device& device) {
    if (mode == "geometry") {
        return RunGeometry(device);
    }
    if (mode == "texture") {
        return RunTexture(device);
    }
    if (mode == "stencil") {
        return RunStencil(device);
    }
    if (mode == "scene") {
        return RunScene(device);
    }
    q1b_test::Log("unknown mode '%s'", mode.c_str());
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "geometry";
    return q1b_test::ForEachAdapter(
        [&mode](doom3::rhi::Device& device) { return Dispatch(mode, device); });
}
