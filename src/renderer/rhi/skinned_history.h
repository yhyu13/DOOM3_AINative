#pragma once

// Private skinned-pose retention for Q3 (task T044).
//
// The engine's shared dynamic model can be overwritten or deleted between
// frames (idMD5Mesh::UpdateSurface, InstantiateDynamicModel). This seam keeps a
// private copy of the previous rendered posed positions, keyed by view and
// stable surface identity, so the previous-pose reprojection never aliases the
// shared cache. A subview commits only its own record; the main-view record is
// untouched.

#include "rhi/motion_history.h"

#include <cstdint>
#include <map>
#include <vector>

namespace doom3 {
namespace rhi {

enum class ViewKind : uint32_t {
    Main,
    Mirror,
    Camera,
    Shadow,
};

const char* ToString(ViewKind value);

class SkinnedPoseHistory {
public:
    // Copies `posedPositions` into private storage for (view, identity). An
    // empty pose is rejected. Returns false only for that rejection.
    bool CommitPose(ViewKind view, uint32_t identity, uint32_t generation,
                    const std::vector<MotionVec3>& posedPositions);

    // Copies the stored private pose to `positions` when the generation still
    // matches. Returns Valid, NoHistory or Generation. A generation mismatch
    // never returns stale positions.
    MotionValidity PreviousPose(ViewKind view, uint32_t identity,
                                uint32_t generation,
                                std::vector<MotionVec3>* positions) const;

    bool HasPose(ViewKind view, uint32_t identity) const;

    void Clear();

private:
    struct Key {
        ViewKind view = ViewKind::Main;
        uint32_t identity = 0;

        bool operator<(const Key& other) const {
            if (view != other.view) {
                return static_cast<uint32_t>(view) <
                       static_cast<uint32_t>(other.view);
            }
            return identity < other.identity;
        }
    };

    struct Entry {
        uint32_t generation = 0;
        std::vector<MotionVec3> positions;
    };

    std::map<Key, Entry> entries_;
};

}  // namespace rhi
}  // namespace doom3
