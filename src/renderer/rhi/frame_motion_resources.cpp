#include "rhi/frame_motion_resources.h"

#include "rhi/frame_integration.h"
#include "rhi/motion_history.h"

#include <cstring>

namespace doom3 {
namespace rhi {

namespace {

void StoreHalfLittleEndian(uint16_t word, uint8_t* bytes) {
    bytes[0] = static_cast<uint8_t>(word & 0xFFu);
    bytes[1] = static_cast<uint8_t>((word >> 8) & 0xFFu);
}

}  // namespace

Status CreateMotionFrameResources(Device& device, const HistoryBuffer& history,
                                  MotionFrameResource* resource,
                                  std::string* error) {    if (resource == nullptr) {
        if (error != nullptr) {
            *error = "motion frame resources need an output resource";
        }
        return Status::InvalidArgument;
    }
    *resource = MotionFrameResource();
    if (history.empty() || history.width() == 0 || history.height() == 0) {
        if (error != nullptr) {
            *error = "motion frame resources need a resized history buffer";
        }
        return Status::InvalidArgument;
    }

    const uint32_t width = history.width();
    const uint32_t height = history.height();
    const size_t texels = static_cast<size_t>(width) * height;
    std::vector<uint8_t> motionBytes(texels * 4u, 0u);
    std::vector<uint8_t> validityBytes(texels, 0u);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const PerPixelMotionSample sample = history.At(x, y);
            const size_t index = static_cast<size_t>(y) * width + x;
            if (!sample.valid) {
                continue;
            }
            uint8_t* texel = motionBytes.data() + index * 4u;
            StoreHalfLittleEndian(MotionHalfWord(sample.x), texel);
            StoreHalfLittleEndian(MotionHalfWord(sample.y), texel + 2);
            validityBytes[index] = 0xFFu;
        }
    }

    TextureDesc motionDesc;
    motionDesc.format = TextureFormat::RGBA8Unorm;
    TextureMip motionMip;
    motionMip.width = width;
    motionMip.height = height;
    motionMip.bytes = motionBytes;
    motionDesc.mips.push_back(std::move(motionMip));

    TextureDesc validityDesc;
    validityDesc.format = TextureFormat::R8Unorm;
    TextureMip validityMip;
    validityMip.width = width;
    validityMip.height = height;
    validityMip.bytes = validityBytes;
    validityDesc.mips.push_back(std::move(validityMip));

    Texture motion;
    if (device.CreateTexture(motionDesc, &motion, error) != Status::Ok) {
        if (error != nullptr) {
            *error = "motion texture: " + *error;
        }
        return Status::Error;
    }
    Texture validity;
    if (device.CreateTexture(validityDesc, &validity, error) != Status::Ok) {
        if (error != nullptr) {
            *error = "validity texture: " + *error;
        }
        device.DestroyTexture(motion, nullptr);
        return Status::Error;
    }

    resource->motion = motion;
    resource->validity = validity;
    resource->width = width;
    resource->height = height;
    resource->generation = history.generation();
    resource->packedMotion = std::move(motionBytes);
    resource->packedValidity = std::move(validityBytes);
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

Status FrameIntegration::UploadPerPixelMotion(Device& device,
                                              MotionFrameResource* resource,
                                              std::string* error) const {
    if (!initialized_) {
        if (error != nullptr) {
            *error = "frame integration is not initialized";
        }
        return Status::NotReady;
    }
    return CreateMotionFrameResources(device, motion_, resource, error);
}

}  // namespace rhi
}  // namespace doom3
