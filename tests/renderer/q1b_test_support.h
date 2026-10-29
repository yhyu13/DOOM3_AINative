#pragma once

// Shared Q1b public-fixture support.
//
// Everything here is scanned as a public test unit, so it names no backend
// type, handle or private vocabulary. Program text and expected values are
// independent literals; no expectation is derived from a readback.

#include "q1a_test_support.h"
#include "rhi/frame_resources.h"
#include "rhi/rhi.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace q1b_test {

using q1a_test::ForEachAdapter;
using q1a_test::kSkipExitCode;
using q1a_test::Log;
using q1a_test::Require;
using q1a_test::WaitComplete;

constexpr uint32_t kWidth = 8;
constexpr uint32_t kHeight = 8;
constexpr uint32_t kRowStride = kWidth * 4;
constexpr int kTolerance = 1;

// Vertex program: one oversized triangle covering the whole viewport.
inline const char* kFullscreenVertex = R"(
struct VSOut {
    float4 position : SV_Position;
};

VSOut VSMain(uint id : SV_VertexID) {
    float2 corner = float2((id == 2) ? 3.0f : -1.0f, (id == 1) ? 3.0f : -1.0f);
    VSOut output;
    output.position = float4(corner, 0.0f, 1.0f);
    return output;
}
)";

// Colour program: channel values are recoverable from the pixel coordinate.
inline const char* kPatternPixel = R"(
cbuffer Tint : register(b0) {
    float4 tint;
};

struct VSOut {
    float4 position : SV_Position;
};

float4 PSMain(VSOut input) : SV_Target {
    uint2 pixel = uint2(input.position.xy);
    float4 base = float4(pixel.x * 32.0f / 255.0f, pixel.y * 32.0f / 255.0f,
                         128.0f / 255.0f, 1.0f);
    return float4(base.rgb * tint.rgb, base.a);
}
)";

// Three distinct colour outputs.
inline const char* kMrtPixel = R"(
cbuffer Tint : register(b0) {
    float4 tint;
};

struct VSOut {
    float4 position : SV_Position;
};

struct PSOut {
    float4 first : SV_Target0;
    float4 second : SV_Target1;
    float4 third : SV_Target2;
};

PSOut PSMain(VSOut input, out float depth : SV_Depth) {
    depth = (input.position.x < 1.0f) ? 1.0f
            : ((input.position.x < 4.0f) ? 0.25f : 0.75f);
    PSOut output;
    output.first = float4(10.0f, 20.0f, 30.0f, 255.0f) / 255.0f * tint;
    output.second = float4(40.0f, 50.0f, 60.0f, 255.0f) / 255.0f * tint;
    output.third = float4(70.0f, 80.0f, 90.0f, 255.0f) / 255.0f * tint;
    return output;
}
)";

// Samples the depth attachment written by the previous pass.
inline const char* kDepthSamplePixel = R"(
Texture2D<float> depthInput : register(t0);
SamplerState pointSampler : register(s0);

struct VSOut {
    float4 position : SV_Position;
};

float4 PSMain(VSOut input) : SV_Target {
    float2 uv = input.position.xy / float2(8.0f, 8.0f);
    float depth = depthInput.SampleLevel(pointSampler, uv, 0);
    return float4(depth, 0.0f, 0.0f, 1.0f);
}
)";

// Writes a value recoverable from the dispatch coordinate.
inline const char* kComputeProgram = R"(
RWStructuredBuffer<uint> results : register(u0);

[numthreads(4, 4, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    if (id.x >= 4u || id.y >= 4u) {
        return;
    }
    results[id.y * 4u + id.x] = ((id.x * 16u + id.y * 3u + 7u) ^ 0xA5u);
}
)";

