#pragma once

// Shared Q2 fixture support (spec 004 T034/T037/T038).
//
// Everything here is scanned as a public test unit, so it names no backend type, native
// handle or private state. Order expectations are literal sequences from classic-parity.md
// §4, never derived from what the adapter happens to produce.

#include "rhi/engine_scene_adapter.h"
#include "rhi/lighting.h"
#include "rhi/materials.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace q2_test {

inline void Log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::vprintf(format, args);
    va_end(args);
    std::printf("\n");
    std::fflush(stdout);
}

inline bool Require(bool condition, const std::string& message) {
    if (!condition) {
        Log("ASSERTION FAILED: %s", message.c_str());
        return false;
    }
    return true;
}

inline bool RequireEq(uint32_t actual, uint32_t expected, const std::string& what) {
    if (actual != expected) {
        Log("ASSERTION FAILED: %s: expected %u, observed %u", what.c_str(), expected, actual);
        return false;
    }
    return true;
}

inline bool RequireNear(float actual, float expected, float tolerance, const std::string& what) {
    const float delta = actual > expected ? actual - expected : expected - actual;
    if (delta > tolerance) {
        Log("ASSERTION FAILED: %s: expected %.6f, observed %.6f", what.c_str(),
            static_cast<double>(expected), static_cast<double>(actual));
        return false;
    }
    return true;
}

using doom3::rhi::SceneCommandKind;

inline const char* ShortName(SceneCommandKind kind) {
    switch (kind) {
        case SceneCommandKind::BeginView: return "BeginView";
        case SceneCommandKind::EndView: return "EndView";
        case SceneCommandKind::DrawPacket: return "DrawPacket";
        case SceneCommandKind::SetColorTarget: return "SetColorTarget";
        case SceneCommandKind::CopyToImage: return "CopyToImage";
        case SceneCommandKind::SwapBuffers: return "SwapBuffers";
    }
    return "Unknown";
}

inline std::string Describe(const std::vector<SceneCommandKind>& sequence) {
    std::string text;
    for (size_t i = 0; i < sequence.size(); ++i) {
        if (i != 0) {
            text += " > ";
        }
        text += ShortName(sequence[i]);
    }
    return text;
}

// Exact sequence comparison. A still that looks right with a reordered stream is a failure
// (classic-parity.md §6.2 rule 5), so this never sorts, dedupes or tolerates extra entries.
inline bool SameSequence(const std::vector<SceneCommandKind>& actual,
                         const std::vector<SceneCommandKind>& expected) {
    if (actual.size() != expected.size()) {
        return false;
    }
    for (size_t i = 0; i < actual.size(); ++i) {
        if (actual[i] != expected[i]) {
            return false;
        }
    }
    return true;
}

inline bool RequireSequence(const std::vector<SceneCommandKind>& actual,
                            const std::vector<SceneCommandKind>& expected,
                            const std::string& what) {
    if (SameSequence(actual, expected)) {
        Log("ORDER %s: %s", what.c_str(), Describe(actual).c_str());
        return true;
    }
    Log("ORDER %s: expected [%s], observed [%s]", what.c_str(), Describe(expected).c_str(),
        Describe(actual).c_str());
    return false;
}

// The comparator must reject a reordered stream. Without this, an "equal" claim is
// unfalsifiable.
inline bool SequenceControlRejectsReorder() {
    std::vector<SceneCommandKind> forward = {SceneCommandKind::BeginView,
                                             SceneCommandKind::DrawPacket,
                                             SceneCommandKind::SwapBuffers};
    std::vector<SceneCommandKind> swapped = {SceneCommandKind::BeginView,
                                             SceneCommandKind::SwapBuffers,
                                             SceneCommandKind::DrawPacket};
    if (SameSequence(forward, swapped)) {
        Log("CONTROL comparator accepted a reordered stream");
        return false;
    }
    Log("CONTROL comparator rejects a reordered stream");
    return true;
}

inline doom3::rhi::DrawSurfPacket MakePacket(uint32_t surfaceId, uint32_t stageCount) {
    doom3::rhi::DrawSurfPacket packet;
    packet.surfaceId = surfaceId;
    packet.materialId = surfaceId * 7u;
    packet.vertexCount = 64;
    packet.indexCount = 96;
    packet.stageCount = stageCount;
    packet.lightCount = 1;
    return packet;
}

inline doom3::rhi::ViewDesc MakeSceneView(uint32_t identity, uint32_t width, uint32_t height) {
    doom3::rhi::ViewDesc view;
    view.identity = identity;
    view.width = width;
    view.height = height;
    return view;
}

}  // namespace q2_test
