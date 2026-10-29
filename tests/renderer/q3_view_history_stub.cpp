// Build-only placeholder for the PROPOSED view-history seam in
// tests/renderer/q3_view_history_seam.h.
//
// This file is not production code and proves nothing. It exists so the red
// tests produce a runtime assertion failure instead of a link error, because a
// compile/link failure is not red evidence (tasks.md, motion-fixtures.md 9).
// Every entry point announces itself on stderr and reports "nothing was
// rendered", which is the state before the production slice exists.
//
// The production slice named by T043/T045 replaces this file with
// src/renderer/rhi/view_history.cpp and promotes the proposed header into
// src/renderer/rhi/view_history.h.

#include "q3_view_history_seam.h"

#include <cstdint>
#include <cstdio>

namespace doom3 {
namespace rhi {

namespace {

void Announce(const char* name) {
    std::fprintf(stderr, "SEAM-STUB %s: production implementation absent\n", name);
}

}  // namespace

bool RenderedViewHistory::CommitRendered(ViewKind kind,
                                         const ViewIdentity& identity,
                                         const RenderedViewState& state) {
    (void)kind;
    (void)identity;
    (void)state;
    Announce("RenderedViewHistory::CommitRendered");
    return false;
}

MotionValidity RenderedViewHistory::Previous(
    ViewKind kind, const ViewIdentity& identity,
    RenderedViewState* previous) const {
    (void)kind;
    (void)identity;
    (void)previous;
    Announce("RenderedViewHistory::Previous");
    return MotionValidity::NoHistory;
}

void RenderedViewHistory::Invalidate(ViewKind kind,
                                     const ViewIdentity& identity,
                                     MotionValidity reason) {
    (void)kind;
    (void)identity;
    (void)reason;
    Announce("RenderedViewHistory::Invalidate");
}

void RenderedViewHistory::InvalidateAll(MotionValidity reason) {
    (void)reason;
    Announce("RenderedViewHistory::InvalidateAll");
}

void RenderedViewHistory::Clear() { Announce("RenderedViewHistory::Clear"); }

}  // namespace rhi
}  // namespace doom3