// Literal expected RGBA8 pattern for each uniform tint. These bytes are
// independent of the program and of any readback.
inline constexpr uint32_t kTintCount = 4;
inline constexpr float kTints[kTintCount] = {1.0f, 0.5f, 0.25f, 0.75f};
inline constexpr uint8_t kPattern[kTintCount][kHeight][kWidth][4] = {
    {  // tint 1.00
        {0, 0, 128, 255, 32, 0, 128, 255, 64, 0, 128, 255, 96, 0, 128, 255, 128, 0, 128, 255, 160, 0, 128, 255, 192, 0, 128, 255, 224, 0, 128, 255},
        {0, 32, 128, 255, 32, 32, 128, 255, 64, 32, 128, 255, 96, 32, 128, 255, 128, 32, 128, 255, 160, 32, 128, 255, 192, 32, 128, 255, 224, 32, 128, 255},
        {0, 64, 128, 255, 32, 64, 128, 255, 64, 64, 128, 255, 96, 64, 128, 255, 128, 64, 128, 255, 160, 64, 128, 255, 192, 64, 128, 255, 224, 64, 128, 255},
        {0, 96, 128, 255, 32, 96, 128, 255, 64, 96, 128, 255, 96, 96, 128, 255, 128, 96, 128, 255, 160, 96, 128, 255, 192, 96, 128, 255, 224, 96, 128, 255},
        {0, 128, 128, 255, 32, 128, 128, 255, 64, 128, 128, 255, 96, 128, 128, 255, 128, 128, 128, 255, 160, 128, 128, 255, 192, 128, 128, 255, 224, 128, 128, 255},
        {0, 160, 128, 255, 32, 160, 128, 255, 64, 160, 128, 255, 96, 160, 128, 255, 128, 160, 128, 255, 160, 160, 128, 255, 192, 160, 128, 255, 224, 160, 128, 255},
        {0, 192, 128, 255, 32, 192, 128, 255, 64, 192, 128, 255, 96, 192, 128, 255, 128, 192, 128, 255, 160, 192, 128, 255, 192, 192, 128, 255, 224, 192, 128, 255},
        {0, 224, 128, 255, 32, 224, 128, 255, 64, 224, 128, 255, 96, 224, 128, 255, 128, 224, 128, 255, 160, 224, 128, 255, 192, 224, 128, 255, 224, 224, 128, 255},
    },
    {  // tint 0.50
        {0, 0, 64, 255, 16, 0, 64, 255, 32, 0, 64, 255, 48, 0, 64, 255, 64, 0, 64, 255, 80, 0, 64, 255, 96, 0, 64, 255, 112, 0, 64, 255},
        {0, 16, 64, 255, 16, 16, 64, 255, 32, 16, 64, 255, 48, 16, 64, 255, 64, 16, 64, 255, 80, 16, 64, 255, 96, 16, 64, 255, 112, 16, 64, 255},
        {0, 32, 64, 255, 16, 32, 64, 255, 32, 32, 64, 255, 48, 32, 64, 255, 64, 32, 64, 255, 80, 32, 64, 255, 96, 32, 64, 255, 112, 32, 64, 255},
        {0, 48, 64, 255, 16, 48, 64, 255, 32, 48, 64, 255, 48, 48, 64, 255, 64, 48, 64, 255, 80, 48, 64, 255, 96, 48, 64, 255, 112, 48, 64, 255},
        {0, 64, 64, 255, 16, 64, 64, 255, 32, 64, 64, 255, 48, 64, 64, 255, 64, 64, 64, 255, 80, 64, 64, 255, 96, 64, 64, 255, 112, 64, 64, 255},
        {0, 80, 64, 255, 16, 80, 64, 255, 32, 80, 64, 255, 48, 80, 64, 255, 64, 80, 64, 255, 80, 80, 64, 255, 96, 80, 64, 255, 112, 80, 64, 255},
        {0, 96, 64, 255, 16, 96, 64, 255, 32, 96, 64, 255, 48, 96, 64, 255, 64, 96, 64, 255, 80, 96, 64, 255, 96, 96, 64, 255, 112, 96, 64, 255},
        {0, 112, 64, 255, 16, 112, 64, 255, 32, 112, 64, 255, 48, 112, 64, 255, 64, 112, 64, 255, 80, 112, 64, 255, 96, 112, 64, 255, 112, 112, 64, 255},
    },
    {  // tint 0.25
        {0, 0, 32, 255, 8, 0, 32, 255, 16, 0, 32, 255, 24, 0, 32, 255, 32, 0, 32, 255, 40, 0, 32, 255, 48, 0, 32, 255, 56, 0, 32, 255},
        {0, 8, 32, 255, 8, 8, 32, 255, 16, 8, 32, 255, 24, 8, 32, 255, 32, 8, 32, 255, 40, 8, 32, 255, 48, 8, 32, 255, 56, 8, 32, 255},
        {0, 16, 32, 255, 8, 16, 32, 255, 16, 16, 32, 255, 24, 16, 32, 255, 32, 16, 32, 255, 40, 16, 32, 255, 48, 16, 32, 255, 56, 16, 32, 255},
        {0, 24, 32, 255, 8, 24, 32, 255, 16, 24, 32, 255, 24, 24, 32, 255, 32, 24, 32, 255, 40, 24, 32, 255, 48, 24, 32, 255, 56, 24, 32, 255},
        {0, 32, 32, 255, 8, 32, 32, 255, 16, 32, 32, 255, 24, 32, 32, 255, 32, 32, 32, 255, 40, 32, 32, 255, 48, 32, 32, 255, 56, 32, 32, 255},
        {0, 40, 32, 255, 8, 40, 32, 255, 16, 40, 32, 255, 24, 40, 32, 255, 32, 40, 32, 255, 40, 40, 32, 255, 48, 40, 32, 255, 56, 40, 32, 255},
        {0, 48, 32, 255, 8, 48, 32, 255, 16, 48, 32, 255, 24, 48, 32, 255, 32, 48, 32, 255, 40, 48, 32, 255, 48, 48, 32, 255, 56, 48, 32, 255},
        {0, 56, 32, 255, 8, 56, 32, 255, 16, 56, 32, 255, 24, 56, 32, 255, 32, 56, 32, 255, 40, 56, 32, 255, 48, 56, 32, 255, 56, 56, 32, 255},
    },
    {  // tint 0.75
        {0, 0, 96, 255, 24, 0, 96, 255, 48, 0, 96, 255, 72, 0, 96, 255, 96, 0, 96, 255, 120, 0, 96, 255, 144, 0, 96, 255, 168, 0, 96, 255},
        {0, 24, 96, 255, 24, 24, 96, 255, 48, 24, 96, 255, 72, 24, 96, 255, 96, 24, 96, 255, 120, 24, 96, 255, 144, 24, 96, 255, 168, 24, 96, 255},
        {0, 48, 96, 255, 24, 48, 96, 255, 48, 48, 96, 255, 72, 48, 96, 255, 96, 48, 96, 255, 120, 48, 96, 255, 144, 48, 96, 255, 168, 48, 96, 255},
        {0, 72, 96, 255, 24, 72, 96, 255, 48, 72, 96, 255, 72, 72, 96, 255, 96, 72, 96, 255, 120, 72, 96, 255, 144, 72, 96, 255, 168, 72, 96, 255},
        {0, 96, 96, 255, 24, 96, 96, 255, 48, 96, 96, 255, 72, 96, 96, 255, 96, 96, 96, 255, 120, 96, 96, 255, 144, 96, 96, 255, 168, 96, 96, 255},
        {0, 120, 96, 255, 24, 120, 96, 255, 48, 120, 96, 255, 72, 120, 96, 255, 96, 120, 96, 255, 120, 120, 96, 255, 144, 120, 96, 255, 168, 120, 96, 255},
        {0, 144, 96, 255, 24, 144, 96, 255, 48, 144, 96, 255, 72, 144, 96, 255, 96, 144, 96, 255, 120, 144, 96, 255, 144, 144, 96, 255, 168, 144, 96, 255},
        {0, 168, 96, 255, 24, 168, 96, 255, 48, 168, 96, 255, 72, 168, 96, 255, 96, 168, 96, 255, 120, 168, 96, 255, 144, 168, 96, 255, 168, 168, 96, 255},
    },
};

