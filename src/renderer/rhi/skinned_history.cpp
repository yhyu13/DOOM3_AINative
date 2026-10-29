#include "rhi/skinned_history.h"

#include <utility>

namespace doom3 {
namespace rhi {

const char* ToString(ViewKind value) {
    switch (value) {
        case ViewKind::Main: return "MAIN";
        case ViewKind::Mirror: return "MIRROR";
        case ViewKind::Camera: return "CAMERA";
        case ViewKind::Shadow: return "SHADOW";
    }
    return "UNKNOWN";
}

bool SkinnedPoseHistory::CommitPose(
    ViewKind view, uint32_t identity, uint32_t generation,
    const std::vector<MotionVec3>& posedPositions) {
    if (posedPositions.empty()) {
        return false;
    }
    Entry entry;
    entry.generation = generation;
    entry.positions = posedPositions;  // private copy, never an alias
    entries_[Key{view, identity}] = std::move(entry);
    return true;
}

MotionValidity SkinnedPoseHistory::PreviousPose(
    ViewKind view, uint32_t identity, uint32_t generation,
    std::vector<MotionVec3>* positions) const {
    if (positions != nullptr) {
        positions->clear();
    }
    const auto found = entries_.find(Key{view, identity});
    if (found == entries_.end()) {
        return MotionValidity::NoHistory;
    }
    if (found->second.generation != generation) {
        return MotionValidity::Generation;
    }
    if (positions != nullptr) {
        *positions = found->second.positions;
    }
    return MotionValidity::Valid;
}

bool SkinnedPoseHistory::HasPose(ViewKind view, uint32_t identity) const {
    return entries_.find(Key{view, identity}) != entries_.end();
}

void SkinnedPoseHistory::Clear() { entries_.clear(); }

}  // namespace rhi
}  // namespace doom3
