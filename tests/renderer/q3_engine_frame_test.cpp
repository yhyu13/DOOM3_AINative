// T085 engine-link fixture: the Q2 scene sink -> Q3 frame resources.
//
// This test drives the same portable callback table the engine registration
// unit installs (EngineQ2SinkConsumer). It is not a simulated command list:
//
//   frame:  beginView/draw/endView/swapBuffers go through the real Q2 adapter,
//           the real Q3 FrameIntegration and a device submission; the readback
//           is an ordered two-surface frame.
//   motion: the current/previous pose is reprojected, per-pixel motion and
//           validity are stored, uploaded as two device textures and read back
//           from those textures through a fullscreen pass.
//
// A host without a device returns 77 (SKIP). No PASS is claimed without a
// readback.

#include "q1b_test_support.h"
#include "q2_gpu_test_support.h"
#include "rhi/engine_q2_sink.h"
#include "rhi/frame_motion_resources.h"
#include "rhi/motion_history.h"

#include <cmath>
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
using rhi::Pipeline;
using rhi::Q2SinkCallbacks;
using rhi::Q2SinkMaterialFacts;
using rhi::SceneGeometryBackend;
using rhi::SceneGeometrySource;
using rhi::SceneLightGeometry;
using rhi::SceneMeshHandles;
using rhi::SceneSurfaceGeometry;
using rhi::ShaderModule;
using rhi::Status;
using rhi::Texture;
using rhi::Vertex;

constexpr uint32_t kWidth = 8;
constexpr uint32_t kHeight = 8;

// Raw texture readback through the vertex-input item path: the backend binds
// the item texture, and Load returns the stored texel. Writing it to an RGBA8
// target gives the same bytes back. No filter, no derived expectation.
const char* kRawTexturePixel = R"(
Texture2D sourceTexture : register(t0);

struct VSOut {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 PSMain(VSOut input) : SV_Target {
    uint2 pixel = uint2(input.position.xy);
    return sourceTexture.Load(int3(pixel, 0));
}
)";

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

class TwoSurfaceScene : public SceneGeometrySource {
public:
    TwoSurfaceScene(Texture white, std::vector<Vertex> red,
                    std::vector<Vertex> green)
        : white_(white), red_(std::move(red)), green_(std::move(green)) {}

    bool ResolveSurface(uint32_t surfaceId,
                        SceneSurfaceGeometry* geometry) override {
        if (surfaceId == 1) {
            geometry->vertices = red_;
            geometry->tint[0] = 1.0f;
            geometry->tint[1] = 0.0f;
        } else if (surfaceId == 2) {
            geometry->vertices = green_;
            geometry->tint[0] = 0.0f;
            geometry->tint[1] = 1.0f;
        } else {
            return false;
        }
        geometry->tint[2] = 0.0f;
        geometry->tint[3] = 1.0f;
        geometry->indices = q2_gpu_test::QuadIndicesBothWindings();
        geometry->texture = white_;
        return true;
    }

    bool ResolveLight(const rhi::LightPacket&, SceneLightGeometry*) override {
        return false;
    }

private:
    Texture white_;
    std::vector<Vertex> red_;
    std::vector<Vertex> green_;
};

Q2SinkMaterialFacts SurfaceFacts() {
    Q2SinkMaterialFacts facts;
    facts.numStages = 1;
    facts.hasTexture = true;
    facts.hasProgram = true;
    return facts;
}

float IdentityProjection[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 1.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 1.0f, 0.0f,
                                0.0f, 0.0f, 0.0f, 1.0f};

