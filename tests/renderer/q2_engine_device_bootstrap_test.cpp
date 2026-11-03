// Engine bootstrap fixture (spec 004 T080/T081/T082).
//
// R_InitRenderApi cannot name Device or Pipeline, so its initialize callback
// calls EngineQ2DeviceRuntime. This fixture exercises exactly that object: it
// creates the hardware device, compiles the four classic pipelines, initializes
// EngineQ2DeviceConsumer and then drives the same callback table the engine
// registration unit converts. It proves the "device and pipelines exist when the
// installed initialize callback is reached" half with a real readback; it does
// not launch the engine, and a fresh launch is refused before this callback
// because no window surface exists. No retail data, no engine frame, no Q2
// acceptance.
//
// A host with no hardware device returns 77 (SKIP). No PASS is claimed without
// a readback.

#include "q1b_test_support.h"
#include "q2_gpu_test_support.h"

#include "rhi/engine_q2_device_bootstrap.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

namespace rhi = doom3::rhi;
using rhi::EngineQ2DeviceRuntime;
using rhi::Q2SinkImageUpload;
using rhi::Q2SinkLightFacts;
using rhi::Q2SinkMaterialFacts;
using rhi::Q2SinkSurfaceGeometry;
using rhi::Status;
using rhi::Texture;

constexpr uint32_t kWidth = 8;
constexpr uint32_t kHeight = 8;

float Identity[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                      0.0f, 1.0f, 0.0f, 0.0f,
                      0.0f, 0.0f, 1.0f, 0.0f,
                      0.0f, 0.0f, 0.0f, 1.0f};

Q2SinkSurfaceGeometry Quad(float left, float right, float top, float bottom,
                           float depth) {
    Q2SinkSurfaceGeometry geometry;
    geometry.vertices =
        q2_gpu_test::QuadVertices(left, right, top, bottom, depth);
    geometry.indices = q2_gpu_test::QuadIndicesBothWindings();
    std::memcpy(geometry.transform, Identity, sizeof(geometry.transform));
    return geometry;
}

