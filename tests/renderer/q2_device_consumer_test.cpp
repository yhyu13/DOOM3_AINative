// Q2 device-consumer fixtures (spec 004 T080/T081/T082).
//
// These drive the same sink callback table the engine installs, but through
// EngineQ2DeviceConsumer instead of a test-local backend:
//
//   texture: an engine image upload becomes a device texture, the material
//            stage selection binds it, and the ordered draw samples it;
//   shadow:  a shadow-volume packet is attached to its light packet and the
//            adapter submits the back/front stencil passes plus the shade pass;
//   stream:  two ordered textured surfaces, a two-stage material whose diffuse
//            slot is stage 1, and an image replacement in the same cache.
//
// The content is synthetic. A host without a device device returns 77 (SKIP); no
// PASS is claimed without a readback. No retail data and no live engine frame is
// involved.

#include "q1b_test_support.h"
#include "q2_gpu_test_support.h"

#include "rhi/engine_q2_sink_device.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

namespace rhi = doom3::rhi;
using rhi::EngineQ2DeviceConsumer;
using rhi::EngineQ2DevicePipelines;
using rhi::Pipeline;
using rhi::Q2SinkCallbacks;
using rhi::Q2SinkImageUpload;
using rhi::Q2SinkLightFacts;
using rhi::Q2SinkMaterialFacts;
using rhi::Q2SinkShadowVolumePacket;
using rhi::Q2SinkSurfaceGeometry;
using rhi::ShaderModule;
using rhi::Status;
using rhi::Texture;
using rhi::Vertex;

constexpr uint32_t kWidth = 8;
constexpr uint32_t kHeight = 8;

float Identity[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                      0.0f, 1.0f, 0.0f, 0.0f,
                      0.0f, 0.0f, 1.0f, 0.0f,
                      0.0f, 0.0f, 0.0f, 1.0f};

struct Pipelines {
    Pipeline surface;
    Pipeline back;
    Pipeline front;
    Pipeline shade;
};

bool CreatePipelines(rhi::Device& device, Pipelines* pipelines,
                     std::string* error) {
    const ShaderModule vertex = q2_gpu_test::MakeShader(
        device, rhi::ShaderStage::Vertex, q2_gpu_test::kItemVertex, "VSMain",
        error);
    const ShaderModule textured = q2_gpu_test::MakeShader(
        device, rhi::ShaderStage::Pixel, q2_gpu_test::kTexturedPixel, "PSMain",
        error);
    const ShaderModule tint = q2_gpu_test::MakeShader(
        device, rhi::ShaderStage::Pixel, q2_gpu_test::kTintPixel, "PSMain",
        error);
    const ShaderModule fullscreen = q2_gpu_test::MakeShader(
        device, rhi::ShaderStage::Vertex, q1b_test::kFullscreenVertex, "VSMain",
        error);
    const ShaderModule uniform = q2_gpu_test::MakeShader(
        device, rhi::ShaderStage::Pixel, q2_gpu_test::kUniformPixel, "PSMain",
        error);
    if (vertex.id == 0 || textured.id == 0 || tint.id == 0 ||
        fullscreen.id == 0 || uniform.id == 0) {
        return false;
    }
    return q2_gpu_test::CreateSurfacePipeline(device, vertex, textured, true,
                                              &pipelines->surface, error) &&
           q2_gpu_test::CreateStencilPipeline(
               device, vertex, tint, 0, rhi::CullMode::Front,
               rhi::StencilOp::IncrementSaturate, true, 0xFF, &pipelines->back,
               error) &&
           q2_gpu_test::CreateStencilPipeline(
               device, vertex, tint, 0, rhi::CullMode::Back,
               rhi::StencilOp::DecrementSaturate, true, 0xFF, &pipelines->front,
               error) &&
           q2_gpu_test::CreateShadePipeline(device, fullscreen, uniform,
                                            &pipelines->shade, error);
}

EngineQ2DevicePipelines ToDesc(const Pipelines& pipelines) {
    EngineQ2DevicePipelines desc;
    desc.surface = pipelines.surface;
    desc.shadowBack = pipelines.back;
    desc.shadowFront = pipelines.front;
    desc.shade = pipelines.shade;
    desc.surfaceAttachmentIndex = 0;
    desc.lightAttachmentIndex = 0;
    return desc;
}