inline bool VerifyPattern(const doom3::rhi::ReadbackResult& result, uint32_t variant,
                          std::string* detail) {
    if (!Require(variant < kTintCount, "pattern variant is out of range")) {
        return false;
    }
    if (!Require(result.width == kWidth && result.height == kHeight,
                 "pattern readback dimensions differ from the frame")) {
        return false;
    }
    if (!Require(result.rowStride == kRowStride, "pattern row is not tightly packed")) {
        return false;
    }
    if (!Require(result.pixels.size() == static_cast<size_t>(kHeight) * kRowStride,
                 "pattern readback byte count is wrong")) {
        return false;
    }
    bool ok = true;
    for (uint32_t y = 0; y < kHeight; ++y) {
        for (uint32_t x = 0; x < kWidth; ++x) {
            const size_t offset = static_cast<size_t>(y) * kRowStride + x * 4;
            for (uint32_t channel = 0; channel < 4; ++channel) {
                const int expected = kPattern[variant][y][x][channel];
                const int actual = result.pixels[offset + channel];
                if (std::abs(expected - actual) > kTolerance) {
                    Log("PIXEL_MISMATCH (%u,%u) channel=%u expected=%d actual=%d",
                        x, y, channel, expected, actual);
                    ok = false;
                }
            }
        }
    }
    if (detail != nullptr) {
        *detail = ok ? "every pixel matches the literal pattern"
                     : "one or more pixels differ from the literal pattern";
    }
    return ok;
}
inline void PrintPixel(const char* label, uint32_t x, uint32_t y,
                       const doom3::rhi::ReadbackResult& result) {
    const size_t offset = static_cast<size_t>(y) * result.rowStride + x * 4;
    if (offset + 4 > result.pixels.size()) {
        Log("%s (%u,%u) OUT_OF_RANGE", label, x, y);
        return;
    }
    const uint8_t* p = result.pixels.data() + offset;
    Log("%s (%u,%u) = %u,%u,%u,%u", label, x, y, p[0], p[1], p[2], p[3]);
}
// A test-side control: comparing a pixel with its right-hand neighbour must
// fail because the pattern varies with x.
inline bool AdjacentPixelControl(const doom3::rhi::ReadbackResult& result) {
    if (result.width < 2 || result.rowStride != kRowStride) {
        return false;
    }
    uint32_t mismatches = 0;
    for (uint32_t y = 0; y < result.height; ++y) {
        for (uint32_t x = 0; x + 1 < result.width; ++x) {
            const size_t left = static_cast<size_t>(y) * kRowStride + x * 4;
            const size_t right = static_cast<size_t>(y) * kRowStride + (x + 1) * 4;
            if (std::memcmp(result.pixels.data() + left, result.pixels.data() + right, 4) !=
                0) {
                ++mismatches;
            }
        }
    }
    Log("CONTROL adjacent_pixel mismatches=%u", mismatches);
    return mismatches > 0;
}