int RunFrame(rhi::Device& device) {
    std::string error;
    const ShaderModule vertex = q2_gpu_test::MakeShader(
        device, rhi::ShaderStage::Vertex, q2_gpu_test::kItemVertex, "VSMain",
        &error);
    const ShaderModule pixel = q2_gpu_test::MakeShader(
        device, rhi::ShaderStage::Pixel, q2_gpu_test::kTexturedPixel, "PSMain",
        &error);
    Pipeline surface;
    if (vertex.id == 0 || pixel.id == 0 ||
        !q2_gpu_test::CreateSurfacePipeline(device, vertex, pixel, true,
                                            &surface, &error)) {
        q1b_test::Log("SINK pipeline creation failed: %s", error.c_str());
        return 1;
    }
    const Texture white = q2_gpu_test::MakeRgba8Texture(
        device, 1, 1, {255, 255, 255, 255}, rhi::TextureFormat::RGBA8Unorm,
        &error);
    if (white.id == 0) {
        q1b_test::Log("SINK white texture failed: %s", error.c_str());
        return 1;
    }

    TwoSurfaceScene geometry(
        white,
        q2_gpu_test::QuadVertices(-1.0f, 1.0f, 1.0f, -1.0f, 0.5f),
        q2_gpu_test::QuadVertices(-1.0f, 0.0f, 1.0f, -1.0f, 0.5f));
    DeviceBackend backend(device);

    EngineQ2SinkDesc desc;
    desc.structuralOnly = false;
    desc.backend = &backend;
    desc.geometry = &geometry;
    desc.surfacePipeline = surface;
    desc.surfaceAttachmentIndex = 0;
    EngineQ2SinkConsumer consumer(desc);
    if (!consumer.Initialize(kWidth, kHeight, &error)) {
        q1b_test::Log("SINK initialize failed: %s", error.c_str());
        return 1;
    }

    const Q2SinkCallbacks* sink = consumer.callbacks();
    if (!sink->beginView(sink->user, 1u, 0, 0, kWidth, kHeight, false, false,
                         IdentityProjection)) {
        q1b_test::Log("SINK beginView refused");
        return 1;
    }
    const Q2SinkMaterialFacts facts = SurfaceFacts();
    if (!sink->drawPacket(sink->user, 1u, &facts, 6, 12, 0, 0.0f,
                          false, false) ||
        !sink->drawPacket(sink->user, 2u, &facts, 4, 12, 0, 0.0f,
                          false, false)) {
        q1b_test::Log("SINK drawPacket refused");
        return 1;
    }
    if (!sink->endView(sink->user)) {
        q1b_test::Log("SINK endView refused");
        return 1;
    }
    if (!sink->swapBuffers(sink->user)) {
        q1b_test::Log("SINK swapBuffers refused: %s",
                      consumer.lastError().c_str());
        return 1;
    }

    const rhi::SceneExecuteResult& result = consumer.lastResult();
    q1b_test::Log(
        "SINK draws extracted=%u submitted=%u refused=%u submissions=%u "
        "unavailable=%d views=%u overlays=%u",
        result.drawsExtracted, result.drawsSubmitted, result.drawsRefused,
        static_cast<unsigned>(result.submissions.size()),
        result.drawSubmissionUnavailable ? 1 : 0, result.views,
        result.overlays);
    if (result.drawsExtracted != 2u || result.drawsSubmitted != 2u ||
        result.drawsRefused != 0u || result.submissions.size() != 1u ||
        result.drawSubmissionUnavailable) {
        q1b_test::Log("SINK counts do not match the expected frame");
        return 1;
    }

    rhi::ReadbackResult image;
    if (!q1b_test::WaitComplete(device, result.submissions[0]) ||
        device.ReadbackAttachment(result.submissions[0], 0, &image, &error) !=
            Status::Ok) {
        q1b_test::Log("SINK readback failed: %s", error.c_str());
        return 1;
    }
    q2_gpu_test::PrintPixel("SINK frame", 1, 1, image);
    q2_gpu_test::PrintPixel("SINK frame", 6, 6, image);
    if (!q2_gpu_test::PixelIs(image, 1, 1, 0, 255, 0, 255) ||
        !q2_gpu_test::PixelIs(image, 6, 6, 255, 0, 0, 255)) {
        q1b_test::Log("SINK surface order failed");
        return 1;
    }
    q1a_test::WriteBmp(q1a_test::EvidencePath("q3-engine-sink-frame.bmp"),
                       image);
    q1b_test::Log("SINK PASS: one ordered two-surface frame from the Q2 sink");
    return 0;
}