Q2SinkMaterialFacts MaterialWithImage(uint32_t imageId) {
    Q2SinkMaterialFacts facts;
    facts.numStages = 1;
    facts.hasTexture = true;
    facts.hasProgram = true;
    facts.coverage = 1;  // MC_OPAQUE
    facts.textures.numStages = 1;
    facts.textures.hasImage[0] = true;
    facts.textures.imageId[0] = imageId;
    facts.textures.filter[0] = 1;
    facts.textures.repeat[0] = 1;
    return facts;
}

Q2SinkMaterialFacts TwoStageDiffuse(uint32_t imageId) {
    Q2SinkMaterialFacts facts;
    facts.numStages = 2;
    facts.hasTexture = true;
    facts.hasProgram = true;
    facts.coverage = 1;
    facts.textures.numStages = 2;
    // Stage 0 is the bump slot and has no image; stage 1 is the diffuse slot.
    facts.textures.hasImage[1] = true;
    facts.textures.imageId[1] = imageId;
    facts.textures.filter[1] = 1;
    facts.textures.repeat[1] = 1;
    return facts;
}

Q2SinkSurfaceGeometry Quad(float left, float right, float top, float bottom,
                           float depth) {
    Q2SinkSurfaceGeometry geometry;
    geometry.vertices = q2_gpu_test::QuadVertices(left, right, top, bottom, depth);
    geometry.indices = q2_gpu_test::QuadIndicesBothWindings();
    std::memcpy(geometry.transform, Identity, sizeof(geometry.transform));
    return geometry;
}

Q2SinkImageUpload RgbaImage(uint32_t imageId, int width, int height,
                            std::vector<uint8_t> pixels) {
    Q2SinkImageUpload image;
    image.imageId = imageId;
    image.name = "synthetic/device";
    image.width = width;
    image.height = height;
    image.mipCount = 1;
    image.format = 0;
    image.sRGB = false;
    image.minFilter = 3;
    image.magFilter = 3;
    image.wrapS = 1;
    image.wrapT = 1;
    image.bytesPerRow = width * 4;
    image.pixels = std::move(pixels);
    return image;
}

Q2SinkLightFacts CastingLightFacts() {
    Q2SinkLightFacts light;
    light.pointLight = true;
    light.hasProjection = false;
    light.falloffRadius = 128.0f;
    light.chainKind = 2;
    light.chainSurfaceCount = 1;
    light.chainMaterialNoSelfShadow = true;
    light.globalShadows = 1;
    light.localShadows = 1;
    return light;
}

Q2SinkLightFacts ExcludedLightFacts() {
    Q2SinkLightFacts light;
    light.pointLight = true;
    light.falloffRadius = 128.0f;
    light.noShadows = true;
    light.chainKind = 0;
    light.excluded = true;
    light.exclusionKind = 1;
    return light;
}

Q2SinkShadowVolumePacket ShadowPacket() {
    Q2SinkShadowVolumePacket packet;
    packet.chainKind = 2;
    packet.tint[0] = 1.0f;
    packet.tint[1] = 0.0f;
    packet.tint[2] = 0.0f;
    packet.tint[3] = 1.0f;
    packet.back = Quad(-0.5f, 0.5f, 1.0f, -1.0f, 0.5f);
    packet.front = Quad(-0.5f, -0.1f, 1.0f, -1.0f, 0.5f);
    return packet;
}

