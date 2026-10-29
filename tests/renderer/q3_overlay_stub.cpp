// Build-only placeholder for the PROPOSED overlay seam in
// tests/renderer/q3_overlay_seam.h.
//
// This file is not production code and proves nothing. It exists so the red
// test produces a runtime assertion failure instead of a link error, because a
// compile/link failure is not red evidence (tasks.md, motion-fixtures.md 9).
// Every entry point announces itself on stderr and refuses to plan or map.
//
// The production slice named by T047 replaces this file with
// src/renderer/rhi/overlay_composition.cpp and promotes the proposed header
// into src/renderer/rhi/overlay_composition.h.

#include "q3_overlay_seam.h"

#include <cstdint>
#include <cstdio>

namespace doom3 {
namespace rhi {

namespace {

void Announce(const char* name) {
    std::fprintf(stderr, "SEAM-STUB %s: production implementation absent\n", name);
}

}  // namespace

PostReconstructionPlan PlanPostReconstruction(
    const std::vector<ComposeLayer>& layers, FrameExtent renderExtent,
    FrameExtent displayExtent) {
    (void)layers;
    (void)renderExtent;
    (void)displayExtent;
    Announce("PlanPostReconstruction");
    PostReconstructionPlan plan;
    plan.status = Status::Unsupported;
    plan.error = "q3 seam stub: post-reconstruction composition is not implemented";
    return plan;
}

bool MapDisplayTexel(const ComposeLayer& layer, FrameExtent renderExtent,
                     FrameExtent displayExtent, uint32_t x, uint32_t y,
                     uint32_t* sourceX, uint32_t* sourceY) {
    (void)layer;
    (void)renderExtent;
    (void)displayExtent;
    (void)x;
    (void)y;
    (void)sourceX;
    (void)sourceY;
    Announce("MapDisplayTexel");
    return false;
}

}  // namespace rhi
}  // namespace doom3