inline bool WriteBmp(const std::string& path, const doom3::rhi::ReadbackResult& result,
                     uint32_t scale = 16) {
    return q1a_test::WriteBmp(path, result, scale);
}

inline bool VerifySolidColour(const doom3::rhi::ReadbackResult& result,
                              const doom3::rhi::ClearRow& colour) {
    if (result.width == 0 || result.height == 0) {
        return false;
    }
    for (uint32_t y = 0; y < result.height; ++y) {
        for (uint32_t x = 0; x < result.width; ++x) {
            const size_t offset = static_cast<size_t>(y) * result.rowStride + x * 4;
            if (offset + 4 > result.pixels.size()) {
                return false;
            }
            if (std::abs(result.pixels[offset + 0] - colour.red) > kTolerance ||
                std::abs(result.pixels[offset + 1] - colour.green) > kTolerance ||
                std::abs(result.pixels[offset + 2] - colour.blue) > kTolerance ||
                std::abs(result.pixels[offset + 3] - colour.alpha) > kTolerance) {
                Log("SOLID_MISMATCH (%u,%u) actual=%u,%u,%u,%u", x, y,
                    result.pixels[offset + 0], result.pixels[offset + 1],
                    result.pixels[offset + 2], result.pixels[offset + 3]);
                return false;
            }
        }
    }
    return true;
}

}  // namespace q1b_test