int RunTexture(rhi::Device& device) {
    std::string error;
    Pipelines pipelines;
    if (!CreatePipelines(device, &pipelines, &error)) {
        q1b_test::Log("DEVICE texture pipeline creation failed: %s", error.c_str());
        return 1;
    }
    EngineQ2DeviceConsumer consumer(device);
    if (!consumer.Initialize(kWidth, kHeight, ToDesc(pipelines), &error)) {
        q1b_test::Log("DEVICE texture initialize failed: %s", error.c_str());
        return 1;
    }
    const Q2SinkCallbacks* sink = consumer.sink().callbacks();

    Q2SinkImageUpload upload = RgbaImage(
        7u, 2, 2, {255, 0, 0, 255, 0, 255, 0, 255,
                   0, 0, 255, 255, 255, 255, 255, 255});
    if (!sink->imageUpload(sink->user, &upload)) {
        q1b_test::Log("DEVICE imageUpload refused: %s",
                      consumer.sink().lastError().c_str());
        return 1;
    }
    if (consumer.textures().size() != 1u ||
        !consumer.textures().Find(7u, nullptr)) {
        q1b_test::Log("DEVICE texture cache did not retain the upload");
        return 1;
    }

    if (!sink->beginView(sink->user, 1u, 0, 0, kWidth, kHeight, false, false,
                         Identity)) {
        q1b_test::Log("DEVICE beginView refused");
        return 1;
    }
    const Q2SinkSurfaceGeometry quad = Quad(-1.0f, 1.0f, 1.0f, -1.0f, 0.5f);
    if (!sink->surfacePacket(sink->user, 10u, &quad)) {
        q1b_test::Log("DEVICE surfacePacket refused");
        return 1;
    }
    // A declared stage image with no upload must refuse the draw rather than
    // sampling the previously bound texture.
    const Q2SinkMaterialFacts missing = MaterialWithImage(99u);
    if (sink->drawPacket(sink->user, 10u, &missing, 4, 12, 0, 0.0f, false,
                         false)) {
        q1b_test::Log("DEVICE control: a declared map with no upload was accepted");
        return 1;
    }
    q1b_test::Log("DEVICE control rejected an unuploaded declared map: %s",
                  consumer.sink().lastError().c_str());

    const Q2SinkMaterialFacts facts = MaterialWithImage(7u);
    if (!sink->drawPacket(sink->user, 10u, &facts, 4, 12, 0, 0.0f, false,
                          false)) {
        q1b_test::Log("DEVICE drawPacket refused: %s",
                      consumer.sink().lastError().c_str());
        return 1;
    }
    if (consumer.sink().surfaceStageIndex(10u) != 0u) {
        q1b_test::Log("DEVICE selected stage %u, expected the only stage 0",
                      consumer.sink().surfaceStageIndex(10u));
        return 1;
    }
    if (!sink->endView(sink->user) || !sink->swapBuffers(sink->user)) {
        q1b_test::Log("DEVICE texture frame refused: %s",
                      consumer.sink().lastError().c_str());
        return 1;
    }
    const rhi::SceneExecuteResult& result = consumer.sink().lastResult();
    q1b_test::Log(
        "DEVICE texture draws extracted=%u submitted=%u refused=%u "
        "submissions=%u unavailable=%d",
        result.drawsExtracted, result.drawsSubmitted, result.drawsRefused,
        static_cast<unsigned>(result.submissions.size()),
        result.drawSubmissionUnavailable ? 1 : 0);
    if (result.drawsExtracted != 1u || result.drawsSubmitted != 1u ||
        result.drawsRefused != 0u || result.submissions.size() != 1u ||
        result.drawSubmissionUnavailable) {
        q1b_test::Log("DEVICE texture counts do not match the expected frame");
        return 1;
    }
    if (!q1b_test::WaitComplete(device, result.submissions[0])) {
        return 1;
    }
    rhi::ReadbackResult image;
    if (device.ReadbackAttachment(result.submissions[0], 0, &image, &error) !=
        Status::Ok) {
        q1b_test::Log("DEVICE texture readback failed: %s", error.c_str());
        return 1;
    }
    q2_gpu_test::PrintPixel("DEVICE texture", 1, 1, image);
    q2_gpu_test::PrintPixel("DEVICE texture", 5, 1, image);
    q2_gpu_test::PrintPixel("DEVICE texture", 1, 5, image);
    q2_gpu_test::PrintPixel("DEVICE texture", 5, 5, image);
    if (!q2_gpu_test::PixelIs(image, 1, 1, 255, 0, 0, 255) ||
        !q2_gpu_test::PixelIs(image, 5, 1, 0, 255, 0, 255) ||
        !q2_gpu_test::PixelIs(image, 1, 5, 0, 0, 255, 255) ||
        !q2_gpu_test::PixelIs(image, 5, 5, 255, 255, 255, 255)) {
        q1b_test::Log("DEVICE texture sampling failed: the four quadrants changed");
        return 1;
    }
    q1a_test::WriteBmp(q1a_test::EvidencePath("q2-device-texture.bmp"), image);

    // A replacement upload under the same identity must be used by the next
    // frame, and the cache must still hold one entry.
    Q2SinkImageUpload replacement =
        RgbaImage(7u, 1, 1, {255, 255, 0, 255});
    if (!sink->imageUpload(sink->user, &replacement)) {
        q1b_test::Log("DEVICE replacement upload refused: %s",
                      consumer.sink().lastError().c_str());
        return 1;
    }
    if (consumer.textures().size() != 1u) {
        q1b_test::Log("DEVICE replacement duplicated the cache entry");
        return 1;
    }
    if (!sink->beginView(sink->user, 1u, 0, 0, kWidth, kHeight, false, false,
                         Identity) ||
        !sink->surfacePacket(sink->user, 10u, &quad) ||
        !sink->drawPacket(sink->user, 10u, &facts, 4, 12, 0, 0.0f, false,
                          false) ||
        !sink->endView(sink->user) || !sink->swapBuffers(sink->user)) {
        q1b_test::Log("DEVICE replacement frame refused: %s",
                      consumer.sink().lastError().c_str());
        return 1;
    }
    const rhi::SceneExecuteResult& second = consumer.sink().lastResult();
    if (second.submissions.size() != 1u ||
        !q1b_test::WaitComplete(device, second.submissions[0])) {
        return 1;
    }
    rhi::ReadbackResult replaced;
    if (device.ReadbackAttachment(second.submissions[0], 0, &replaced,
                                  &error) != Status::Ok) {
        q1b_test::Log("DEVICE replacement readback failed: %s", error.c_str());
        return 1;
    }
    if (!q2_gpu_test::PixelIs(replaced, 1, 1, 255, 255, 0, 255) ||
        !q2_gpu_test::PixelIs(replaced, 6, 6, 255, 255, 0, 255)) {
        q1b_test::Log("DEVICE replacement texture was not sampled");
        return 1;
    }
    q2_gpu_test::PrintPixel("DEVICE replacement", 1, 1, replaced);
    q1a_test::WriteBmp(q1a_test::EvidencePath("q2-device-texture-replaced.bmp"),
                       replaced);

    // A format with no portable device format is refused; the cache keeps the
    // accepted texture and the sink reports the actionable reason.
    Q2SinkImageUpload bc1 = RgbaImage(8u, 4, 4, std::vector<uint8_t>(16, 0));
    bc1.format = 2;  // BC1
    bc1.bytesPerRow = 4;
    if (sink->imageUpload(sink->user, &bc1)) {
        q1b_test::Log("DEVICE control: an unmapped image format was accepted");
        return 1;
    }
    if (consumer.textures().size() != 1u) {
        q1b_test::Log("DEVICE control: a refused format changed the cache");
        return 1;
    }
    q1b_test::Log("DEVICE control rejected an unmapped format: %s",
                  consumer.sink().lastError().c_str());

    // Exactly the missing declared map and the unmapped format were refused.
    if (consumer.sink().refusedCallbacks() != 2u) {
        q1b_test::Log("DEVICE refused %u callbacks, expected 2",
                      consumer.sink().refusedCallbacks());
        return 1;
    }

    q1b_test::Log("DEVICE texture PASS: image upload -> device texture -> stage draw");
    return 0;
}

