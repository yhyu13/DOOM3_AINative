// Engine-fork seam fixture (spec 004 T080-T082/T044).
//
// This test drives the same portable callback table the engine registration
// unit installs, with the fields the fork call sites now fill:
//
//   surfacePacket   ordered world geometry (T080)
//   imageUpload     texture pixels and sampler metadata (T081)
//   lightPacket     one packet per shadow chain, with the exclusion facts (T082)
//   privatePose     the private skinned pose snapshot (T044)
//
// It uses synthetic content and a real device. The engine-fork call sites that
// produce these values are compiled by the target build; this fixture proves the
// portable consumer executes the values and that the adapter keeps its order.
// A host without a device returns 77 (SKIP): no PASS is claimed without a
// readback.

#include "q1b_test_support.h"
#include "q2_gpu_test_support.h"
#include "rhi/engine_q2_sink.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

namespace rhi = doom3::rhi;
using rhi::DrawFrameDesc;
using rhi::DrawItem;
using rhi::DrawPassDesc;
using rhi::EngineQ2SinkConsumer;
using rhi::EngineQ2SinkDesc;
using rhi::MotionVec3;
using rhi::Pipeline;
using rhi::Q2SinkCallbacks;
using rhi::Q2SinkImageUpload;
using rhi::Q2SinkLightFacts;
using rhi::Q2SinkMaterialFacts;
using rhi::Q2SinkPrivatePose;
using rhi::Q2SinkSurfaceGeometry;
using rhi::SceneGeometryBackend;
using rhi::SceneGeometrySource;
using rhi::SceneLightGeometry;
using rhi::SceneMeshHandles;
using rhi::SceneSurfaceGeometry;
using rhi::ShaderModule;
using rhi::Status;
using rhi::Texture;
using rhi::Vertex;
using rhi::ViewKind;

constexpr uint32_t kWidth = 8;
constexpr uint32_t kHeight = 8;

class DeviceBackend : public SceneGeometryBackend {
public:
    explicit DeviceBackend(rhi::Device& device) : device_(device) {}

    bool Upload(uint32_t, const SceneSurfaceGeometry& geometry,
                SceneMeshHandles* handles, std::string* error) override {
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

    Status Submit(const DrawFrameDesc& frame, rhi::Submission* submission,
                  std::string* error) override {
        return device_.SubmitDrawFrame(frame, submission, error);
    }

private:
    rhi::Device& device_;
};

// The engine seam does not carry shadow-volume geometry yet. This fixture
// supplies the same closed back/front volume the Q2 scene fixture uses, so the
// exclusion and chain facts are exercised end to end. It is not an engine
// geometry source and is never used by the target build.
class ForkLightGeometry : public SceneGeometrySource {
public:
    explicit ForkLightGeometry(std::vector<Vertex> back, std::vector<Vertex> front)
        : back_(std::move(back)), front_(std::move(front)) {}

    bool ResolveSurface(uint32_t, SceneSurfaceGeometry*) override { return false; }

