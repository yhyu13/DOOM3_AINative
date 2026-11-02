#pragma once

// Device-independent packing for the Q3 per-pixel motion/validity textures
// (spec 004 T069/T070).
//
// `rhi/frame_motion_resources.h` defines the byte contract the Q3 device
// resource writes: RGBA8 with binary16 du in R/G and dv in B/A, little endian,
// plus an R8 validity texture (255 valid, 0 invalid). The Q6 capture creates the
// same two textures with raw D3D12 calls, so this unit performs that packing from
// the same Q3 `HistoryBuffer` and the same `MotionHalfWord` rounding. It is not a
// second validity policy: the vector and the validity come from the Q3 store.
//
// The resource identity is a dense copy of the Q3 buffer. A capture records the
// history generation so a later frame cannot assume the bytes are current.

#include "rhi/frame_resources.h"

#include <cstdint>
#include <string>
#include <vector>

namespace doom3 {
namespace dxr {

struct Q6MotionTextureData {
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t generation = 0;
    // RGBA8, 4 bytes per texel.
    std::vector<uint8_t> motion;
    // R8, 1 byte per texel.
    std::vector<uint8_t> validity;
    uint32_t validPixels = 0;
    uint32_t invalidPixels = 0;
};

// Packs the buffer. An empty or unresized buffer is refused and leaves `out`
// empty; a padded row is not produced, because the caller's upload pitch is its
// own concern.
bool PackMotionTexture(const rhi::HistoryBuffer& history, Q6MotionTextureData* out,
                       std::string* error);

}  // namespace dxr
}  // namespace doom3
