#include "pt_content.h"

#define NOMINMAX
#include <windows.h>

#include "rhi/motion_history.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include <bcrypt.h>
#include <wincodec.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace q6cap {
namespace {

struct Image {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgba;  // straight RGBA8
};

std::wstring Widen(const std::string& value) {
    if (value.empty()) {
        return std::wstring();
    }
    const int length =
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (length <= 0) {
        return std::wstring();
    }
    std::wstring wide(static_cast<size_t>(length - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wide.data(), length);
    return wide;
}

bool LoadPng(const std::string& path, Image* image, std::string* error) {
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        *error = "CoCreateInstance(WIC) failed for " + path;
        return false;
    }
    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(Widen(path).c_str(), nullptr, GENERIC_READ,
                                            WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) {
        *error = "WIC could not open " + path;
        return false;
    }
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) {
        *error = "WIC found no frame in " + path;
        return false;
    }
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter))) {
        *error = "WIC format converter failed";
        return false;
    }
    if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                                     WICBitmapDitherTypeNone, nullptr, 0.0,
                                     WICBitmapPaletteTypeCustom))) {
        *error = "WIC could not convert " + path + " to RGBA8";
        return false;
    }
    UINT width = 0;
    UINT height = 0;
    converter->GetSize(&width, &height);
    if (width == 0 || height == 0) {
        *error = "WIC reported a zero extent for " + path;
        return false;
    }
    image->width = width;
    image->height = height;
    const UINT stride = width * 4;
    image->rgba.resize(static_cast<size_t>(stride) * height);
    if (FAILED(converter->CopyPixels(nullptr, stride, static_cast<UINT>(image->rgba.size()),
                                     image->rgba.data()))) {
        *error = "WIC CopyPixels failed for " + path;
        return false;
    }
    return true;
}

float SrgbToLinear(float value) {
    return value <= 0.04045f ? value / 12.92f
                             : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

float Luma(uint8_t r, uint8_t g, uint8_t b) {
    return (0.2126f * static_cast<float>(r) + 0.7152f * static_cast<float>(g) +
            0.0722f * static_cast<float>(b)) /
           255.0f;
}

// Area-average downscale into an RGBA8 target of `targetWidth` x `targetHeight`.
void ResizeInto(const Image& source, uint32_t targetWidth, uint32_t targetHeight,
                std::vector<uint8_t>* out) {
    out->assign(static_cast<size_t>(targetWidth) * targetHeight * 4u, 0u);
    for (uint32_t y = 0; y < targetHeight; ++y) {
        const uint32_t y0 = y * source.height / targetHeight;
        const uint32_t y1 = std::max(y0 + 1, (y + 1) * source.height / targetHeight);
        for (uint32_t x = 0; x < targetWidth; ++x) {
            const uint32_t x0 = x * source.width / targetWidth;
            const uint32_t x1 = std::max(x0 + 1, (x + 1) * source.width / targetWidth);
            uint64_t sum[4] = {0, 0, 0, 0};
            uint64_t count = 0;
            for (uint32_t sy = y0; sy < y1 && sy < source.height; ++sy) {
                for (uint32_t sx = x0; sx < x1 && sx < source.width; ++sx) {
                    const size_t index =
                        (static_cast<size_t>(sy) * source.width + sx) * 4u;
                    for (int channel = 0; channel < 4; ++channel) {
                        sum[channel] += source.rgba[index + channel];
                    }
                    ++count;
                }
            }
            uint8_t* texel = out->data() + (static_cast<size_t>(y) * targetWidth + x) * 4u;
            for (int channel = 0; channel < 4; ++channel) {
                texel[channel] =
                    count == 0 ? 0 : static_cast<uint8_t>(sum[channel] / count);
            }
        }
    }
}

void StoreHalf(float value, uint16_t* out) {
    // The Q3 half conversion is shared with the motion texture packing, so the
    // atlas and the motion encoding cannot drift apart.
    *out = doom3::rhi::MotionHalfWord(static_cast<double>(value));
}

void WriteBaseTexel(ContentAtlas* atlas, uint32_t x, uint32_t y, float r, float g, float b,
                    float a) {
    const size_t index = (static_cast<size_t>(y) * atlas->width + x) * 4u;
    StoreHalf(r, &atlas->base[index + 0]);
    StoreHalf(g, &atlas->base[index + 1]);
    StoreHalf(b, &atlas->base[index + 2]);
    StoreHalf(a, &atlas->base[index + 3]);
}

void WriteEmissiveTexel(ContentAtlas* atlas, uint32_t x, uint32_t y, float r, float g,
                           float b, float a) {
    const size_t index = (static_cast<size_t>(y) * atlas->width + x) * 4u;
    StoreHalf(r, &atlas->emissive[index + 0]);
    StoreHalf(g, &atlas->emissive[index + 1]);
    StoreHalf(b, &atlas->emissive[index + 2]);
    StoreHalf(a, &atlas->emissive[index + 3]);
}

void WritePbrTexel(ContentAtlas* atlas, uint32_t x, uint32_t y, float roughness,
                   float metalness, float alpha, float emissive) {
    const size_t index = (static_cast<size_t>(y) * atlas->width + x) * 4u;
    auto toByte = [](float value) {
        return static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, value * 255.0f + 0.5f)));
    };
    atlas->pbr[index + 0] = toByte(roughness);
    atlas->pbr[index + 1] = toByte(metalness);
    atlas->pbr[index + 2] = toByte(alpha);
    atlas->pbr[index + 3] = toByte(emissive);
}