    bool ResolveLight(const rhi::LightPacket& packet,
                      SceneLightGeometry* geometry) override {
        // Only the casting packet (first light identity) gets a volume; the
        // excluded packet must never reach here.
        if (packet.lightId != 1u) {
            return false;
        }
        geometry->volumes[0].vertices = back_;
        geometry->volumes[0].indices = q2_gpu_test::QuadIndicesBothWindings();
        geometry->volumes[1].vertices = front_;
        geometry->volumes[1].indices = q2_gpu_test::QuadIndicesBothWindings();
        geometry->shade.vertices.clear();
        geometry->tint[0] = 1.0f;
        geometry->tint[1] = 0.0f;
        geometry->tint[2] = 0.0f;
        geometry->tint[3] = 1.0f;
        return true;
    }

private:
    std::vector<Vertex> back_;
    std::vector<Vertex> front_;
};

Q2SinkMaterialFacts SurfaceFacts() {
    Q2SinkMaterialFacts facts;
    facts.numStages = 1;
    facts.hasTexture = true;
    facts.hasProgram = true;
    facts.textures.numStages = 1;
    facts.textures.hasImage[0] = true;
    facts.textures.imageId[0] = 1u;
    facts.textures.filter[0] = 1;   // TF_LINEAR in the engine's ordering
    facts.textures.repeat[0] = 1;   // TR_REPEAT
    return facts;
}

Q2SinkLightFacts CastingLightFacts() {
    Q2SinkLightFacts light;
    light.pointLight = true;
    light.hasProjection = false;
    light.falloffRadius = 128.0f;
    light.chainKind = 2;  // local chain
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
    light.exclusionKind = 1;  // light noShadows
    return light;
}

float IdentityProjection[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 1.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 1.0f, 0.0f,
                                0.0f, 0.0f, 0.0f, 1.0f};

int RunForkSeam(rhi::Device& device) {
    std::string error;
    const ShaderModule vertex = q2_gpu_test::MakeShader(
        device, rhi::ShaderStage::Vertex, q2_gpu_test::kItemVertex, "VSMain",
        &error);
    const ShaderModule tint = q2_gpu_test::MakeShader(
        device, rhi::ShaderStage::Pixel, q2_gpu_test::kTintPixel, "PSMain",
        &error);
    const ShaderModule fullscreen = q2_gpu_test::MakeShader(
        device, rhi::ShaderStage::Vertex, q1b_test::kFullscreenVertex, "VSMain",
        &error);
    const ShaderModule uniform = q2_gpu_test::MakeShader(
        device, rhi::ShaderStage::Pixel, q2_gpu_test::kUniformPixel, "PSMain",
        &error);
    Pipeline surface;
    Pipeline backInc;
    Pipeline frontDec;
    Pipeline shade;
    if (vertex.id == 0 || tint.id == 0 || fullscreen.id == 0 ||
        uniform.id == 0 ||
        !q2_gpu_test::CreateSurfacePipeline(device, vertex, tint, false,
                                            &surface, &error) ||
        !q2_gpu_test::CreateStencilPipeline(device, vertex, tint, 0,
                                            rhi::CullMode::Front,
                                            rhi::StencilOp::IncrementSaturate,
                                            true, 0xFF, &backInc, &error) ||
        !q2_gpu_test::CreateStencilPipeline(device, vertex, tint, 0,
                                            rhi::CullMode::Back,
                                            rhi::StencilOp::DecrementSaturate,
                                            true, 0xFF, &frontDec, &error) ||
        !q2_gpu_test::CreateShadePipeline(device, fullscreen, uniform, &shade,
                                          &error)) {
        q1b_test::Log("FORK pipeline creation failed: %s", error.c_str());
        return 1;
    }

    ForkLightGeometry lightSource(
        q2_gpu_test::QuadVertices(-0.5f, 0.5f, 1.0f, -1.0f, 0.5f),
        q2_gpu_test::QuadVertices(-0.5f, -0.1f, 1.0f, -1.0f, 0.5f));
    DeviceBackend backend(device);

    EngineQ2SinkDesc desc;
    desc.structuralOnly = false;
    desc.backend = &backend;
    desc.lightGeometrySource = &lightSource;
    desc.captureGeometry = true;
    desc.captureImageUploads = true;
    desc.surfacePipeline = surface;
    desc.shadowBackPipeline = backInc;
    desc.shadowFrontPipeline = frontDec;
    desc.lightShadePipeline = shade;
    desc.surfaceAttachmentIndex = 0;
    desc.lightAttachmentIndex = 0;
    EngineQ2SinkConsumer consumer(desc);
    if (!consumer.Initialize(kWidth, kHeight, &error)) {
        q1b_test::Log("FORK initialize failed: %s", error.c_str());
        return 1;
    }
    const Q2SinkCallbacks* sink = consumer.callbacks();

    // T081: a 2x2 RGBA8 upload with a known byte layout.
    Q2SinkImageUpload upload;
    upload.imageId = 1u;
    upload.name = "synthetic/checker";
    upload.width = 2;
    upload.height = 2;
    upload.mipCount = 1;
    upload.format = 0;
    upload.sRGB = false;
    upload.minFilter = 3;
    upload.magFilter = 3;
    upload.wrapS = 1;
    upload.wrapT = 1;
    upload.bytesPerRow = 8;
    upload.pixels = {255, 0, 0, 255, 0, 255, 0, 255,
                     0, 0, 255, 255, 255, 255, 255, 255};
    if (!sink->imageUpload(sink->user, &upload)) {
        q1b_test::Log("FORK imageUpload refused");
        return 1;
    }
    if (consumer.imageUploads().size() != 1u) {
        q1b_test::Log("FORK image upload was not retained");
        return 1;
    }
    const Q2SinkImageUpload& storedUpload = consumer.imageUploads().at(1u);
    if (storedUpload.pixels.size() != 16u ||
        storedUpload.pixels[0] != 255u || storedUpload.pixels[5] != 255u ||
        storedUpload.bytesPerRow != 8) {
        q1b_test::Log("FORK image upload bytes changed");
        return 1;
    }

    if (!sink->beginView(sink->user, 1u, 0, 0, kWidth, kHeight, false, false,
                         IdentityProjection)) {
        q1b_test::Log("FORK beginView refused");
        return 1;
    }

    // T080: ordered world geometry. The engine sends the snapshot immediately
    // before the draw packet for the same id.
    Q2SinkSurfaceGeometry geometry;
    geometry.vertices = q2_gpu_test::QuadVertices(-1.0f, 1.0f, 1.0f, -1.0f,
                                                   0.5f);
    geometry.indices = q2_gpu_test::QuadIndicesBothWindings();
    std::memcpy(geometry.transform, IdentityProjection,
                sizeof(geometry.transform));
    if (!sink->surfacePacket(sink->user, 10u, &geometry)) {
        q1b_test::Log("FORK surfacePacket refused");
        return 1;
    }
    const Q2SinkMaterialFacts facts = SurfaceFacts();
    if (!sink->drawPacket(sink->user, 10u, &facts,
                          static_cast<int>(geometry.vertices.size()),
                          static_cast<int>(geometry.indices.size()), 0, 0.0f,
                          false, false)) {
        q1b_test::Log("FORK drawPacket refused");
        return 1;
    }

    // T082: one local shadow chain, then one excluded light.
    const Q2SinkLightFacts casting = CastingLightFacts();
    const Q2SinkLightFacts excluded = ExcludedLightFacts();
    if (!sink->lightPacket(sink->user, &casting) ||
        !sink->lightPacket(sink->user, &excluded)) {
        q1b_test::Log("FORK lightPacket refused");
        return 1;
    }

    // T044: a private pose committed by the engine-side call site. The portable
    // history returns it for the matching generation and refuses a newer one.
    Q2SinkPrivatePose pose;
    pose.viewId = 1u;
    pose.viewKind = 0u;
    pose.surfaceId = 77u;
    pose.generation = 1u;
    pose.positions = {MotionVec3{1.0, 2.0, 3.0},
                      MotionVec3{4.0, 5.0, 6.0}};
    if (!sink->privatePose(sink->user, &pose)) {
        q1b_test::Log("FORK privatePose refused");
        return 1;
    }
    std::vector<MotionVec3> previous;
    if (consumer.PreviousPrivatePose(ViewKind::Main, 77u, 1u, &previous) !=
            rhi::MotionValidity::Valid ||
        previous.size() != 2u || previous[1].z != 6.0) {
        q1b_test::Log("FORK private pose was not retained");
        return 1;
    }
    if (consumer.PreviousPrivatePose(ViewKind::Main, 77u, 2u, &previous) !=
        rhi::MotionValidity::Generation) {
        q1b_test::Log("FORK private pose did not reject a stale generation");
        return 1;
    }

    if (!sink->endView(sink->user)) {
        q1b_test::Log("FORK endView refused");
        return 1;
    }
    if (!sink->swapBuffers(sink->user)) {
        q1b_test::Log("FORK swapBuffers refused: %s",
                      consumer.lastError().c_str());
        return 1;
    }

    const rhi::SceneExecuteResult& result = consumer.lastResult();
    q1b_test::Log(
        "FORK draws extracted=%u submitted=%u refused=%u lights=%u excluded=%u "
        "refused=%u passes=%u submissions=%u unavailable=%d",
        result.drawsExtracted, result.drawsSubmitted, result.drawsRefused,
        result.lightsExtracted, result.lightsExcluded, result.lightsRefused,
        result.lightPassesSubmitted,
        static_cast<unsigned>(result.submissions.size()),
        result.drawSubmissionUnavailable ? 1 : 0);
    if (result.drawsExtracted != 1u || result.drawsSubmitted != 1u ||
        result.drawsRefused != 0u || result.lightsExtracted != 2u ||
        result.lightsExcluded != 1u || result.lightsRefused != 0u ||
        result.lightPassesSubmitted != 3u || result.submissions.size() != 2u ||
        result.drawSubmissionUnavailable) {
        q1b_test::Log("FORK counts do not match the expected frame plan");
        return 1;
    }

    rhi::ReadbackResult surfaceImage;
    rhi::ReadbackResult lightImage;
    if (!q1b_test::WaitComplete(device, result.submissions[0]) ||
        !q1b_test::WaitComplete(device, result.submissions[1]) ||
        device.ReadbackAttachment(result.submissions[0], 0, &surfaceImage,
                                  &error) != Status::Ok ||
        device.ReadbackAttachment(result.submissions[1], 0, &lightImage,
                                  &error) != Status::Ok) {
        q1b_test::Log("FORK readback failed: %s", error.c_str());
        return 1;
    }
    q2_gpu_test::PrintPixel("FORK surface", 1, 1, surfaceImage);
    q2_gpu_test::PrintPixel("FORK light", 4, 1, lightImage);
    q2_gpu_test::PrintPixel("FORK light", 1, 1, lightImage);
    // The surface quad is drawn with the default white per-item tint; the light
    // frame keeps the shadowed column black and shades the rest with the
    // fixture's red tint.
    if (!q2_gpu_test::PixelIs(surfaceImage, 1, 1, 255, 255, 255, 255) ||
        !q2_gpu_test::PixelIs(lightImage, 4, 1, 0, 0, 0, 255) ||
        !q2_gpu_test::PixelIs(lightImage, 1, 1, 255, 0, 0, 255)) {
        q1b_test::Log("FORK frame readback does not match the expected content");
        return 1;
    }
    q1a_test::WriteBmp(q1a_test::EvidencePath("q2-fork-seam-surface.bmp"),
                       surfaceImage);
    q1a_test::WriteBmp(q1a_test::EvidencePath("q2-fork-seam-light.bmp"),
                       lightImage);
    q1b_test::Log(
        "FORK PASS: ordered surface, texture, shadow-chain and private-pose "
        "values reached the device frame");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    return q1b_test::ForEachAdapter(
        [](rhi::Device& device) { return RunForkSeam(device); });
}
