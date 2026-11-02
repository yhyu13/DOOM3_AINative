#pragma once

// Shared Q2 GPU fixture support (T080/T081/T082).
//
// Every program and expectation here is an independent literal. A readback is
// compared against a value derived from the fixture, never against itself. The
// header names only public RHI vocabulary, so the public-contract scan accepts
// it.

#include "q1b_test_support.h"

#include <cmath>
#include <string>
#include <vector>

namespace q2_gpu_test {

using q1a_test::kSkipExitCode;
using q1a_test::Log;
using q1a_test::Require;
using q1a_test::WaitComplete;

constexpr uint32_t kWidth = 8;
constexpr uint32_t kHeight = 8;
constexpr uint32_t kRowStride = kWidth * 4;

// Vertex program for a geometry item. The transform is column-major and the
// fixture supplies the identity, so positions are already in clip space.
inline const char* kItemVertex = R"(
cbuffer Item : register(b1) {
    float4x4 transform;
    float4 tint;
};

struct VSIn {
    float3 position : POSITION;
    float2 texCoord : TEXCOORD0;
};

struct VSOut {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOut VSMain(VSIn input) {
    VSOut output;
    output.position = mul(float4(input.position, 1.0f), transform);
    output.uv = input.texCoord;
    return output;
}
)";

// Per-item flat colour.
inline const char* kTintPixel = R"(
cbuffer Item : register(b1) {
    float4x4 transform;
    float4 tint;
};

struct VSOut {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 PSMain(VSOut input) : SV_Target {
    return tint;
}
)";

// Sample the item texture and multiply by the item tint.
inline const char* kTexturedPixel = R"(
cbuffer Item : register(b1) {
    float4x4 transform;
    float4 tint;
};

Texture2D itemTexture : register(t0);
SamplerState itemSampler : register(s0);

struct VSOut {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 PSMain(VSOut input) : SV_Target {
    return itemTexture.Sample(itemSampler, input.uv) * tint;
}
)";

// First channel replicated into RGB, used by the BC4/BC5 pins.
inline const char* kChannelPixel = R"(
cbuffer Item : register(b1) {
    float4x4 transform;
    float4 tint;
};

Texture2D itemTexture : register(t0);
SamplerState itemSampler : register(s0);

struct VSOut {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 PSMain(VSOut input) : SV_Target {
    float value = itemTexture.Sample(itemSampler, input.uv).r;
    return float4(value, value, value, 1.0f) * tint;
}
)";

// Red and green from a BC5 normal block, blue forced to zero.
inline const char* kTwoChannelPixel = R"(
cbuffer Item : register(b1) {
    float4x4 transform;
    float4 tint;
};

Texture2D itemTexture : register(t0);
SamplerState itemSampler : register(s0);

struct VSOut {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 PSMain(VSOut input) : SV_Target {
    float2 value = itemTexture.Sample(itemSampler, input.uv).rg;
    return float4(value.x, value.y, 0.0f, 1.0f) * tint;
}
)";

// Procedural fullscreen triangle whose colour is the pass tint (b0).
inline const char* kUniformPixel = R"(
cbuffer Tint : register(b0) {
    float4 tint;
};

struct VSOut {
    float4 position : SV_Position;
};

float4 PSMain(VSOut input) : SV_Target {
    return tint;
}
)";

inline doom3::rhi::Texture MakeRgba8Texture(doom3::rhi::Device& device,
                                            uint32_t width, uint32_t height,
                                            const std::vector<uint8_t>& bytes,
                                            doom3::rhi::TextureFormat format,
                                            std::string* error) {
    doom3::rhi::Texture texture;
    doom3::rhi::TextureDesc desc;
    desc.format = format;
    doom3::rhi::TextureMip mip;
    mip.width = width;
    mip.height = height;
    mip.bytes = bytes;
    desc.mips.push_back(std::move(mip));
    if (device.CreateTexture(desc, &texture, error) != doom3::rhi::Status::Ok) {
        return doom3::rhi::Texture{};
    }
    return texture;
}

inline bool CreateVertexBuffer(doom3::rhi::Device& device,
                               const std::vector<doom3::rhi::Vertex>& vertices,
                               doom3::rhi::Buffer* buffer, std::string* error) {
    doom3::rhi::BufferDesc desc;
    desc.usage = doom3::rhi::BufferUsage::Vertex;
    desc.bytes.resize(vertices.size() * sizeof(doom3::rhi::Vertex));
    std::memcpy(desc.bytes.data(), vertices.data(), desc.bytes.size());
    return device.CreateBuffer(desc, buffer, error) == doom3::rhi::Status::Ok;
}

inline bool CreateIndexBuffer(doom3::rhi::Device& device,
                              const std::vector<uint16_t>& indices,
                              doom3::rhi::Buffer* buffer, std::string* error) {
    doom3::rhi::BufferDesc desc;
    desc.usage = doom3::rhi::BufferUsage::Index;
    desc.bytes.resize(indices.size() * sizeof(uint16_t));
    std::memcpy(desc.bytes.data(), indices.data(), desc.bytes.size());
    return device.CreateBuffer(desc, buffer, error) == doom3::rhi::Status::Ok;
}