int RunShadow(rhi::Device& device) {
    std::string error;
    Pipelines pipelines;
    if (!CreatePipelines(device, &pipelines, &error)) {
        q1b_test::Log("DEVICE shadow pipeline creation failed: %s", error.c_str());
        return 1;
    }
    EngineQ2DeviceConsumer consumer(device);
    if (!consumer.Initialize(kWidth, kHeight, ToDesc(pipelines), &error)) {
        q1b_test::Log("DEVICE shadow initialize failed: %s", error.c_str());
        return 1;
    }
    const Q2SinkCallbacks* sink = consumer.sink().callbacks();

    if (!sink->beginView(sink->user, 1u, 0, 0, kWidth, kHeight, false, false,
                         Identity)) {
        q1b_test::Log("DEVICE shadow beginView refused");
        return 1;
    }
    // The volume packet precedes its light packet, exactly as a surface packet
    // precedes its draw packet. The consumer attaches it by light identity.
    const Q2SinkShadowVolumePacket volume = ShadowPacket();
    if (!consumer.sink().ShadowVolumePacket(volume)) {
        q1b_test::Log("DEVICE shadow volume refused: %s",
                      consumer.sink().lastError().c_str());
        return 1;
    }
    const Q2SinkLightFacts casting = CastingLightFacts();
    const Q2SinkLightFacts excluded = ExcludedLightFacts();
    if (!sink->lightPacket(sink->user, &casting) ||
        !sink->lightPacket(sink->user, &excluded)) {
        q1b_test::Log("DEVICE shadow lightPacket refused: %s",
                      consumer.sink().lastError().c_str());
        return 1;
    }
    if (!sink->endView(sink->user) || !sink->swapBuffers(sink->user)) {
        q1b_test::Log("DEVICE shadow frame refused: %s",
                      consumer.sink().lastError().c_str());
        return 1;
    }
    // ResolveLight runs during execution, so the resolved count is checked
    // after the frame.
    if (consumer.sink().shadowVolumePackets() != 1u ||
        consumer.sink().shadowVolumesResolved() != 1u) {
        q1b_test::Log("DEVICE shadow volume was not attached to its light");
        return 1;
    }
    const rhi::SceneExecuteResult& result = consumer.sink().lastResult();
    q1b_test::Log(
        "DEVICE shadow lights extracted=%u excluded=%u refused=%u passes=%u "
        "submissions=%u",
        result.lightsExtracted, result.lightsExcluded, result.lightsRefused,
        result.lightPassesSubmitted,
        static_cast<unsigned>(result.submissions.size()));
    if (result.lightsExtracted != 2u || result.lightsExcluded != 1u ||
        result.lightsRefused != 0u || result.lightPassesSubmitted != 3u ||
        result.submissions.size() != 1u) {
        q1b_test::Log("DEVICE shadow counts do not match the expected frame");
        return 1;
    }
    if (!q1b_test::WaitComplete(device, result.submissions[0])) {
        return 1;
    }
    rhi::ReadbackResult image;
    if (device.ReadbackAttachment(result.submissions[0], 0, &image, &error) !=
        Status::Ok) {
        q1b_test::Log("DEVICE shadow readback failed: %s", error.c_str());
        return 1;
    }
    q2_gpu_test::PrintPixel("DEVICE shadow", 4, 1, image);
    q2_gpu_test::PrintPixel("DEVICE shadow", 1, 1, image);
    // The back-only columns keep the clear colour; the overlap and the outside
    // are shaded by the packet tint.
    if (!q2_gpu_test::PixelIs(image, 4, 1, 0, 0, 0, 255) ||
        !q2_gpu_test::PixelIs(image, 1, 1, 255, 0, 0, 255)) {
        q1b_test::Log("DEVICE shadow pattern failed");
        return 1;
    }
    q1a_test::WriteBmp(q1a_test::EvidencePath("q2-device-shadow.bmp"), image);

    // Control: a volume for the local chain cannot be attached to a global
    // packet. The light is refused before it can be queued.
    if (!sink->beginView(sink->user, 1u, 0, 0, kWidth, kHeight, false, false,
                         Identity) ||
        !consumer.sink().ShadowVolumePacket(volume)) {
        return 1;
    }
    Q2SinkLightFacts wrongChain = casting;
    wrongChain.chainKind = 1;
    if (sink->lightPacket(sink->user, &wrongChain)) {
        q1b_test::Log("DEVICE control: a volume was attached to the wrong chain");
        return 1;
    }
    if (consumer.sink().refusedCallbacks() != 1u) {
        q1b_test::Log("DEVICE shadow refused %u callbacks, expected 1",
                      consumer.sink().refusedCallbacks());
        return 1;
    }
    q1b_test::Log("DEVICE control rejected a chain mismatch: %s",
                  consumer.sink().lastError().c_str());

    q1b_test::Log("DEVICE shadow PASS: volume packet -> stencil passes -> shade");
    return 0;
}

