#pragma once

// Per-pixel motion and validity as device resources (spec 004 T085).
//
// The Q3 integration stores per-pixel motion in a bounded HistoryBuffer. This
// unit turns that buffer into two device-scoped textures so a subsequent frame
// can read the same values the CPU seam produced:
//
//   motion:   RGBA8Unorm, R/G = binary16 previous-minus-current du,
//             B/A = binary16 previous-minus-current dv, little endian.
//   validity: R8Unorm, 255 for a valid sample and 0 for an invalid one.
//
// Validity is a separate texture on purpose: a static camera produces a valid
// (0,0) vector, while an unprojectable sample carries no usable vector at all.
// Packing validity into a sentinel vector would make the two indistinguishable.
//
// No native type, handle or synchronization value appears here.

#include "rhi/frame_resources.h"
#include "rhi/rhi.h"

#include <cstdint>
#include <string>
#include <vector>

namespace doom3 {
namespace rhi {

struct MotionFrameResource {
    // The two device-scoped textures. A zero id means creation was refused.
    Texture motion;
    Texture validity;
    uint32_t width = 0;
    uint32_t height = 0;
    // The HistoryBuffer generation the textures were built from. A caller
    // compares it with a later generation instead of assuming the bytes are
    // current.
    uint64_t generation = 0;
    // Exact CPU mirror of the uploaded texels, for a fixture to check against
    // the readback without re-deriving the encoding.
    std::vector<uint8_t> packedMotion;
    std::vector<uint8_t> packedValidity;

    bool valid() const {
        return motion.id != 0 && validity.id != 0 && width != 0 && height != 0;
    }
};

// Uploads one HistoryBuffer into two textures. The buffer must have been
// resized; an empty buffer or a zero extent is InvalidArgument. A device
// failure is returned with an actionable reason and leaves `resource` empty.
Status CreateMotionFrameResources(Device& device, const HistoryBuffer& history,
                                  MotionFrameResource* resource,
                                  std::string* error);

}  // namespace rhi
}  // namespace doom3
