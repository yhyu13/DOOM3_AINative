#pragma once

// Engine-side device consumer for the Q2 scene sink (spec 004 T080/T081/T082).
//
// The portable EngineQ2SinkConsumer turns ordered surface/draw/light packets
// into an ordered SceneCommandQueue, but it deliberately names no Device. This
// header is the concrete half that the engine target links:
//
//   * EngineQ2DeviceBackend implements SceneGeometryBackend with Device::CreateBuffer
//     and Device::SubmitDrawFrame, so an ordered draw packet becomes a real
//     indexed/non-indexed device draw;
//   * EngineQ2DeviceTextureCache implements Q2TextureUploader, so an engine image
//     upload becomes a device texture that the draw's material stage samples;
//   * EngineQ2DeviceConsumer owns the three plus the portable sink, so a caller
//     that has a Device and the four classic pipelines can initialize the whole
//     chain with one descriptor.
//
// Lifetime: the Device must outlive the consumer and its texture cache. The
// cache never destroys textures from its destructor, because the destructor may
// run after the Device has been shut down; call Clear() (or Shutdown()) while
// the Device is still valid.
//
// This is not a rendered engine frame by itself. The engine fork still has to
// create the Device and the pipelines in R_InitRenderApi and call this class;
// until that call site exists, r_renderApi 1 remains the typed refusal.

#include "rhi/engine_q2_sink.h"
#include "rhi/engine_scene_adapter.h"
#include "rhi/rhi.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace doom3 {
namespace rhi {

// The facts the cache retains for one uploaded image. The sampler fields are
// recorded rather than applied: the portable RHI exposes one static point
// sampler, so honouring a per-image filter/repeat is future scope. Keeping the
// facts makes that limit visible instead of silently changing the visible
// sampling.
struct EngineQ2TextureInfo {
    Texture texture;
    int width = 0;
    int height = 0;
    int mipCount = 0;       // mips forwarded by the engine seam
    int uploadedMips = 0;   // mips actually uploaded (the seam sends mip 0)
    int format = 0;         // q2ImageUpload_t format value
    bool sRGB = false;
    int minFilter = 0;
    int magFilter = 0;
    int wrapS = 0;
    int wrapT = 0;
};

// Device texture cache keyed by the engine's stable image identity. A repeated
// upload replaces the texture and retires the old handle; the handle is
// destroyed once no recorded submission can still reference it.
class EngineQ2DeviceTextureCache : public Q2TextureUploader {
public:
    explicit EngineQ2DeviceTextureCache(Device& device);
    ~EngineQ2DeviceTextureCache() override = default;

    EngineQ2DeviceTextureCache(const EngineQ2DeviceTextureCache&) = delete;
    EngineQ2DeviceTextureCache& operator=(const EngineQ2DeviceTextureCache&) = delete;

    // Uploads one image. Format 0 is RGBA8 (sRGB-aware), 1 is R8, 4 is BC5 and
    // 5 is BC7 (sRGB-aware). BC1/BC3 have no portable format and are refused
    // with a reason rather than decoded on the CPU. R8 sRGB is refused because
    // the portable format set has no R8 sRGB variant; sampling it as UNORM
    // would silently change the colour space.
    bool UploadImage(uint32_t imageId, const Q2SinkImageUpload& image,
                     Texture* texture, std::string* error) override;

    bool Find(uint32_t imageId, Texture* texture,
              EngineQ2TextureInfo* info = nullptr) const;

    const std::map<uint32_t, EngineQ2TextureInfo>& textures() const {
        return textures_;
    }
    size_t size() const { return textures_.size(); }

    // Destroys every cached and retired texture that is safe to destroy now.
    // A texture still referenced by an outstanding submission is kept and
    // retried later. The Device must still be valid.
    Status Clear(std::string* error);

private:
    Status CollectRetired(std::string* error);
    Status Retire(const Texture& texture, std::string* error);

    Device& device_;
    std::map<uint32_t, EngineQ2TextureInfo> textures_;
    std::vector<Texture> retired_;
};

// Concrete SceneGeometryBackend: uploads the translated mesh and records the
// adapter's ordered frame on the device.
class EngineQ2DeviceBackend : public SceneGeometryBackend {
public:
    explicit EngineQ2DeviceBackend(Device& device) : device_(device) {}

    bool Upload(uint32_t key, const SceneSurfaceGeometry& geometry,
                SceneMeshHandles* handles, std::string* error) override;
    Status Submit(const DrawFrameDesc& frame, Submission* submission,
                  std::string* error) override;

    Device& device() { return device_; }
    const Device& device() const { return device_; }

private:
    Device& device_;
};

// The four classic pipelines plus the attachment indices the adapter submits
// to. The caller creates them through Device::CreateGraphicsPipeline; this
// consumer does not compile shaders or guess a program.
struct EngineQ2DevicePipelines {
    Pipeline surface;
    Pipeline shadowBack;
    Pipeline shadowFront;
    Pipeline shade;
    uint32_t surfaceAttachmentIndex = 0;
    uint32_t lightAttachmentIndex = 0;
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
};

// Owns the portable sink, the device backend and the texture cache. It is the
// single object an engine call site needs after it has a Device and pipelines.
class EngineQ2DeviceConsumer {
public:
    explicit EngineQ2DeviceConsumer(Device& device);
    ~EngineQ2DeviceConsumer();

    EngineQ2DeviceConsumer(const EngineQ2DeviceConsumer&) = delete;
    EngineQ2DeviceConsumer& operator=(const EngineQ2DeviceConsumer&) = delete;

    bool Initialize(int width, int height, const EngineQ2DevicePipelines& pipelines,
                    std::string* error);
    void Shutdown();
    bool initialized() const { return initialized_; }

    EngineQ2SinkConsumer& sink() { return sink_; }
    const EngineQ2SinkConsumer& sink() const { return sink_; }
    EngineQ2DeviceBackend& backend() { return backend_; }
    const EngineQ2DeviceBackend& backend() const { return backend_; }
    EngineQ2DeviceTextureCache& textures() { return textures_; }
    const EngineQ2DeviceTextureCache& textures() const { return textures_; }

private:
    Device& device_;
    EngineQ2DeviceBackend backend_;
    EngineQ2DeviceTextureCache textures_;
    EngineQ2SinkConsumer sink_;
    bool initialized_ = false;
};

}  // namespace rhi
}  // namespace doom3