int RunStream(rhi::Device& device) {
    std::string error;
    Pipelines pipelines;
    if (!CreatePipelines(device, &pipelines, &error)) {
        q1b_test::Log("DEVICE stream pipeline creation failed: %s", error.c_str());
        return 1;
    }
    EngineQ2DeviceConsumer consumer(device);
    if (!consumer.Initialize(kWidth, kHeight, ToDesc(pipelines), &error)) {
        q1b_test::Log("DEVICE stream initialize failed: %s", error.c_str());
        return 1;
    }
    const Q2SinkCallbacks* sink = consumer.sink().callbacks();

    Q2SinkImageUpload red = RgbaImage(20u, 1, 1, {255, 0, 0, 255});
    Q2SinkImageUpload green = RgbaImage(21u, 1, 1, {0, 255, 0, 255});
    if (!sink->imageUpload(sink->user, &red) ||
        !sink->imageUpload(sink->user, &green)) {
        q1b_test::Log("DEVICE stream uploads refused: %s",
                      consumer.sink().lastError().c_str());
        return 1;
    }
    if (consumer.textures().size() != 2u) {
        q1b_test::Log("DEVICE stream cache did not hold both images");
        return 1;
    }

    if (!sink->beginView(sink->user, 1u, 0, 0, kWidth, kHeight, false, false,
                         Identity)) {
        return 1;
    }
    // Surface 30 samples image 20 through its only stage; surface 31 samples
    // image 21 through the diffuse slot (stage 1) of a two-stage material.
    const Q2SinkSurfaceGeometry full = Quad(-1.0f, 1.0f, 1.0f, -1.0f, 0.5f);
    const Q2SinkSurfaceGeometry left = Quad(-1.0f, 0.0f, 1.0f, -1.0f, 0.5f);
    const Q2SinkMaterialFacts single = MaterialWithImage(20u);
    const Q2SinkMaterialFacts twoStage = TwoStageDiffuse(21u);
    if (!sink->surfacePacket(sink->user, 30u, &full) ||
        !sink->drawPacket(sink->user, 30u, &single, 4, 12, 0, 0.0f, false,
                          false) ||
        !sink->surfacePacket(sink->user, 31u, &left) ||
        !sink->drawPacket(sink->user, 31u, &twoStage, 4, 12, 0, 0.0f, false,
                          false)) {
        q1b_test::Log("DEVICE stream draw refused: %s",
                      consumer.sink().lastError().c_str());
        return 1;
    }
    if (consumer.sink().surfaceStageIndex(30u) != 0u ||
        consumer.sink().surfaceStageIndex(31u) != 1u) {
        q1b_test::Log("DEVICE stream selected stages %u/%u, expected 0/1",
                      consumer.sink().surfaceStageIndex(30u),
                      consumer.sink().surfaceStageIndex(31u));
        return 1;
    }
    if (!sink->endView(sink->user) || !sink->swapBuffers(sink->user)) {
        q1b_test::Log("DEVICE stream frame refused: %s",
                      consumer.sink().lastError().c_str());
        return 1;
    }
    const rhi::SceneExecuteResult& result = consumer.sink().lastResult();
    q1b_test::Log("DEVICE stream draws extracted=%u submitted=%u refused=%u "
                  "submissions=%u",
                  result.drawsExtracted, result.drawsSubmitted,
                  result.drawsRefused,
                  static_cast<unsigned>(result.submissions.size()));
    if (result.drawsExtracted != 2u || result.drawsSubmitted != 2u ||
        result.drawsRefused != 0u || result.submissions.size() != 1u) {
        q1b_test::Log("DEVICE stream counts do not match the expected frame");
        return 1;
    }
    if (!q1b_test::WaitComplete(device, result.submissions[0])) {
        return 1;
    }
    rhi::ReadbackResult image;
    if (device.ReadbackAttachment(result.submissions[0], 0, &image, &error) !=
        Status::Ok) {
        q1b_test::Log("DEVICE stream readback failed: %s", error.c_str());
        return 1;
    }
    q2_gpu_test::PrintPixel("DEVICE stream", 1, 1, image);
    q2_gpu_test::PrintPixel("DEVICE stream", 6, 6, image);
    if (!q2_gpu_test::PixelIs(image, 1, 1, 0, 255, 0, 255) ||
        !q2_gpu_test::PixelIs(image, 6, 6, 255, 0, 0, 255)) {
        q1b_test::Log("DEVICE stream order or stage binding failed");
        return 1;
    }
    q1a_test::WriteBmp(q1a_test::EvidencePath("q2-device-stream.bmp"), image);
    q1b_test::Log("DEVICE stream PASS: two ordered textured surfaces");
    return 0;
}

int Dispatch(const std::string& mode, rhi::Device& device) {
    if (mode == "texture") {
        return RunTexture(device);
    }
    if (mode == "shadow") {
        return RunShadow(device);
    }
    if (mode == "stream") {
        return RunStream(device);
    }
    q1b_test::Log("unknown mode '%s'", mode.c_str());
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "texture";
    return q1b_test::ForEachAdapter(
        [&mode](doom3::rhi::Device& device) { return Dispatch(mode, device); });
}
