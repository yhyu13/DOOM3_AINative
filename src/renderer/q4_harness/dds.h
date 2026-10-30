#pragma once

// Minimal DDS reader for the Q4 package textures.
//
// The content pipeline (content/tools/build-material.ps1) writes DX10-header
// DDS files in BC7_UNORM_SRGB / BC5_UNORM / BC4_UNORM with a full mip chain.
// This reader accepts exactly those block-compressed layouts and records each
// mip's source row pitch so the D3D12 upload can copy it without guessing.

#include <dxgiformat.h>

#include <cstdint>
#include <string>
#include <vector>

namespace q4 {

struct DdsMip {
    uint32_t width = 0;
    uint32_t height = 0;
    // Compressed row pitch of the source data (blocksWide * blockBytes).
    uint32_t rowPitch = 0;
    // Number of compressed rows (block rows).
    uint32_t rows = 0;
    uint64_t offset = 0;
    uint64_t size = 0;
};

struct DdsImage {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipCount = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    std::vector<uint8_t> data;
    std::vector<DdsMip> mips;
};

bool LoadDds(const std::string& path, DdsImage* image, std::string* error);

}  // namespace q4
