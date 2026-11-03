#include "rhi/engine_q2_sink_device.h"

#include <algorithm>
#include <cstring>

namespace doom3 {
namespace rhi {

namespace {

uint32_t DivRoundUp(uint32_t value, uint32_t divisor) {
    return (value + divisor - 1u) / divisor;
}

// q2ImageUpload_t::format values, mirrored from RenderApi.h. Keeping the
// numbers here rather than including an engine header preserves the portable
// boundary: the registration unit already converted the value.
enum Q2ImageFormat {
    kImageRgba8 = 0,
    kImageR8 = 1,
    kImageBc1 = 2,
    kImageBc3 = 3,
    kImageBc5 = 4,
    kImageBc7 = 5,
};

bool MapFormat(const Q2SinkImageUpload& image, TextureFormat* format,
               std::string* error) {
    switch (image.format) {
        case kImageRgba8:
            *format = image.sRGB ? TextureFormat::RGBA8Srgb
                                 : TextureFormat::RGBA8Unorm;
            return true;
        case kImageR8:
            if (image.sRGB) {
                if (error != nullptr) {
                    *error =
                        "R8 sRGB has no portable texture format; refusing instead of "
                        "sampling it as UNORM and changing the colour space";
                }
                return false;
            }
            *format = TextureFormat::R8Unorm;
            return true;
        case kImageBc5:
            *format = TextureFormat::BC5Unorm;
            return true;
        case kImageBc7:
            *format = image.sRGB ? TextureFormat::BC7UnormSrgb
                                 : TextureFormat::BC7Unorm;
            return true;
        case kImageBc1:
        case kImageBc3:
            if (error != nullptr) {
                *error =
                    "BC1/BC3 have no portable texture format; refusing instead of "
                    "decoding them on the CPU";
            }
            return false;
        default:
            if (error != nullptr) {
                *error = "unknown engine image format " +
                         std::to_string(image.format);
            }
            return false;
    }
}

// The engine seam forwards mip 0 only. A device texture with one mip is a
// complete chain for the RHI; the declared mip count is retained so a reader
// can see that the seam did not send the smaller levels.
bool ValidateUpload(const Q2SinkImageUpload& image, uint32_t* rowBytes,
                    std::string* error) {
    if (image.width <= 0 || image.height <= 0) {
        if (error != nullptr) {
            *error = "an image upload has an empty extent";
        }
        return false;
    }
    const uint32_t width = static_cast<uint32_t>(image.width);
    const uint32_t height = static_cast<uint32_t>(image.height);

    if (image.format == kImageRgba8 || image.format == kImageR8) {
        const uint32_t texelBytes = image.format == kImageRgba8 ? 4u : 1u;
        const uint32_t expectedRow = width * texelBytes;
        if (image.bytesPerRow != static_cast<int>(expectedRow)) {
            if (error != nullptr) {
                *error = "an image upload has an inconsistent row pitch";
            }
            return false;
        }
        if (image.pixels.size() !=
            static_cast<size_t>(expectedRow) * static_cast<size_t>(height)) {
            if (error != nullptr) {
                *error = "an image upload has an inconsistent byte count";
            }
            return false;
        }
        *rowBytes = expectedRow;
        return true;
    }

    if (image.format == kImageBc5 || image.format == kImageBc7) {
        // One 4x4 block is 16 bytes for both formats.
        const uint32_t blocks =
            DivRoundUp(width, 4u) * DivRoundUp(height, 4u);
        const size_t expected = static_cast<size_t>(blocks) * 16u;
        if (image.pixels.size() != expected) {
            if (error != nullptr) {
                *error = "a block-compressed upload has an inconsistent byte count";
            }
            return false;
        }
        *rowBytes = static_cast<uint32_t>(image.bytesPerRow > 0
                                              ? image.bytesPerRow
                                              : 0);
        return true;
    }

    if (error != nullptr) {
        *error = "an uploaded image format was not validated";
    }
    return false;
}

}  // namespace

EngineQ2DeviceTextureCache::EngineQ2DeviceTextureCache(Device& device)
    : device_(device) {}

bool EngineQ2DeviceTextureCache::UploadImage(
    uint32_t imageId, const Q2SinkImageUpload& image, Texture* texture,
    std::string* error) {
    if (texture == nullptr) {
        if (error != nullptr) {
            *error = "UploadImage needs an output texture";
        }
        return false;
    }
    *texture = Texture();
    if (imageId == 0) {
        if (error != nullptr) {
            *error = "an image upload has no identity";
        }
        return false;
    }

    TextureFormat format = TextureFormat::RGBA8Unorm;
    if (!MapFormat(image, &format, error)) {
        return false;
    }
    uint32_t rowBytes = 0;
    if (!ValidateUpload(image, &rowBytes, error)) {
        return false;
    }
    (void)rowBytes;  // the RHI takes tightly packed bytes; the row pitch is validated

    TextureDesc desc;
    desc.format = format;
    TextureMip mip;
    mip.width = static_cast<uint32_t>(image.width);
    mip.height = static_cast<uint32_t>(image.height);
    mip.bytes = image.pixels;
    desc.mips.push_back(std::move(mip));

    Texture created;
    if (device_.CreateTexture(desc, &created, error) != Status::Ok) {
        return false;
    }

    // A repeated upload replaces the old texture. Create the new one first so
    // a failed upload leaves the previous texture usable.
    const auto existing = textures_.find(imageId);
    if (existing != textures_.end()) {
        std::string retireError;
        if (Retire(existing->second.texture, &retireError) != Status::Ok) {
            // A still-referenced handle is kept in the retired list; the new
            // upload is still valid, so this is not a failure.
        }
    }

    EngineQ2TextureInfo info;
    info.texture = created;
    info.width = image.width;
    info.height = image.height;
    info.mipCount = std::max(image.mipCount, 1);
    info.uploadedMips = 1;
    info.format = image.format;
    info.sRGB = image.sRGB;
    info.minFilter = image.minFilter;
    info.magFilter = image.magFilter;
    info.wrapS = image.wrapS;
    info.wrapT = image.wrapT;
    textures_[imageId] = info;
    *texture = created;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool EngineQ2DeviceTextureCache::Find(uint32_t imageId, Texture* texture,
                                     EngineQ2TextureInfo* info) const {
    const auto found = textures_.find(imageId);
    if (found == textures_.end()) {
        return false;
    }
    if (texture != nullptr) {
        *texture = found->second.texture;
    }
    if (info != nullptr) {
        *info = found->second;
    }
    return true;
}

Status EngineQ2DeviceTextureCache::Retire(const Texture& texture,
                                         std::string* error) {
    if (texture.id == 0) {
        return Status::Ok;
    }
    const Status status = device_.DestroyTexture(texture, error);
    if (status == Status::NotReady) {
        // A recorded submission can still reference it; try again later.
        retired_.push_back(texture);
        return Status::Ok;
    }
    return status;
}

Status EngineQ2DeviceTextureCache::CollectRetired(std::string* error) {
    std::vector<Texture> stillRetired;
    Status result = Status::Ok;
    for (const Texture& texture : retired_) {
        const Status status = device_.DestroyTexture(texture, error);
        if (status == Status::NotReady) {
            stillRetired.push_back(texture);
        } else if (status != Status::Ok && result == Status::Ok) {
            result = status;
        }
    }
    retired_ = std::move(stillRetired);
    return result;
}

Status EngineQ2DeviceTextureCache::Clear(std::string* error) {
    Status result = CollectRetired(error);
    for (const auto& entry : textures_) {
        const Status status = device_.DestroyTexture(entry.second.texture, error);
        if (status == Status::NotReady) {
            retired_.push_back(entry.second.texture);
        } else if (status != Status::Ok && result == Status::Ok) {
            result = status;
        }
    }
    textures_.clear();
    if (result == Status::Ok) {
        result = CollectRetired(error);
    }
    return result;
}

bool EngineQ2DeviceBackend::Upload(uint32_t key,
                                  const SceneSurfaceGeometry& geometry,
                                  SceneMeshHandles* handles,
                                  std::string* error) {
    (void)key;
    if (handles == nullptr) {
        if (error != nullptr) {
            *error = "mesh upload needs an output handle";
        }
        return false;
    }
    *handles = SceneMeshHandles();
    if (geometry.vertices.empty()) {
        if (error != nullptr) {
            *error = "surface geometry has no vertices";
        }
        return false;
    }

    BufferDesc vertexDesc;
    vertexDesc.usage = BufferUsage::Vertex;
    vertexDesc.bytes.resize(geometry.vertices.size() * sizeof(Vertex));
    std::memcpy(vertexDesc.bytes.data(), geometry.vertices.data(),
                vertexDesc.bytes.size());
    Buffer vertexBuffer;
    if (device_.CreateBuffer(vertexDesc, &vertexBuffer, error) != Status::Ok) {
        return false;
    }
    handles->vertexBuffer = vertexBuffer;
    handles->vertexCount = static_cast<uint32_t>(geometry.vertices.size());

    if (geometry.indices.empty()) {
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }

    BufferDesc indexDesc;
    indexDesc.usage = BufferUsage::Index;
    indexDesc.bytes.resize(geometry.indices.size() * sizeof(uint16_t));
    std::memcpy(indexDesc.bytes.data(), geometry.indices.data(),
                indexDesc.bytes.size());
    Buffer indexBuffer;
    if (device_.CreateBuffer(indexDesc, &indexBuffer, error) != Status::Ok) {
        std::string ignored;
        device_.DestroyBuffer(vertexBuffer, &ignored);
        handles->vertexBuffer = Buffer();
        handles->vertexCount = 0;
        return false;
    }
    handles->indexBuffer = indexBuffer;
    handles->indexCount = static_cast<uint32_t>(geometry.indices.size());
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

Status EngineQ2DeviceBackend::Submit(const DrawFrameDesc& frame,
                                    Submission* submission,
                                    std::string* error) {
    return device_.SubmitDrawFrame(frame, submission, error);
}

EngineQ2DeviceConsumer::EngineQ2DeviceConsumer(Device& device)
    : device_(device), backend_(device), textures_(device) {}

EngineQ2DeviceConsumer::~EngineQ2DeviceConsumer() {
    if (initialized_) {
        Shutdown();
    }
}

bool EngineQ2DeviceConsumer::Initialize(int width, int height,
                                       const EngineQ2DevicePipelines& pipelines,
                                       std::string* error) {
    if (initialized_) {
        sink_.Shutdown();
    }
    initialized_ = false;
    // A re-initialization must not inherit textures from the previous session.
    std::string clearError;
    textures_.Clear(&clearError);
    if (width <= 0 || height <= 0) {
        if (error != nullptr) {
            *error = "the D3D12 Q2 consumer needs a non-zero display extent";
        }
        return false;
    }
    if (pipelines.surface.id == 0) {
        if (error != nullptr) {
            *error =
                "the D3D12 Q2 consumer has no surface pipeline; refusing instead "
                "of drawing an empty frame";
        }
        return false;
    }

    EngineQ2SinkDesc desc;
    desc.structuralOnly = false;
    desc.backend = &backend_;
    desc.textureBackend = &textures_;
    desc.geometry = nullptr;  // the sink resolves the surfaces it received
    desc.captureGeometry = true;
    desc.captureImageUploads = false;  // the device textures are the cache
    desc.surfacePipeline = pipelines.surface;
    desc.shadowBackPipeline = pipelines.shadowBack;
    desc.shadowFrontPipeline = pipelines.shadowFront;
    desc.lightShadePipeline = pipelines.shade;
    desc.surfaceAttachmentIndex = pipelines.surfaceAttachmentIndex;
    desc.lightAttachmentIndex = pipelines.lightAttachmentIndex;
    desc.renderWidth = pipelines.renderWidth;
    desc.renderHeight = pipelines.renderHeight;
    sink_.SetDesc(desc);
    if (!sink_.Initialize(width, height, error)) {
        return false;
    }
    initialized_ = true;
    return true;
}

void EngineQ2DeviceConsumer::Shutdown() {
    sink_.Shutdown();
    std::string ignored;
    textures_.Clear(&ignored);
    initialized_ = false;
}

}  // namespace rhi
}  // namespace doom3
