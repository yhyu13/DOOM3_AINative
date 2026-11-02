#pragma once

// Q4 content identity for the Q6 path-traced reference/showcase (spec 004 T069/T070).
//
// Q6 may only claim a PT-S1..S5 identity when the frame names the authored
// content cell, its licence state and its declared channels. This unit reads the
// seven committed sidecars under `content/materials/*.mat.json` and the licence
// files beside the image channels, and refuses a cell whose declared channel is
// missing on disk. It does not read game data, does not upload a texture and
// names no device type, so it is compiled and tested without a DXR device.
//
// What it does not do: decode a PNG, apply a sidecar value or decide that a
// channel is correct. Those are the capture tool's job; this record only proves
// that the identity a capture cites exists in the tree with a stated licence.

#include <cstdint>
#include <string>
#include <vector>

namespace doom3 {
namespace dxr {

enum class Q6ContentCell : uint32_t {
    Dielectric = 0,
    Metal,
    Grating,
    EmissiveSign,
    CharacterSkin,
    Glass,
    Environment,
    Count,
};

const char* ToString(Q6ContentCell cell);
const char* CellMaterialName(Q6ContentCell cell);
const char* CellMatrixName(Q6ContentCell cell);

// The declared state of one texture channel in a sidecar.
enum class Q6ChannelState : uint32_t {
    Authored = 0,
    Unavailable,
    Absent,
};

const char* ToString(Q6ChannelState state);

struct Q6Channel {
    Q6ChannelState state = Q6ChannelState::Absent;
    std::string path;
    // Recorded exactly as the sidecar states it; the tool decides how to apply it.
    std::string format;
    bool srgb = false;
    // Declared reason when the channel is unavailable. Empty for an authored channel.
    std::string reason;
};

struct Q6ContentCellRecord {
    Q6ContentCell cell = Q6ContentCell::Dielectric;
    std::string material;
    std::string mtr;
    std::string sidecar;
    std::string licence;
    // True only when the licence file exists and is non-empty.
    bool licencePresent = false;
    Q6Channel baseColor;
    Q6Channel normal;
    Q6Channel roughness;
    Q6Channel metalness;
    Q6Channel emissive;
    // Declared HDR scale for the emissive cell. 1.0 when the sidecar names none.
    float emissiveScale = 1.0f;
    // The four environment tiles when the cell is a tiled assembly; empty otherwise.
    std::vector<std::string> environmentTiles;
    // Required committed source bytes, measured on disk. Not a budget.
    uint64_t authoredBytes = 0;
    // True when every channel the sidecar declares "authored" exists and is non-empty.
    bool complete = false;
    std::string error;
};

struct Q6ContentRegistry {
    std::vector<Q6ContentCellRecord> cells;

    const Q6ContentCellRecord* Find(Q6ContentCell cell) const;
    // Number of cells whose declared authored channels all exist on disk.
    uint32_t CompleteCount() const;
    // Human-readable one-line identity for a capture record.
    std::string Identity(Q6ContentCell cell) const;
};

// Binds a Q2 surface id to the content cell the capture renders on it. A surface
// with no assignment is refused by the Q6 frame, because a capture must name its
// content.
struct Q6SurfaceAssignment {
    uint32_t surfaceId = 0;
    Q6ContentCell cell = Q6ContentCell::Dielectric;
    bool alphaTested = false;
};

// Reads the seven sidecars relative to `repoRoot` (the repository root, not the
// worktree of the caller). A missing sidecar, licence or declared channel is
// recorded on the cell and makes it incomplete; it is never substituted.
bool LoadQ6Content(const std::string& repoRoot, Q6ContentRegistry* registry, std::string* error);

// The content cells a PT identity must name, in draw order. Returns false for an
// unknown identity string.
bool CellsForIdentity(const std::string& identity, std::vector<Q6ContentCell>* cells);

}  // namespace dxr
}  // namespace doom3
