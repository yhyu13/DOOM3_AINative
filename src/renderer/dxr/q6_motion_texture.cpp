#include "dxr/q6_motion_texture.h"

#include "rhi/motion_history.h"

namespace doom3 {
namespace dxr {
namespace {

void StoreHalfLittleEndian(uint16_t word, uint8_t* bytes) {
    bytes[0] = static_cast<uint8_t>(word & 0xFFu);
    bytes[1] = static_cast<uint8_t>((word >> 8) & 0xFFu);
}

}  // namespace

bool PackMotionTexture(const rhi::HistoryBuffer& history, Q6MotionTextureData* out,
                       std::string* error) {
    if (out == nullptr) {
        if (error != nullptr) *error = "motion packing needs an output";
        return false;
    }
    *out = Q6MotionTextureData{};
    if (history.empty() || history.width() == 0 || history.height() == 0) {
        if (error != nullptr) *error = "motion packing needs a resized history buffer";
        return false;
    }
    const uint32_t width = history.width();
    const uint32_t height = history.height();
    const size_t texels = static_cast<size_t>(width) * height;
    out->width = width;
    out->height = height;
    out->generation = history.generation();
    out->motion.assign(texels * 4u, 0u);
    out->validity.assign(texels, 0u);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const size_t index = static_cast<size_t>(y) * width + x;
            const rhi::PerPixelMotionSample sample = history.At(x, y);
            if (!sample.valid) {
                ++out->invalidPixels;
                continue;
            }
            uint8_t* texel = out->motion.data() + index * 4u;
            StoreHalfLittleEndian(rhi::MotionHalfWord(sample.x), texel);
            StoreHalfLittleEndian(rhi::MotionHalfWord(sample.y), texel + 2);
            out->validity[index] = 0xFFu;
            ++out->validPixels;
        }
    }
    if (error != nullptr) error->clear();
    return true;
}

}  // namespace dxr
}  // namespace doom3
