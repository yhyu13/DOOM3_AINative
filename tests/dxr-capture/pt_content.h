#pragma once

// Q4 content atlas for the Q6 DXR capture (spec 004 T069/T070).
//
// The capture renders the authored Q4 cells, not synthetic colours. This unit
// decodes the committed PNG channels, writes them into two small atlases and
// records the byte hashes, so a capture can name the content it actually
// sampled:
//
//   base: RGBA16F linear RGB base colour (sRGB decoded), one block per cell.
//   pbr:  RGBA8, R = roughness, G = metalness, B = baseColor alpha, A = emissive
//         mask (sRGB decoded luminance).
//
// The environment cell is the declared four-tile 4096x4096 assembly, box-
// downsampled into one block. A channel the sidecar declares unavailable uses
// the neutral constant (roughness 1, metalness 0, alpha 1, emissive 0) and is
// recorded as such; it is never substituted with an invented texture.
//
// This header names no D3D12 type. The GPU upload lives in the capture tool.

#include "dxr/q6_content.h"

#include <cstdint>
#include <string>
#include <vector>

namespace q6cap {

constexpr uint32_t kAtlasBlock = 256;
constexpr uint32_t kAtlasColumns = 4;
constexpr uint32_t kAtlasRows = 2;

struct ContentCellSampling {
    doom3::dxr::Q6ContentCell cell = doom3::dxr::Q6ContentCell::Dielectric;
    uint32_t column = 0;
    uint32_t row = 0;
    bool baseColorSampled = false;
    bool roughnessSampled = false;
    bool metalnessSampled = false;
    bool emissiveSampled = false;
    bool alphaSampled = false;
    std::string note;
};

struct ContentAtlas {
    uint32_t width = 0;
    uint32_t height = 0;
    // RGBA16F, little-endian half words, row-major.
    std::vector<uint16_t> base;
    // RGBA8.
    std::vector<uint8_t> pbr;
    // RGBA16F linear emissive radiance: the authored emissive channel, scaled by
    // the sidecar's emissiveScale at shading time.
    std::vector<uint16_t> emissive;
    std::vector<ContentCellSampling> cells;
    std::string baseSha256;
    std::string pbrSha256;
    std::string emissiveSha256;
    std::string environmentAssembly;

    const ContentCellSampling* Find(doom3::dxr::Q6ContentCell cell) const;
};

// Decodes and assembles every cell. `repoRoot` is the repository root. A missing
// declared channel fails the load instead of producing a neutral atlas silently.
bool LoadContentAtlas(const std::string& repoRoot,
                      const doom3::dxr::Q6ContentRegistry& registry, ContentAtlas* atlas,
                      std::string* error);

// Row-major uv into the atlas block for `cell`. Returns false for an unknown
// cell. `uv` is wrapped to [0,1) and mapped into the block.
bool AtlasUv(const ContentAtlas& atlas, doom3::dxr::Q6ContentCell cell, float u, float v,
             float* atlasU, float* atlasV);

}  // namespace q6cap