Q2SinkImageUpload RgbaImage(uint32_t imageId, int width, int height,
                            std::vector<uint8_t> pixels) {
    Q2SinkImageUpload image;
    image.imageId = imageId;
    image.name = "synthetic/engine-bootstrap";
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

Q2SinkMaterialFacts TexturedMaterial(uint32_t imageId) {
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

Q2SinkLightFacts CastingLight() {
    Q2SinkLightFacts light;
    light.pointLight = true;
    light.falloffRadius = 128.0f;
    light.chainKind = 2;
    light.chainSurfaceCount = 1;
    light.chainMaterialNoSelfShadow = true;
    light.globalShadows = 1;
    light.localShadows = 1;
    return light;
}

Q2SinkLightFacts ExcludedLight() {
    Q2SinkLightFacts light;
    light.pointLight = true;
    light.falloffRadius = 128.0f;
    light.noShadows = true;
    light.chainKind = 0;
    light.excluded = true;
    light.exclusionKind = 1;
    return light;
}

}  // namespace

int main() {
    q1b_test::Log("BOOTSTRAP start: hardware device probe");
    std::string probeError;
    {
        rhi::Device probe;
        rhi::DeviceOptions options;
        options.adapterClass = rhi::AdapterClass::Hardware;
        options.adapterIndex = 0;
        const Status status = probe.Initialize(options, &probeError);
        if (status == Status::Unsupported) {
            q1b_test::Log("BOOTSTRAP SKIPPED: %s", probeError.c_str());
            return q1a_test::kSkipExitCode;
        }
        if (status != Status::Ok) {
            q1b_test::Log("BOOTSTRAP probe failed: %s", probeError.c_str());
            return 1;
        }
        probe.Shutdown();
    }

    EngineQ2DeviceRuntime runtime;
    std::string error;
    if (!runtime.Create(kWidth, kHeight, &error)) {
        q1b_test::Log("BOOTSTRAP create failed: %s", error.c_str());
        return 1;
    }
    q1b_test::Log("BOOTSTRAP device=%s class=%s",
                  runtime.device().adapter().name.c_str(),
                  rhi::ToString(runtime.device().adapter().adapterClass));

    // Control: the pipeline factory refuses a missing output descriptor rather
    // than returning a half-filled one.
    if (rhi::CreateEngineQ2ClassicPipelines(runtime.device(), nullptr, &error)) {
        q1b_test::Log("BOOTSTRAP control: a null pipeline descriptor was accepted");
        return 1;
    }

    rhi::EngineQ2DeviceConsumer& consumer = runtime.consumer();
    const rhi::Q2SinkCallbacks* sink = consumer.sink().callbacks();

    // T081: an engine image upload reaches the device cache through the same
    // callback the engine registration unit installs.
    Q2SinkImageUpload upload =
        RgbaImage(7u, 2, 2, {255, 0, 0, 255, 0, 255, 0, 255,
                             0, 0, 255, 255, 255, 255, 255, 255});
    if (!sink->imageUpload(sink->user, &upload)) {
        q1b_test::Log("BOOTSTRAP imageUpload refused: %s",
                      consumer.sink().lastError().c_str());
        return 1;
    }
    Texture texture;
    if (consumer.textures().size() != 1u ||
        !consumer.textures().Find(7u, &texture)) {
        q1b_test::Log("BOOTSTRAP texture cache did not retain the upload");
        return 1;
    }

    // T080/T081: an ordered textured surface, submitted through the pipelines
    // the bootstrap created.
    const Q2SinkSurfaceGeometry quad = Quad(-1.0f, 1.0f, 1.0f, -1.0f, 0.5f);
    const Q2SinkMaterialFacts material = TexturedMaterial(7u);
    if (!sink->beginView(sink->user, 1u, 0, 0, kWidth, kHeight, false, false,
                         Identity) ||
        !sink->surfacePacket(sink->user, 10u, &quad) ||
        !sink->drawPacket(sink->user, 10u, &material, 4, 12, 0, 0.0f, false,
                          false) ||
        !sink->endView(sink->user) || !sink->swapBuffers(sink->user)) {
        q1b_test::Log("BOOTSTRAP texture frame refused: %s",
                      consumer.sink().lastError().c_str());
        return 1;
    }
    const rhi::SceneExecuteResult& result = consumer.sink().lastResult();
    q1b_test::Log(
        "BOOTSTRAP texture draws extracted=%u submitted=%u refused=%u "
        "submissions=%u",
        result.drawsExtracted, result.drawsSubmitted, result.drawsRefused,
        static_cast<unsigned>(result.submissions.size()));
    if (result.drawsExtracted != 1u || result.drawsSubmitted != 1u ||
        result.drawsRefused != 0u || result.submissions.size() != 1u ||
        result.drawSubmissionUnavailable) {
        q1b_test::Log("BOOTSTRAP texture counts do not match the expected frame");
        return 1;
    }
    if (!q1b_test::WaitComplete(runtime.device(), result.submissions[0])) {
        return 1;
    }
    rhi::ReadbackResult image;
    if (runtime.device().ReadbackAttachment(result.submissions[0], 0, &image,
                                            &error) != Status::Ok) {
        q1b_test::Log("BOOTSTRAP texture readback failed: %s", error.c_str());
        return 1;
    }
    q2_gpu_test::PrintPixel("BOOTSTRAP texture", 1, 1, image);
    q2_gpu_test::PrintPixel("BOOTSTRAP texture", 5, 5, image);
    if (!q2_gpu_test::PixelIs(image, 1, 1, 255, 0, 0, 255) ||
        !q2_gpu_test::PixelIs(image, 5, 1, 0, 255, 0, 255) ||
        !q2_gpu_test::PixelIs(image, 1, 5, 0, 0, 255, 255) ||
        !q2_gpu_test::PixelIs(image, 5, 5, 255, 255, 255, 255)) {
        q1b_test::Log("BOOTSTRAP texture sampling failed: the four quadrants changed");
        return 1;
    }
    q1a_test::WriteBmp(
        q1a_test::EvidencePath("q2-engine-device-bootstrap-texture.bmp"), image);

    // T082: a shadow-volume packet precedes its light packet. Both are sent
    // through the same callbacks the engine registration unit converts.
    if (!sink->beginView(sink->user, 1u, 0, 0, kWidth, kHeight, false, false,
                         Identity)) {
        return 1;
    }
    rhi::Q2SinkShadowVolumePacket volume;
    volume.chainKind = 2;
    volume.tint[0] = 1.0f;
    volume.tint[1] = 0.0f;
    volume.tint[2] = 0.0f;
    volume.tint[3] = 1.0f;
    volume.back = Quad(-0.5f, 0.5f, 1.0f, -1.0f, 0.5f);
    volume.front = Quad(-0.5f, -0.1f, 1.0f, -1.0f, 0.5f);
    if (!consumer.sink().ShadowVolumePacket(volume)) {
        q1b_test::Log("BOOTSTRAP shadow volume refused: %s",
                      consumer.sink().lastError().c_str());
        return 1;
    }
    const Q2SinkLightFacts casting = CastingLight();
    const Q2SinkLightFacts excluded = ExcludedLight();
    if (!sink->lightPacket(sink->user, &casting) ||
        !sink->lightPacket(sink->user, &excluded) ||
        !sink->endView(sink->user) || !sink->swapBuffers(sink->user)) {
        q1b_test::Log("BOOTSTRAP shadow frame refused: %s",
                      consumer.sink().lastError().c_str());
        return 1;
    }
    if (consumer.sink().shadowVolumePackets() != 1u ||
        consumer.sink().shadowVolumesResolved() != 1u) {
        q1b_test::Log("BOOTSTRAP shadow volume was not attached to its light");
        return 1;
    }
    const rhi::SceneExecuteResult& shadow = consumer.sink().lastResult();
    q1b_test::Log(
        "BOOTSTRAP shadow lights extracted=%u excluded=%u refused=%u "
        "passes=%u submissions=%u",
        shadow.lightsExtracted, shadow.lightsExcluded, shadow.lightsRefused,
        shadow.lightPassesSubmitted,
        static_cast<unsigned>(shadow.submissions.size()));
    if (shadow.lightsExtracted != 2u || shadow.lightsExcluded != 1u ||
        shadow.lightsRefused != 0u || shadow.lightPassesSubmitted != 3u ||
        shadow.submissions.size() != 1u) {
        q1b_test::Log("BOOTSTRAP shadow counts do not match the expected frame");
        return 1;
    }
    if (!q1b_test::WaitComplete(runtime.device(), shadow.submissions[0])) {
        return 1;
    }
    rhi::ReadbackResult shadowImage;
    if (runtime.device().ReadbackAttachment(shadow.submissions[0], 0,
                                            &shadowImage,
                                            &error) != Status::Ok) {
        q1b_test::Log("BOOTSTRAP shadow readback failed: %s", error.c_str());
        return 1;
    }
    q2_gpu_test::PrintPixel("BOOTSTRAP shadow", 4, 1, shadowImage);
    q2_gpu_test::PrintPixel("BOOTSTRAP shadow", 1, 1, shadowImage);
    if (!q2_gpu_test::PixelIs(shadowImage, 4, 1, 0, 0, 0, 255) ||
        !q2_gpu_test::PixelIs(shadowImage, 1, 1, 255, 0, 0, 255)) {
        q1b_test::Log("BOOTSTRAP shadow pattern failed");
        return 1;
    }
    q1a_test::WriteBmp(
        q1a_test::EvidencePath("q2-engine-device-bootstrap-shadow.bmp"),
        shadowImage);

    runtime.Shutdown();
    if (runtime.created()) {
        q1b_test::Log("BOOTSTRAP shutdown left the runtime created");
        return 1;
    }
    q1b_test::Log("BOOTSTRAP PASS: initialize-callback device/pipelines -> sink -> readback (live launch NOT RUN)");
    return 0;
}