inline doom3::rhi::ShaderModule MakeShader(doom3::rhi::Device& device,
                                           doom3::rhi::ShaderStage stage,
                                           const char* source, const char* entry,
                                           std::string* error) {
    doom3::rhi::ShaderModule module;
    doom3::rhi::ShaderModuleDesc desc;
    desc.stage = stage;
    desc.source = source;
    desc.entryPoint = entry;
    if (device.CreateShaderModule(desc, &module, error) != doom3::rhi::Status::Ok) {
        return doom3::rhi::ShaderModule{};
    }
    return module;
}

inline bool CreateSurfacePipeline(doom3::rhi::Device& device,
                                  const doom3::rhi::ShaderModule& vertex,
                                  const doom3::rhi::ShaderModule& pixel,
                                  bool textured, doom3::rhi::Pipeline* pipeline,
                                  std::string* error) {
    doom3::rhi::GraphicsPipelineDesc desc;
    desc.vertexShader = vertex;
    desc.pixelShader = pixel;
    desc.colorAttachmentCount = 1;
    desc.vertexInput = true;
    desc.textured = textured;
    return device.CreateGraphicsPipeline(desc, pipeline, error) == doom3::rhi::Status::Ok;
}

inline bool CreateStencilPipeline(doom3::rhi::Device& device,
                                  const doom3::rhi::ShaderModule& vertex,
                                  const doom3::rhi::ShaderModule& pixel,
                                  uint32_t colorAttachments,
                                  doom3::rhi::CullMode cull, doom3::rhi::StencilOp op,
                                  bool vertexInput, uint8_t writeMask,
                                  doom3::rhi::Pipeline* pipeline, std::string* error) {
    doom3::rhi::GraphicsPipelineDesc desc;
    desc.vertexShader = vertex;
    desc.pixelShader = pixel;
    desc.colorAttachmentCount = colorAttachments;
    desc.depthTest = true;
    desc.depthWrite = false;
    desc.vertexInput = vertexInput;
    desc.cullMode = cull;
    desc.stencil.enabled = true;
    desc.stencil.writeMask = writeMask;
    desc.stencil.front.func = doom3::rhi::CompareFunc::Always;
    desc.stencil.front.failStencil = op;
    desc.stencil.front.failDepth = op;
    desc.stencil.front.pass = op;
    desc.stencil.back = desc.stencil.front;
    return device.CreateGraphicsPipeline(desc, pipeline, error) == doom3::rhi::Status::Ok;
}

inline bool CreateShadePipeline(doom3::rhi::Device& device,
                                const doom3::rhi::ShaderModule& vertex,
                                const doom3::rhi::ShaderModule& pixel,
                                doom3::rhi::Pipeline* pipeline, std::string* error) {
    doom3::rhi::GraphicsPipelineDesc desc;
    desc.vertexShader = vertex;
    desc.pixelShader = pixel;
    desc.colorAttachmentCount = 1;
    desc.depthTest = true;
    desc.depthWrite = false;
    desc.stencil.enabled = true;
    desc.stencil.writeMask = 0;
    doom3::rhi::StencilFaceState face;
    face.func = doom3::rhi::CompareFunc::Equal;
    face.failStencil = doom3::rhi::StencilOp::Keep;
    face.failDepth = doom3::rhi::StencilOp::Keep;
    face.pass = doom3::rhi::StencilOp::Keep;
    desc.stencil.front = face;
    desc.stencil.back = face;
    return device.CreateGraphicsPipeline(desc, pipeline, error) == doom3::rhi::Status::Ok;
}

inline bool PixelIs(const doom3::rhi::ReadbackResult& image, uint32_t x, uint32_t y,
                    uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha,
                    int tolerance = 1) {
    if (x >= image.width || y >= image.height ||
        image.pixels.size() < static_cast<size_t>(image.height) * image.rowStride) {
        return false;
    }
    const uint8_t* pixel =
        image.pixels.data() + static_cast<size_t>(y) * image.rowStride + x * 4;
    return std::abs(static_cast<int>(pixel[0]) - red) <= tolerance &&
           std::abs(static_cast<int>(pixel[1]) - green) <= tolerance &&
           std::abs(static_cast<int>(pixel[2]) - blue) <= tolerance &&
           std::abs(static_cast<int>(pixel[3]) - alpha) <= tolerance;
}

inline void PrintPixel(const char* label, uint32_t x, uint32_t y,
                       const doom3::rhi::ReadbackResult& image) {
    const uint8_t* pixel =
        image.pixels.data() + static_cast<size_t>(y) * image.rowStride + x * 4;
    Log("%s (%u,%u) = %u,%u,%u,%u", label, x, y, pixel[0], pixel[1], pixel[2],
        pixel[3]);
}

// A quad in clip space, with both windings so a cull-mode test does not depend
// on the backend's facing convention. Returns four triangles (two per winding).
inline std::vector<doom3::rhi::Vertex> QuadVertices(float left, float right,
                                                    float top, float bottom,
                                                    float depth) {
    const doom3::rhi::Vertex topLeft{{left, top, depth}, {0.0f, 0.0f}};
    const doom3::rhi::Vertex topRight{{right, top, depth}, {1.0f, 0.0f}};
    const doom3::rhi::Vertex bottomRight{{right, bottom, depth}, {1.0f, 1.0f}};
    const doom3::rhi::Vertex bottomLeft{{left, bottom, depth}, {0.0f, 1.0f}};
    return {topLeft, topRight, bottomRight, bottomLeft};
}

inline std::vector<uint16_t> QuadIndicesBothWindings() {
    return {0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2};
}

}  // namespace q2_gpu_test