int RunMotionResources(rhi::Device& device) {
    std::string error;
    EngineQ2SinkDesc structural;
    structural.structuralOnly = true;
    EngineQ2SinkConsumer consumer(structural);
    if (!consumer.Initialize(kWidth, kHeight, &error)) {
        q1b_test::Log("MOTION sink initialize failed: %s", error.c_str());
        return 1;
    }

    rhi::ViewIdentity identity;
    identity.worldId = 1;
    identity.instanceId = 1;
    identity.generation = 1;
    identity.topology = 1;
    identity.entity = 1;
    identity.provider = 1;
    identity.width = kWidth;
    identity.height = kHeight;

    rhi::CameraState camera;
    camera.width = kWidth;
    camera.height = kHeight;
    rhi::RenderedViewCommit previous;
    previous.kind = rhi::ViewKind::Main;
    previous.identity = identity;
    previous.camera = camera;
    rhi::RenderRecord previousRecord;
    previousRecord.point = rhi::MotionVec3{100.0, 0.0, 1.0};
    previous.records.push_back(previousRecord);
    previous.poseIdentity = 41u;
    previous.poseGeneration = 3u;
    previous.posedPositions.push_back(previousRecord.point);
    if (!consumer.CommitRenderedView(previous)) {
        q1b_test::Log("MOTION previous view commit failed");
        return 1;
    }

    rhi::RenderedViewState stored;
    if (rhi::FrameIntegration* frame = &consumer.frame();
        frame->Previous(rhi::ViewKind::Main, identity, &stored) !=
        rhi::MotionValidity::Valid) {
        q1b_test::Log("MOTION previous view lookup failed");
        return 1;
    }
    rhi::RenderRecord current;
    current.point = rhi::MotionVec3{100.0, 0.0, 0.5};
    const rhi::MotionSample sample =
        rhi::ReprojectPoint(stored.camera, stored.records[0], camera, current);
    if (sample.validity != rhi::MotionValidity::Valid ||
        !consumer.StorePerPixelMotion(1u, 1u, sample)) {
        q1b_test::Log("MOTION reprojection or store failed");
        return 1;
    }
    // One invalid pixel: validity is stored separately, not as a zero vector.
    const rhi::MotionSample invalid;
    if (!consumer.StorePerPixelMotion(2u, 1u, invalid)) {
        q1b_test::Log("MOTION invalid store failed");
        return 1;
    }

    rhi::MotionFrameResource resource;
    if (consumer.UploadPerPixelMotion(device, &resource, &error) != Status::Ok ||
        !resource.valid()) {
        q1b_test::Log("MOTION resource upload failed: %s", error.c_str());
        return 1;
    }
    if (resource.width != kWidth || resource.height != kHeight ||
        resource.generation != consumer.frame().motion().generation()) {
        q1b_test::Log("MOTION resource extent/generation changed");
        return 1;
    }
    if (resource.packedMotion.size() != kWidth * kHeight * 4u ||
        resource.packedValidity.size() != kWidth * kHeight) {
        q1b_test::Log("MOTION resource mirror size changed");
        return 1;
    }

    const uint16_t duWord = rhi::MotionHalfWord(sample.vector.du);
    const uint16_t dvWord = rhi::MotionHalfWord(sample.vector.dv);
    const uint8_t* storedMotion = resource.packedMotion.data() + (1u * kWidth + 1u) * 4u;
    const uint16_t storedDu =
        static_cast<uint16_t>(storedMotion[0] | (storedMotion[1] << 8));
    const uint16_t storedDv =
        static_cast<uint16_t>(storedMotion[2] | (storedMotion[3] << 8));
    if (storedDu != duWord || storedDv != dvWord ||
        resource.packedValidity[1u * kWidth + 1u] != 0xFFu ||
        resource.packedValidity[1u * kWidth + 2u] != 0u) {
        q1b_test::Log("MOTION packed encoding changed");
        return 1;
    }

    // Read the two device textures back through a vertex-input fullscreen
    // quad. The bytes compared below come from the resource mirror, and the
    // readback proves the device holds those bytes.
    const ShaderModule vertex = q2_gpu_test::MakeShader(
        device, rhi::ShaderStage::Vertex, q2_gpu_test::kItemVertex, "VSMain",
        &error);
    const ShaderModule raw = q2_gpu_test::MakeShader(
        device, rhi::ShaderStage::Pixel, kRawTexturePixel, "PSMain", &error);
    Pipeline pipeline;
    if (vertex.id == 0 || raw.id == 0 ||
        !q2_gpu_test::CreateSurfacePipeline(device, vertex, raw, true, &pipeline,
                                            &error)) {
        q1b_test::Log("MOTION pipeline creation failed: %s", error.c_str());
        return 1;
    }

    const std::vector<Vertex> quad =
        q2_gpu_test::QuadVertices(-1.0f, 1.0f, 1.0f, -1.0f, 0.5f);
    const std::vector<uint16_t> quadIndices =
        q2_gpu_test::QuadIndicesBothWindings();
    rhi::Buffer quadVertices;
    rhi::Buffer quadIndexBuffer;
    if (!q2_gpu_test::CreateVertexBuffer(device, quad, &quadVertices, &error) ||
        !q2_gpu_test::CreateIndexBuffer(device, quadIndices, &quadIndexBuffer,
                                        &error)) {
        q1b_test::Log("MOTION quad upload failed: %s", error.c_str());
        return 1;
    }

    const uint32_t motionIndex = 0;
    const uint32_t validityIndex = 1;
    DrawPassDesc passes[2];
    passes[0].pipeline = pipeline;
    passes[0].colorIndices = &motionIndex;
    passes[0].colorCount = 1;
    passes[1].pipeline = pipeline;
    passes[1].colorIndices = &validityIndex;
    passes[1].colorCount = 1;

    rhi::ClearRow clears[2];
    clears[0].alpha = 255;
    clears[1].alpha = 255;
    passes[0].colorClears = &clears[0];
    passes[1].colorClears = &clears[1];
    passes[0].tint[0] = 1.0f;
    passes[0].tint[1] = 1.0f;
    passes[0].tint[2] = 1.0f;
    passes[0].tint[3] = 1.0f;
    passes[1].tint[0] = 1.0f;
    passes[1].tint[1] = 1.0f;
    passes[1].tint[2] = 1.0f;
    passes[1].tint[3] = 1.0f;

    DrawItem motionItem;
    motionItem.vertexBuffer = quadVertices;
    motionItem.vertexCount = static_cast<uint32_t>(quad.size());
    motionItem.indexBuffer = quadIndexBuffer;
    motionItem.indexCount = static_cast<uint32_t>(quadIndices.size());
    motionItem.texture = resource.motion;
    DrawItem validityItem = motionItem;
    validityItem.texture = resource.validity;
    passes[0].items = &motionItem;
    passes[0].itemCount = 1;
    passes[1].items = &validityItem;
    passes[1].itemCount = 1;

    DrawFrameDesc frameDesc;
    frameDesc.width = kWidth;
    frameDesc.height = kHeight;
    frameDesc.attachmentCount = 2;
    frameDesc.passCount = 2;
    frameDesc.passes = passes;
    rhi::Submission submission;
    if (device.SubmitDrawFrame(frameDesc, &submission, &error) != Status::Ok) {
        q1b_test::Log("MOTION frame submission failed: %s", error.c_str());
        return 1;
    }
    rhi::ReadbackResult motionImage;
    rhi::ReadbackResult validityImage;
    if (!q1b_test::WaitComplete(device, submission) ||
        device.ReadbackAttachment(submission, 0, &motionImage, &error) !=
            Status::Ok ||
        device.ReadbackAttachment(submission, 1, &validityImage, &error) !=
            Status::Ok) {
        q1b_test::Log("MOTION readback failed: %s", error.c_str());
        return 1;
    }

    bool motionMatches = true;
    for (uint32_t y = 0; y < kHeight && motionMatches; ++y) {
        const uint8_t* row =
            motionImage.pixels.data() + static_cast<size_t>(y) * motionImage.rowStride;
        const size_t offset = static_cast<size_t>(y) * kWidth * 4u;
        motionMatches =
            std::memcmp(row, resource.packedMotion.data() + offset, kWidth * 4u) == 0;
    }
    bool validityMatches = true;
    for (uint32_t y = 0; y < kHeight && validityMatches; ++y) {
        const uint8_t* row =
            validityImage.pixels.data() + static_cast<size_t>(y) * validityImage.rowStride;
        for (uint32_t x = 0; x < kWidth; ++x) {
            const uint8_t expected = resource.packedValidity[y * kWidth + x];
            const uint8_t actual = row[x * 4u];
            if (actual != expected) {
                validityMatches = false;
                break;
            }
        }
    }
    q2_gpu_test::PrintPixel("MOTION resource", 1, 1, motionImage);
    q2_gpu_test::PrintPixel("MOTION validity", 1, 1, validityImage);
    q2_gpu_test::PrintPixel("MOTION validity", 2, 1, validityImage);
    if (!motionMatches || !validityMatches) {
        q1b_test::Log("MOTION readback does not match the stored samples");
        return 1;
    }
    q1a_test::WriteBmp(q1a_test::EvidencePath("q3-engine-motion-resources.bmp"),
                       motionImage);
    q1b_test::Log(
        "MOTION PASS: previous/current pose -> per-pixel motion and validity "
        "uploaded and read back from device textures");
    return 0;
}

int Dispatch(const std::string& mode, rhi::Device& device) {
    if (mode == "frame") {
        return RunFrame(device);
    }
    if (mode == "motion") {
        return RunMotionResources(device);
    }
    q1b_test::Log("unknown mode '%s'", mode.c_str());
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "frame";
    return q1b_test::ForEachAdapter(
        [&mode](rhi::Device& device) { return Dispatch(mode, device); });
}