std::string Sha256(const void* data, size_t bytes) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::string hex;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
        return hex;
    }
    DWORD objectSize = 0;
    DWORD resultSize = 0;
    DWORD hashSize = 0;
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                          reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize),
                          &resultSize, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashSize),
                          sizeof(hashSize), &resultSize, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return hex;
    }
    std::vector<uint8_t> hashObject(objectSize);
    std::vector<uint8_t> digest(hashSize);
    if (BCryptCreateHash(algorithm, &hash, hashObject.data(), objectSize, nullptr, 0, 0) < 0 ||
        BCryptHashData(hash, static_cast<PUCHAR>(const_cast<void*>(data)),
                       static_cast<ULONG>(bytes), 0) < 0 ||
        BCryptFinishHash(hash, digest.data(), hashSize, 0) < 0) {
        if (hash != nullptr) {
            BCryptDestroyHash(hash);
        }
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return hex;
    }
    static const char* kHex = "0123456789abcdef";
    hex.resize(static_cast<size_t>(hashSize) * 2u);
    for (size_t i = 0; i < digest.size(); ++i) {
        hex[i * 2 + 0] = kHex[digest[i] >> 4];
        hex[i * 2 + 1] = kHex[digest[i] & 0x0F];
    }
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return hex;
}

uint32_t CellIndex(doom3::dxr::Q6ContentCell cell) {
    return static_cast<uint32_t>(cell);
}

}  // namespace

const ContentCellSampling* ContentAtlas::Find(doom3::dxr::Q6ContentCell cell) const {
    for (const ContentCellSampling& sampling : cells) {
        if (sampling.cell == cell) {
            return &sampling;
        }
    }
    return nullptr;
}

