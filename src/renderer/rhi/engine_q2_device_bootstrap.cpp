#include "rhi/engine_q2_device_bootstrap.h"

#include <cstring>

namespace doom3 {
namespace rhi {

namespace {

// The classic programs. They are the same four effects the synthetic fixtures
// compile: a transformed item, a sampled item, a flat tint and a full-screen
// shade tint. Keeping them here means the engine path and the fixtures cannot
// drift into different pipeline contracts.
const char* kItemVertex = R"(
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

const char* kTintPixel = R"(
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

const char* kTexturedPixel = R"(
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

const char* kFullscreenVertex = R"(
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

const char* kUniformPixel = R"(
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

ShaderModule CreateShader(Device& device, ShaderStage stage,
                          const char* source, const char* entry,
                          std::string* error) {
    ShaderModule module;
    ShaderModuleDesc desc;
    desc.stage = stage;
    desc.source = source;
    desc.entryPoint = entry;
    std::string compileError;
    if (device.CreateShaderModule(desc, &module, &compileError) != Status::Ok) {
        if (error != nullptr) {
            *error = std::string("classic ") +
                     (stage == ShaderStage::Vertex ? "vertex" : "pixel") +
                     " program failed to compile: " + compileError;
        }
        return ShaderModule{};
    }
    return module;
}

bool CreateSurfacePipeline(Device& device, const ShaderModule& vertex,
                           const ShaderModule& pixel, Pipeline* pipeline,
                           std::string* error) {
    GraphicsPipelineDesc desc;
    desc.vertexShader = vertex;
    desc.pixelShader = pixel;
    desc.colorAttachmentCount = 1;
    desc.vertexInput = true;
    desc.textured = true;
    return device.CreateGraphicsPipeline(desc, pipeline, error) == Status::Ok;
}

bool CreateStencilPipeline(Device& device, const ShaderModule& vertex,
                           const ShaderModule& pixel, CullMode cull,
                           StencilOp op, Pipeline* pipeline,
                           std::string* error) {
    GraphicsPipelineDesc desc;
    desc.vertexShader = vertex;
    desc.pixelShader = pixel;
    desc.colorAttachmentCount = 0;
    desc.depthTest = true;
    desc.depthWrite = false;
    desc.vertexInput = true;
    desc.cullMode = cull;
    desc.stencil.enabled = true;
    desc.stencil.writeMask = 0xFF;
    desc.stencil.front.func = CompareFunc::Always;
    desc.stencil.front.failStencil = op;
    desc.stencil.front.failDepth = op;
    desc.stencil.front.pass = op;
    desc.stencil.back = desc.stencil.front;
    return device.CreateGraphicsPipeline(desc, pipeline, error) == Status::Ok;
}

bool CreateShadePipeline(Device& device, const ShaderModule& vertex,
                         const ShaderModule& pixel, Pipeline* pipeline,
                         std::string* error) {
    GraphicsPipelineDesc desc;
    desc.vertexShader = vertex;
    desc.pixelShader = pixel;
    desc.colorAttachmentCount = 1;
    desc.depthTest = true;
    desc.depthWrite = false;
    desc.stencil.enabled = true;
    desc.stencil.writeMask = 0;
    StencilFaceState face;
    face.func = CompareFunc::Equal;
    face.failStencil = StencilOp::Keep;
    face.failDepth = StencilOp::Keep;
    face.pass = StencilOp::Keep;
    desc.stencil.front = face;
    desc.stencil.back = face;
    return device.CreateGraphicsPipeline(desc, pipeline, error) == Status::Ok;
}

}  // namespace

bool CreateEngineQ2ClassicPipelines(Device& device,
                                    EngineQ2DevicePipelines* pipelines,
                                    std::string* error) {
    if (pipelines == nullptr) {
        if (error != nullptr) {
            *error = "pipeline creation needs an output descriptor";
        }
        return false;
    }
    *pipelines = EngineQ2DevicePipelines();

    std::string compileError;
    const ShaderModule itemVertex = CreateShader(
        device, ShaderStage::Vertex, kItemVertex, "VSMain", &compileError);
    const ShaderModule tintPixel = CreateShader(
        device, ShaderStage::Pixel, kTintPixel, "PSMain", &compileError);
    const ShaderModule texturedPixel = CreateShader(
        device, ShaderStage::Pixel, kTexturedPixel, "PSMain", &compileError);
    const ShaderModule fullscreenVertex = CreateShader(
        device, ShaderStage::Vertex, kFullscreenVertex, "VSMain", &compileError);
    const ShaderModule uniformPixel = CreateShader(
        device, ShaderStage::Pixel, kUniformPixel, "PSMain", &compileError);
    if (itemVertex.id == 0 || tintPixel.id == 0 || texturedPixel.id == 0 ||
        fullscreenVertex.id == 0 || uniformPixel.id == 0) {
        if (error != nullptr) {
            *error = compileError;
        }
        return false;
    }

    std::string pipelineError;
    if (!CreateSurfacePipeline(device, itemVertex, texturedPixel,
                               &pipelines->surface, &pipelineError) ||
        !CreateStencilPipeline(device, itemVertex, tintPixel,
                               CullMode::Front, StencilOp::IncrementSaturate,
                               &pipelines->shadowBack, &pipelineError) ||
        !CreateStencilPipeline(device, itemVertex, tintPixel,
                               CullMode::Back, StencilOp::DecrementSaturate,
                               &pipelines->shadowFront, &pipelineError) ||
        !CreateShadePipeline(device, fullscreenVertex, uniformPixel,
                             &pipelines->shade, &pipelineError)) {
        if (error != nullptr) {
            *error = "classic pipeline creation failed: " + pipelineError;
        }
        return false;
    }
    return true;
}

EngineQ2DeviceRuntime::EngineQ2DeviceRuntime() = default;

EngineQ2DeviceRuntime::~EngineQ2DeviceRuntime() { Shutdown(); }

bool EngineQ2DeviceRuntime::Create(int width, int height, std::string* error) {
    Shutdown();

    auto device = std::make_unique<Device>();
    DeviceOptions options;
    options.adapterClass = AdapterClass::Hardware;
    options.adapterIndex = 0;
    std::string stageError;
    if (device->Initialize(options, &stageError) != Status::Ok) {
        if (error != nullptr) {
            *error = "the D3D12 classic device could not be created: " + stageError;
        }
        return false;
    }

    EngineQ2DevicePipelines pipelines;
    if (!CreateEngineQ2ClassicPipelines(*device, &pipelines, &stageError)) {
        std::string ignored;
        device->Shutdown();
        if (error != nullptr) {
            *error = stageError;
        }
        return false;
    }

    auto consumer = std::make_unique<EngineQ2DeviceConsumer>(*device);
    if (!consumer->Initialize(width, height, pipelines, &stageError)) {
        std::string ignored;
        consumer->Shutdown();
        device->Shutdown();
        if (error != nullptr) {
            *error = stageError;
        }
        return false;
    }

    device_ = std::move(device);
    consumer_ = std::move(consumer);
    pipelines_ = pipelines;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

void EngineQ2DeviceRuntime::Shutdown() {
    if (consumer_ != nullptr) {
        // The texture cache destroys textures through the Device, so the
        // consumer must go first.
        consumer_->Shutdown();
        consumer_.reset();
    }
    if (device_ != nullptr) {
        device_->Shutdown();
        device_.reset();
    }
    pipelines_ = EngineQ2DevicePipelines();
}

}  // namespace rhi
}  // namespace doom3
