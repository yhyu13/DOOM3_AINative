#include "dds.h"

#include <d3d12.h>

#include <cstring>
#include <fstream>
#include <sstream>

namespace q4 {
namespace {

constexpr uint32_t kDdsMagic = 0x20534444u;  // "DDS "
constexpr uint32_t kFourCcDx10 = 0x30315844u;  // "DX10"
constexpr uint32_t kDdpfFourCc = 0x00000004u;

uint32_t ReadU32(const std::vector<uint8_t>& bytes, size_t offset) {
    uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

uint32_t BlockBytes(DXGI_FORMAT format) {
    switch (format) {
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            return 8;
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return 16;
        default:
            return 0;
    }
}

bool ReadFile(const std::string& path, std::vector<uint8_t>* bytes, std::string* error) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        if (error != nullptr) {
            *error = "cannot open " + path;
        }
        return false;
    }
    const std::streamoff size = file.tellg();
    if (size <= 0) {
        if (error != nullptr) {
            *error = path + " is empty";
        }
        return false;
    }
    file.seekg(0, std::ios::beg);
    bytes->resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(bytes->data()), size)) {
        if (error != nullptr) {
            *error = "cannot read " + path;
        }
        return false;
    }
    return true;
}

}  // namespace

bool LoadDds(const std::string& path, DdsImage* image, std::string* error) {
    *image = DdsImage{};
    std::vector<uint8_t> bytes;
    if (!ReadFile(path, &bytes, error)) {
        return false;
    }
    if (bytes.size() < 128 || ReadU32(bytes, 0) != kDdsMagic) {
        if (error != nullptr) {
            *error = path + ": not a DDS file";
        }
        return false;
    }

    const uint32_t headerSize = ReadU32(bytes, 4);
    if (headerSize != 124) {
        if (error != nullptr) {
            *error = path + ": unsupported DDS header size";
        }
        return false;
    }
    const uint32_t height = ReadU32(bytes, 12);
    const uint32_t width = ReadU32(bytes, 16);
    uint32_t mipCount = ReadU32(bytes, 28);
    if (mipCount == 0) {
        mipCount = 1;
    }
    const uint32_t pixelFlags = ReadU32(bytes, 80);
    const uint32_t fourCc = ReadU32(bytes, 84);
    if ((pixelFlags & kDdpfFourCc) == 0 || fourCc != kFourCcDx10) {
        if (error != nullptr) {
            *error = path + ": only DX10-header DDS files are supported";
        }
        return false;
    }
    if (bytes.size() < 148) {
        if (error != nullptr) {
            *error = path + ": truncated DX10 header";
        }
        return false;
    }
    const DXGI_FORMAT format =
        static_cast<DXGI_FORMAT>(ReadU32(bytes, 128));
    const uint32_t resourceDimension = ReadU32(bytes, 132);
    const uint32_t arraySize = ReadU32(bytes, 140);
    if (resourceDimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || arraySize != 1) {
        if (error != nullptr) {
            *error = path + ": only a single 2D texture is supported";
        }
        return false;
    }
    const uint32_t blockBytes = BlockBytes(format);
    if (blockBytes == 0) {
        if (error != nullptr) {
            *error = path + ": unsupported block-compressed DXGI format";
        }
        return false;
    }

    image->width = width;
    image->height = height;
    image->mipCount = mipCount;
    image->format = format;
    image->mips.reserve(mipCount);

    uint64_t offset = 148;
    for (uint32_t mip = 0; mip < mipCount; ++mip) {
        DdsMip entry;
        entry.width = width > mip ? width >> mip : 1;
        entry.height = height > mip ? height >> mip : 1;
        const uint32_t blocksWide = (entry.width + 3) / 4;
        const uint32_t blocksHigh = (entry.height + 3) / 4;
        entry.rowPitch = blocksWide * blockBytes;
        entry.rows = blocksHigh;
        entry.offset = offset;
        entry.size = static_cast<uint64_t>(entry.rowPitch) * blocksHigh;
        if (entry.offset + entry.size > bytes.size()) {
            if (error != nullptr) {
                *error = path + ": mip data is truncated";
            }
            return false;
        }
        offset += entry.size;
        image->mips.push_back(entry);
    }

    image->data = std::move(bytes);
    return true;
}

}  // namespace q4
