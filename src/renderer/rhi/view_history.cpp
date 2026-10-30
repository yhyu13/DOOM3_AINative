#include "rhi/view_history.h"

namespace doom3 {
namespace rhi {

namespace {

// Lower rank wins. The order matches the documented precedence in
// view_history.h; a new reason must be inserted here deliberately, not fall
// through to the end by accident.
int ReasonRank(MotionValidity reason) {
    switch (reason) {
        case MotionValidity::ProviderSwitch: return 0;
        case MotionValidity::Reset: return 1;
        case MotionValidity::Resize: return 2;
        case MotionValidity::Topology: return 3;
        case MotionValidity::EntityReuse: return 4;
        case MotionValidity::Generation: return 5;
        default: return 100;
    }
}

bool IsInvalidationReason(MotionValidity reason) {
    switch (reason) {
        case MotionValidity::Reset:
        case MotionValidity::Resize:
        case MotionValidity::Topology:
        case MotionValidity::EntityReuse:
        case MotionValidity::ProviderSwitch:
            return true;
        default:
            return false;
    }
}

// The first identity check that fails, in precedence order. Valid means the
// stored record answers this identity.
MotionValidity IdentityReason(const ViewIdentity& stored,
                              const ViewIdentity& requested) {
    if (requested.provider != stored.provider) {
        return MotionValidity::ProviderSwitch;
    }
    if (requested.worldId != stored.worldId) {
        return MotionValidity::Reset;
    }
    if (requested.width != stored.width || requested.height != stored.height) {
        return MotionValidity::Resize;
    }
    if (requested.topology != stored.topology) {
        return MotionValidity::Topology;
    }
    if (requested.entity != stored.entity) {
        return MotionValidity::EntityReuse;
    }
    if (requested.generation != stored.generation) {
        return MotionValidity::Generation;
    }
    return MotionValidity::Valid;
}

}  // namespace

bool RenderedViewHistory::CommitRendered(ViewKind kind,
                                         const ViewIdentity& identity,
                                         const RenderedViewState& state) {
    if (state.records.empty()) {
        return false;
    }
    Entry entry;
    entry.identity = identity;
    entry.state = state;
    // A commit replaces the pending reason: the view has rendered a state that
    // is usable for the next frame.
    entry.hasPendingReason = false;
    entry.pendingReason = MotionValidity::NoHistory;
    entries_[Key{kind, identity.instanceId}] = entry;
    return true;
}

MotionValidity RenderedViewHistory::Previous(
    ViewKind kind, const ViewIdentity& identity,
    RenderedViewState* previous) const {
    if (previous == nullptr) {
        return MotionValidity::NoHistory;
    }
    const auto found = entries_.find(Key{kind, identity.instanceId});
    if (found == entries_.end()) {
        return MotionValidity::NoHistory;
    }
    MotionValidity reason = IdentityReason(found->second.identity, identity);
    if (found->second.hasPendingReason
            && ReasonRank(found->second.pendingReason) < ReasonRank(reason)) {
        reason = found->second.pendingReason;
    }
    if (reason != MotionValidity::Valid) {
        return reason;
    }
    *previous = found->second.state;
    return MotionValidity::Valid;
}

void RenderedViewHistory::Invalidate(ViewKind kind,
                                     const ViewIdentity& identity,
                                     MotionValidity reason) {
    if (!IsInvalidationReason(reason)) {
        return;
    }
    const auto found = entries_.find(Key{kind, identity.instanceId});
    if (found == entries_.end()) {
        return;
    }
    found->second.hasPendingReason = true;
    found->second.pendingReason = reason;
}

void RenderedViewHistory::InvalidateAll(MotionValidity reason) {
    if (!IsInvalidationReason(reason)) {
        return;
    }
    for (auto& entry : entries_) {
        entry.second.hasPendingReason = true;
        entry.second.pendingReason = reason;
    }
}

void RenderedViewHistory::Clear() { entries_.clear(); }

}  // namespace rhi
}  // namespace doom3