bool LoadContentAtlas(const std::string& repoRoot,
                      const doom3::dxr::Q6ContentRegistry& registry, ContentAtlas* atlas,
                      std::string* error) {
    if (atlas == nullptr) {
        if (error != nullptr) *error = "content atlas needs an output";
        return false;
    }
    *atlas = ContentAtlas{};
    atlas->width = kAtlasBlock * kAtlasColumns;
    atlas->height = kAtlasBlock * kAtlasRows;
    atlas->base.assign(static_cast<size_t>(atlas->width) * atlas->height * 4u, 0u);
    atlas->pbr.assign(static_cast<size_t>(atlas->width) * atlas->height * 4u, 0u);
    atlas->emissive.assign(static_cast<size_t>(atlas->width) * atlas->height * 4u, 0u);

    for (const doom3::dxr::Q6ContentCellRecord& record : registry.cells) {
        const uint32_t index = CellIndex(record.cell);
        ContentCellSampling sampling;
        sampling.cell = record.cell;
        sampling.column = index % kAtlasColumns;
        sampling.row = index / kAtlasColumns;
        const uint32_t originX = sampling.column * kAtlasBlock;
        const uint32_t originY = sampling.row * kAtlasBlock;

        std::vector<uint8_t> baseBlock(static_cast<size_t>(kAtlasBlock) * kAtlasBlock * 4u,
                                       255u);
        std::vector<uint8_t> emissiveBlock(static_cast<size_t>(kAtlasBlock) * kAtlasBlock *
                                               4u,
                                           0u);
        std::vector<uint8_t> roughnessBlock(kAtlasBlock * kAtlasBlock, 255u);
        std::vector<uint8_t> metalnessBlock(kAtlasBlock * kAtlasBlock, 0u);
        std::vector<uint8_t> alphaBlock(kAtlasBlock * kAtlasBlock, 255u);

        Image image;
        if (record.environmentTiles.size() == 4) {
            // Assemble the declared 4096 canvas: four 2048 tiles, each box
            // downsampled to 128, placed at its declared quadrant.
            for (size_t tile = 0; tile < 4; ++tile) {
                std::string tileError;
                Image tileImage;
                if (!LoadPng(repoRoot + "/" + record.environmentTiles[tile], &tileImage,
                             &tileError)) {
                    if (error != nullptr) *error = tileError;
                    return false;
                }
                std::vector<uint8_t> tileSmall;
                ResizeInto(tileImage, kAtlasBlock / 2, kAtlasBlock / 2, &tileSmall);
                const uint32_t offsetX = static_cast<uint32_t>(tile % 2) * (kAtlasBlock / 2);
                const uint32_t offsetY = static_cast<uint32_t>(tile / 2) * (kAtlasBlock / 2);
                for (uint32_t y = 0; y < kAtlasBlock / 2; ++y) {
                    for (uint32_t x = 0; x < kAtlasBlock / 2; ++x) {
                        const size_t source = (static_cast<size_t>(y) * (kAtlasBlock / 2) + x) *
                                              4u;
                        const size_t destination =
                            (static_cast<size_t>(y + offsetY) * kAtlasBlock +
                             (x + offsetX)) *
                            4u;
                        std::memcpy(baseBlock.data() + destination, tileSmall.data() + source, 4u);
                    }
                }
            }
            atlas->environmentAssembly =
                "4096x4096 assembled from four 2048 tiles, downsampled to one 256 block";
            sampling.baseColorSampled = true;
            sampling.alphaSampled = true;
        } else if (!record.baseColor.path.empty()) {
            std::string imageError;
            if (!LoadPng(repoRoot + "/" + record.baseColor.path, &image, &imageError)) {
                if (error != nullptr) *error = imageError;
                return false;
            }
            ResizeInto(image, kAtlasBlock, kAtlasBlock, &baseBlock);
            for (size_t i = 0; i < alphaBlock.size(); ++i) {
                alphaBlock[i] = baseBlock[i * 4 + 3];
            }
            sampling.baseColorSampled = true;
            sampling.alphaSampled = true;
        } else {
            if (error != nullptr) {
                *error = std::string(doom3::dxr::ToString(record.cell)) +
                         " has no baseColor source";
            }
            return false;
        }

        if (record.roughness.state == doom3::dxr::Q6ChannelState::Authored) {
            std::string imageError;
            Image roughness;
            if (!LoadPng(repoRoot + "/" + record.roughness.path, &roughness, &imageError)) {
                if (error != nullptr) *error = imageError;
                return false;
            }
            std::vector<uint8_t> block;
            ResizeInto(roughness, kAtlasBlock, kAtlasBlock, &block);
            for (size_t i = 0; i < roughnessBlock.size(); ++i) {
                roughnessBlock[i] = block[i * 4];
            }
            sampling.roughnessSampled = true;
        }
        if (record.metalness.state == doom3::dxr::Q6ChannelState::Authored) {
            std::string imageError;
            Image metalness;
            if (!LoadPng(repoRoot + "/" + record.metalness.path, &metalness, &imageError)) {
                if (error != nullptr) *error = imageError;
                return false;
            }
            std::vector<uint8_t> block;
            ResizeInto(metalness, kAtlasBlock, kAtlasBlock, &block);
            for (size_t i = 0; i < metalnessBlock.size(); ++i) {
                metalnessBlock[i] = block[i * 4];
            }
            sampling.metalnessSampled = true;
        }
        if (record.emissive.state == doom3::dxr::Q6ChannelState::Authored) {
            std::string imageError;
            Image emissive;
            if (!LoadPng(repoRoot + "/" + record.emissive.path, &emissive, &imageError)) {
                if (error != nullptr) *error = imageError;
                return false;
            }
            ResizeInto(emissive, kAtlasBlock, kAtlasBlock, &emissiveBlock);
            sampling.emissiveSampled = true;
        }

        if (!sampling.roughnessSampled) sampling.note += "roughness=neutral(1);";
        if (!sampling.metalnessSampled) sampling.note += "metalness=neutral(0);";
        if (!sampling.emissiveSampled) sampling.note += "emissive=neutral(0);";

        for (uint32_t y = 0; y < kAtlasBlock; ++y) {
            for (uint32_t x = 0; x < kAtlasBlock; ++x) {
                const size_t source = (static_cast<size_t>(y) * kAtlasBlock + x) * 4u;
                const uint8_t* base = baseBlock.data() + source;
                const uint8_t alpha = alphaBlock[static_cast<size_t>(y) * kAtlasBlock + x];
                const size_t emissiveIndex =
                    (static_cast<size_t>(y) * kAtlasBlock + x) * 4u;
                const float emissiveR = SrgbToLinear(emissiveBlock[emissiveIndex + 0] /
                                                    255.0f);
                const float emissiveG = SrgbToLinear(emissiveBlock[emissiveIndex + 1] /
                                                    255.0f);
                const float emissiveB = SrgbToLinear(emissiveBlock[emissiveIndex + 2] /
                                                    255.0f);
                const float emissiveValue =
                    Luma(emissiveBlock[emissiveIndex + 0], emissiveBlock[emissiveIndex + 1],
                         emissiveBlock[emissiveIndex + 2]);
                WriteBaseTexel(atlas, originX + x, originY + y,
                               SrgbToLinear(base[0] / 255.0f),
                               SrgbToLinear(base[1] / 255.0f),
                               SrgbToLinear(base[2] / 255.0f), 1.0f);
                WriteEmissiveTexel(atlas, originX + x, originY + y, emissiveR, emissiveG,
                                   emissiveB, 1.0f);
                WritePbrTexel(atlas, originX + x, originY + y,
                              roughnessBlock[static_cast<size_t>(y) * kAtlasBlock + x] /
                                  255.0f,
                              metalnessBlock[static_cast<size_t>(y) * kAtlasBlock + x] /
                                  255.0f,
                              alpha / 255.0f, emissiveValue);
            }
        }
        atlas->cells.push_back(std::move(sampling));
    }

    atlas->baseSha256 = Sha256(atlas->base.data(), atlas->base.size() * sizeof(uint16_t));
    atlas->pbrSha256 = Sha256(atlas->pbr.data(), atlas->pbr.size());
    atlas->emissiveSha256 =
        Sha256(atlas->emissive.data(), atlas->emissive.size() * sizeof(uint16_t));
    if (error != nullptr) error->clear();
    return true;
}

bool AtlasUv(const ContentAtlas& atlas, doom3::dxr::Q6ContentCell cell, float u, float v,
             float* atlasU, float* atlasV) {
    if (atlasU == nullptr || atlasV == nullptr || atlas.width == 0 || atlas.height == 0) {
        return false;
    }
    const ContentCellSampling* sampling = atlas.Find(cell);
    if (sampling == nullptr) {
        return false;
    }
    float wrappedU = u - std::floor(u);
    float wrappedV = v - std::floor(v);
    wrappedU = std::min(wrappedU, 0.999999f);
    wrappedV = std::min(wrappedV, 0.999999f);
    *atlasU = (static_cast<float>(sampling->column * kAtlasBlock) +
               wrappedU * static_cast<float>(kAtlasBlock)) /
              static_cast<float>(atlas.width);
    *atlasV = (static_cast<float>(sampling->row * kAtlasBlock) +
               wrappedV * static_cast<float>(kAtlasBlock)) /
              static_cast<float>(atlas.height);
    return true;
}

}  // namespace q6cap






