// Q7a Emscripten encoder leg.
//
// Executes the same conformance checks as tests/webgpu/conformance.html, but from C++
// compiled to WebAssembly through the emdawnwebgpu port, so the portable header
// (rhi/rhi.h) is compiled for wasm32 and the operations are driven through Dawn's
// webgpu.h rather than through hand-written JavaScript.
//
// Results are published as window.__conformanceResult, the same shape the runner reads
// for the JavaScript page, so tests/webgpu/run-conformance.mjs can drive both.

#include <webgpu/webgpu.h>

#include <emscripten.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "rhi/rhi.h"

namespace {

struct Result {
    std::string id;
    std::string status;
    std::string detail;
};

std::vector<Result> g_results;
WGPUInstance g_instance = nullptr;
WGPUAdapter g_adapter = nullptr;
WGPUDevice g_device = nullptr;
WGPUQueue g_queue = nullptr;

std::string g_vendor;
std::string g_architecture;
std::string g_deviceName;
std::string g_description;
uint32_t g_vendorId = 0;
uint32_t g_deviceId = 0;
uint32_t g_reportedFacts = 0;
const char* g_canvasFormatName = "unknown";
WGPUTextureFormat g_canvasFormat = WGPUTextureFormat_BGRA8Unorm;

int g_deviceErrors = 0;
std::string g_deviceErrorText;

struct LostState {
    bool done = false;
    int reason = 0;
};

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

std::string ToStd(WGPUStringView view) {
    if (!view.data) {
        return "";
    }
    return std::string(view.data, view.length);
}

WGPUStringView View(const char* text) {
    WGPUStringView view;
    view.data = text;
    view.length = std::strlen(text);
    return view;
}

void Record(const char* id, bool ok, const std::string& detail) {
    g_results.push_back(Result{id, ok ? "PASS" : "FAIL", detail});
}

void NotRun(const char* id, const std::string& detail) {
    g_results.push_back(Result{id, "NOT RUN", detail});
}

bool WaitFor(const std::function<bool()>& predicate, int timeoutMs) {
    int waited = 0;
    while (!predicate() && waited < timeoutMs) {
        emscripten_sleep(5);
        waited += 5;
    }
    return predicate();
}

bool BytesEqual(const std::vector<uint8_t>& actual, const std::vector<uint8_t>& expected,
                std::string* why) {
    if (actual.size() != expected.size()) {
        *why = "length " + std::to_string(actual.size()) + " != " +
               std::to_string(expected.size());
        return false;
    }
    for (size_t i = 0; i < actual.size(); ++i) {
        if (actual[i] != expected[i]) {
            *why = "byte " + std::to_string(i) + ": " + std::to_string(actual[i]) +
                   " != " + std::to_string(expected[i]);
            return false;
        }
    }
    *why = "all bytes equal";
    return true;
}

// ---------------------------------------------------------------------------
// Async bridges
// ---------------------------------------------------------------------------

struct AdapterRequest {
    bool done = false;
    WGPURequestAdapterStatus status = WGPURequestAdapterStatus_Error;
    WGPUAdapter adapter = nullptr;
};

void OnAdapterRequested(WGPURequestAdapterStatus status, WGPUAdapter adapter,
                        WGPUStringView, void* userdata1, void*) {
    auto* request = static_cast<AdapterRequest*>(userdata1);
    request->status = status;
    request->adapter = adapter;
    request->done = true;
}

WGPUAdapter RequestAdapter(bool fallback, WGPURequestAdapterStatus* statusOut) {
    AdapterRequest request;
    WGPURequestAdapterOptions options = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    options.forceFallbackAdapter = fallback ? WGPU_TRUE : WGPU_FALSE;
    WGPURequestAdapterCallbackInfo info = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    info.mode = WGPUCallbackMode_AllowSpontaneous;
    info.callback = OnAdapterRequested;
    info.userdata1 = &request;
    wgpuInstanceRequestAdapter(g_instance, &options, info);
    WaitFor([&] { return request.done; }, 15000);
    if (statusOut) {
        *statusOut = request.status;
    }
    return request.done ? request.adapter : nullptr;
}

struct DeviceRequest {
    bool done = false;
    WGPURequestDeviceStatus status = WGPURequestDeviceStatus_Error;
    WGPUDevice device = nullptr;
};

void OnDeviceRequested(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView,
                       void* userdata1, void*) {
    auto* request = static_cast<DeviceRequest*>(userdata1);
    request->status = status;
    request->device = device;
    request->done = true;
}

void OnDeviceLost(WGPUDevice const*, WGPUDeviceLostReason reason, WGPUStringView message,
                  void* userdata1, void*) {
    auto* lost = static_cast<LostState*>(userdata1);
    lost->reason = static_cast<int>(reason);
    lost->done = true;
    if (message.data) {
        std::printf("device lost reason=%d message=%s\n", static_cast<int>(reason),
                    ToStd(message).c_str());
    }
}

void OnUncapturedError(WGPUDevice const*, WGPUErrorType type, WGPUStringView message,
                       void*, void*) {
    ++g_deviceErrors;
    g_deviceErrorText += std::to_string(static_cast<int>(type)) + ":" + ToStd(message) + ";";
}

WGPUDevice RequestDevice(WGPUAdapter adapter, LostState* lost) {
    DeviceRequest request;
    WGPUDeviceDescriptor descriptor = WGPU_DEVICE_DESCRIPTOR_INIT;
    descriptor.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    descriptor.deviceLostCallbackInfo.callback = OnDeviceLost;
    descriptor.deviceLostCallbackInfo.userdata1 = lost;
    descriptor.uncapturedErrorCallbackInfo.callback = OnUncapturedError;
    WGPURequestDeviceCallbackInfo info = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    info.mode = WGPUCallbackMode_AllowSpontaneous;
    info.callback = OnDeviceRequested;
    info.userdata1 = &request;
    wgpuAdapterRequestDevice(adapter, &descriptor, info);
    WaitFor([&] { return request.done; }, 15000);
    return request.done ? request.device : nullptr;
}

struct MapRequest {
    bool done = false;
    WGPUMapAsyncStatus status = WGPUMapAsyncStatus_Error;
};

void OnBufferMapped(WGPUMapAsyncStatus status, WGPUStringView, void* userdata1, void*) {
    auto* request = static_cast<MapRequest*>(userdata1);
    request->status = status;
    request->done = true;
}

bool MapRead(WGPUBuffer buffer, size_t size, std::vector<uint8_t>* out, std::string* why) {
    MapRequest request;
    WGPUBufferMapCallbackInfo info = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    info.mode = WGPUCallbackMode_AllowSpontaneous;
    info.callback = OnBufferMapped;
    info.userdata1 = &request;
    wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, size, info);
    if (!WaitFor([&] { return request.done; }, 8000)) {
        *why = "mapAsync timed out";
        return false;
    }
    if (request.status != WGPUMapAsyncStatus_Success) {
        *why = "mapAsync status " + std::to_string(static_cast<int>(request.status));
        return false;
    }
    // A MAP_READ mapping is const-only in this API: GetMappedRange is for write maps.
    const void* mapped = wgpuBufferGetConstMappedRange(buffer, 0, size);
    if (!mapped) {
        *why = "getMappedRange returned null";
        return false;
    }
    out->assign(static_cast<const uint8_t*>(mapped),
                static_cast<const uint8_t*>(mapped) + size);
    wgpuBufferUnmap(buffer);
    *why = "mapped";
    return true;
}

WGPUBuffer MakeBuffer(uint64_t size, WGPUBufferUsage usage, const char* label) {
    WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    descriptor.label = View(label);
    descriptor.size = size;
    descriptor.usage = usage;
    return wgpuDeviceCreateBuffer(g_device, &descriptor);
}

WGPUTexture MakeTexture(uint32_t width, uint32_t height, WGPUTextureFormat format,
                        WGPUTextureUsage usage, const char* label) {
    WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
    descriptor.label = View(label);
    descriptor.usage = usage;
    descriptor.dimension = WGPUTextureDimension_2D;
    descriptor.size.width = width;
    descriptor.size.height = height;
    descriptor.size.depthOrArrayLayers = 1;
    descriptor.format = format;
    return wgpuDeviceCreateTexture(g_device, &descriptor);
}

WGPUShaderModule MakeShader(const char* code, const char* label) {
    WGPUShaderSourceWGSL source = WGPU_SHADER_SOURCE_WGSL_INIT;
    source.code = View(code);
    WGPUShaderModuleDescriptor descriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    descriptor.label = View(label);
    descriptor.nextInChain = &source.chain;
    return wgpuDeviceCreateShaderModule(g_device, &descriptor);
}

void SubmitAndWait(WGPUCommandEncoder encoder) {
    WGPUCommandBuffer buffer = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(g_queue, 1, &buffer);
    wgpuCommandBufferRelease(buffer);
}

// ---------------------------------------------------------------------------
// Checks
// ---------------------------------------------------------------------------

const uint32_t kElements = 64;
const uint32_t kComputeBytes = kElements * 4;
const uint32_t kPitch = 256;
const uint8_t kC6Rows[4][16] = {
    {10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255},
    {40, 50, 60, 255, 40, 50, 60, 255, 40, 50, 60, 255, 40, 50, 60, 255},
    {70, 80, 90, 255, 70, 80, 90, 255, 70, 80, 90, 255, 70, 80, 90, 255},
    {200, 210, 220, 255, 200, 210, 220, 255, 200, 210, 220, 255, 200, 210, 220, 255},
};

std::vector<uint32_t> ExpectedCompute() {
    std::vector<uint32_t> values(kElements);
    for (uint32_t i = 0; i < kElements; ++i) {
        values[i] = i * 3u + 1u;
    }
    return values;
}

void CheckCompute() {
    const char* wgsl =
        "@group(0) @binding(0) var<storage, read_write> data: array<u32>;\n"
        "@compute @workgroup_size(64) fn main(@builtin(global_invocation_id) gid: vec3u) {\n"
        "  let i = gid.x;\n"
        "  if (i < arrayLength(&data)) { data[i] = i * 3u + 1u; }\n"
        "}\n";
    WGPUShaderModule module = MakeShader(wgsl, "q7a-compute");

    WGPUBindGroupLayoutEntry entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entry.binding = 0;
    entry.visibility = WGPUShaderStage_Compute;
    entry.buffer.type = WGPUBufferBindingType_Storage;
    WGPUBindGroupLayoutDescriptor layoutDescriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    layoutDescriptor.entryCount = 1;
    layoutDescriptor.entries = &entry;
    WGPUBindGroupLayout layout = wgpuDeviceCreateBindGroupLayout(g_device, &layoutDescriptor);

    WGPUPipelineLayoutDescriptor pipelineLayoutDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pipelineLayoutDescriptor.bindGroupLayoutCount = 1;
    pipelineLayoutDescriptor.bindGroupLayouts = &layout;
    WGPUPipelineLayout pipelineLayout =
        wgpuDeviceCreatePipelineLayout(g_device, &pipelineLayoutDescriptor);

    WGPUComputePipelineDescriptor pipelineDescriptor = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
    pipelineDescriptor.layout = pipelineLayout;
    pipelineDescriptor.compute.module = module;
    pipelineDescriptor.compute.entryPoint = View("main");
    WGPUComputePipeline pipeline =
        wgpuDeviceCreateComputePipeline(g_device, &pipelineDescriptor);

    WGPUBuffer storage = MakeBuffer(
        kComputeBytes, WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst,
        "q7a-compute-storage");
    WGPUBuffer readback = MakeBuffer(kComputeBytes,
                                     WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
                                     "q7a-compute-readback");

    WGPUBindGroupEntry bindEntry = WGPU_BIND_GROUP_ENTRY_INIT;
    bindEntry.binding = 0;
    bindEntry.buffer = storage;
    bindEntry.offset = 0;
    bindEntry.size = WGPU_WHOLE_SIZE;
    WGPUBindGroupDescriptor bindDescriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bindDescriptor.layout = layout;
    bindDescriptor.entryCount = 1;
    bindDescriptor.entries = &bindEntry;
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(g_device, &bindDescriptor);

    auto dispatchOnce = [&](std::vector<uint32_t>* values, std::string* why) {
        WGPUCommandEncoderDescriptor encoderDescriptor = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_device, &encoderDescriptor);
        WGPUComputePassDescriptor passDescriptor = WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
        WGPUComputePassEncoder pass = wgpuCommandEncoderBeginComputePass(encoder, &passDescriptor);
        wgpuComputePassEncoderSetPipeline(pass, pipeline);
        wgpuComputePassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);
        wgpuComputePassEncoderDispatchWorkgroups(pass, 1, 1, 1);
        wgpuComputePassEncoderEnd(pass);
        wgpuCommandEncoderCopyBufferToBuffer(encoder, storage, 0, readback, 0, kComputeBytes);
        SubmitAndWait(encoder);
        wgpuCommandEncoderRelease(encoder);

        std::vector<uint8_t> bytes;
        if (!MapRead(readback, kComputeBytes, &bytes, why)) {
            return false;
        }
        values->resize(kElements);
        std::memcpy(values->data(), bytes.data(), kComputeBytes);
        return true;
    };

    std::string why;
    std::vector<uint32_t> first;
    std::vector<uint8_t> firstBytes;
    const bool firstOk = dispatchOnce(&first, &why);
    if (!firstOk) {
        Record("C5-compute", false, "compute readback failed: " + why);
        Record("C3-bind-group", false, "compute readback failed: " + why);
    } else {
        const std::vector<uint32_t> expected = ExpectedCompute();
        bool ok = first.size() == expected.size();
        for (size_t i = 0; ok && i < expected.size(); ++i) {
            ok = first[i] == expected[i];
        }
        std::string detail = "dispatch wrote " + std::to_string(kElements) +
                             " u32; expected i*3+1; first=";
        for (size_t i = 0; i < 8 && i < first.size(); ++i) {
            detail += std::to_string(first[i]) + (i + 1 < 8 ? "," : "");
        }
        Record("C5-compute", ok, detail);

        std::vector<uint32_t> second;
        const bool secondOk = dispatchOnce(&second, &why);
        bool reuseOk = secondOk && second.size() == expected.size();
        for (size_t i = 0; reuseOk && i < expected.size(); ++i) {
            reuseOk = second[i] == expected[i] && first[i] == expected[i];
        }
        Record("C3-bind-group", reuseOk,
               "the same bind group drove two dispatches; both readbacks match the literal "
               "expectation");
    }

    wgpuBindGroupRelease(bindGroup);
    wgpuComputePipelineRelease(pipeline);
    wgpuPipelineLayoutRelease(pipelineLayout);
    wgpuBindGroupLayoutRelease(layout);
    wgpuShaderModuleRelease(module);
    wgpuBufferDestroy(storage);
    wgpuBufferDestroy(readback);
    wgpuBufferRelease(storage);
    wgpuBufferRelease(readback);
}

void CheckMrt() {
    const char* wgsl =
        "struct VOut { @builtin(position) pos: vec4f };\n"
        "struct FOut { @location(0) a: vec4f, @location(1) b: vec4f };\n"
        "@vertex fn vs(@builtin(vertex_index) i: u32) -> VOut {\n"
        "  var p = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));\n"
        "  var o: VOut; o.pos = vec4f(p[i], 0.0, 1.0); return o;\n"
        "}\n"
        "@fragment fn fs() -> FOut {\n"
        "  var o: FOut; o.a = vec4f(0.25, 0.5, 0.75, 1.0); o.b = vec4f(1.0, 0.0, 0.5, 0.25);\n"
        "  return o;\n"
        "}\n";
    WGPUShaderModule module = MakeShader(wgsl, "q7a-mrt");

    WGPUTexture textureA = MakeTexture(4, 4, WGPUTextureFormat_RGBA8Unorm,
                                       WGPUTextureUsage_RenderAttachment |
                                           WGPUTextureUsage_CopySrc,
                                       "q7a-mrt-a");
    WGPUTexture textureB = MakeTexture(4, 4, WGPUTextureFormat_RGBA8Unorm,
                                       WGPUTextureUsage_RenderAttachment |
                                           WGPUTextureUsage_CopySrc,
                                       "q7a-mrt-b");

    WGPUColorTargetState targets[2] = {WGPU_COLOR_TARGET_STATE_INIT,
                                       WGPU_COLOR_TARGET_STATE_INIT};
    targets[0].format = WGPUTextureFormat_RGBA8Unorm;
    targets[1].format = WGPUTextureFormat_RGBA8Unorm;
    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
    fragment.module = module;
    fragment.entryPoint = View("fs");
    fragment.targetCount = 2;
    fragment.targets = targets;

    WGPURenderPipelineDescriptor pipelineDescriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    pipelineDescriptor.vertex.module = module;
    pipelineDescriptor.vertex.entryPoint = View("vs");
    pipelineDescriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDescriptor.fragment = &fragment;
    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(g_device, &pipelineDescriptor);

    WGPURenderPassColorAttachment attachments[2] = {
        WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT, WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT};
    attachments[0].view = wgpuTextureCreateView(textureA, nullptr);
    attachments[0].loadOp = WGPULoadOp_Clear;
    attachments[0].storeOp = WGPUStoreOp_Store;
    attachments[0].clearValue = WGPUColor{0.0, 0.0, 0.0, 0.0};
    attachments[1].view = wgpuTextureCreateView(textureB, nullptr);
    attachments[1].loadOp = WGPULoadOp_Clear;
    attachments[1].storeOp = WGPUStoreOp_Store;
    attachments[1].clearValue = WGPUColor{0.0, 0.0, 0.0, 0.0};

    WGPUCommandEncoderDescriptor encoderDescriptor = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_device, &encoderDescriptor);
    WGPURenderPassDescriptor passDescriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    passDescriptor.colorAttachmentCount = 2;
    passDescriptor.colorAttachments = attachments;
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDescriptor);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);

    WGPUBuffer readA = MakeBuffer(kPitch * 4, WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
                                  "q7a-mrt-read-a");
    WGPUBuffer readB = MakeBuffer(kPitch * 4, WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
                                  "q7a-mrt-read-b");
    WGPUTexelCopyTextureInfo fromA = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    fromA.texture = textureA;
    WGPUTexelCopyTextureInfo fromB = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    fromB.texture = textureB;
    WGPUTexelCopyBufferInfo toA = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    toA.buffer = readA;
    toA.layout.bytesPerRow = kPitch;
    toA.layout.rowsPerImage = 4;
    WGPUTexelCopyBufferInfo toB = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    toB.buffer = readB;
    toB.layout.bytesPerRow = kPitch;
    toB.layout.rowsPerImage = 4;
    WGPUExtent3D extent = WGPUExtent3D{4, 4, 1};
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &fromA, &toA, &extent);
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &fromB, &toB, &extent);
    SubmitAndWait(encoder);

    std::string whyA, whyB;
    std::vector<uint8_t> bytesA, bytesB;
    const bool okA =
        MapRead(readA, kPitch * 4, &bytesA, &whyA) &&
        BytesEqual(std::vector<uint8_t>(bytesA.begin(), bytesA.begin() + 4),
                   std::vector<uint8_t>{64, 128, 191, 255}, &whyA);
    const bool okB =
        MapRead(readB, kPitch * 4, &bytesB, &whyB) &&
        BytesEqual(std::vector<uint8_t>(bytesB.begin(), bytesB.begin() + 4),
                   std::vector<uint8_t>{255, 0, 128, 64}, &whyB);

    std::string detail = "two attachments in one pass; A=";
    if (bytesA.size() >= 4) {
        detail += std::to_string(bytesA[0]) + "," + std::to_string(bytesA[1]) + "," +
                  std::to_string(bytesA[2]) + "," + std::to_string(bytesA[3]);
    } else {
        detail += "missing";
    }
    detail += " B=";
    if (bytesB.size() >= 4) {
        detail += std::to_string(bytesB[0]) + "," + std::to_string(bytesB[1]) + "," +
                  std::to_string(bytesB[2]) + "," + std::to_string(bytesB[3]);
    } else {
        detail += "missing";
    }
    detail += " (" + whyA + " / " + whyB + ")";
    Record("C4-mrt", okA && okB, detail);

    wgpuCommandEncoderRelease(encoder);
    wgpuRenderPipelineRelease(pipeline);
    wgpuShaderModuleRelease(module);
    wgpuTextureViewRelease(attachments[0].view);
    wgpuTextureViewRelease(attachments[1].view);
    wgpuTextureDestroy(textureA);
    wgpuTextureDestroy(textureB);
    wgpuTextureRelease(textureA);
    wgpuTextureRelease(textureB);
    wgpuBufferDestroy(readA);
    wgpuBufferDestroy(readB);
    wgpuBufferRelease(readA);
    wgpuBufferRelease(readB);
}

void CheckPitch() {
    WGPUTexture texture = MakeTexture(4, 4, WGPUTextureFormat_RGBA8Unorm,
                                      WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc,
                                      "q7a-pitch");
    uint8_t packed[64];
    std::memcpy(packed, kC6Rows, 64);
    WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    destination.texture = texture;
    WGPUTexelCopyBufferLayout layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
    layout.bytesPerRow = 16;
    layout.rowsPerImage = 4;
    WGPUExtent3D writeSize = WGPUExtent3D{4, 4, 1};
    wgpuQueueWriteTexture(g_queue, &destination, packed, sizeof(packed), &layout, &writeSize);

    WGPUBufferDescriptor stagingDescriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    stagingDescriptor.label = View("q7a-pitch-staging");
    stagingDescriptor.size = kPitch * 4;
    stagingDescriptor.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    stagingDescriptor.mappedAtCreation = WGPU_TRUE;
    WGPUBuffer staging = wgpuDeviceCreateBuffer(g_device, &stagingDescriptor);
    void* mappedAtCreation = wgpuBufferGetMappedRange(staging, 0, kPitch * 4);
    if (mappedAtCreation) {
        std::memset(mappedAtCreation, 0xAA, kPitch * 4);
    }
    wgpuBufferUnmap(staging);

    WGPUCommandEncoderDescriptor encoderDescriptor = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_device, &encoderDescriptor);
    WGPUTexelCopyTextureInfo source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    source.texture = texture;
    WGPUTexelCopyBufferInfo target = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    target.buffer = staging;
    target.layout.bytesPerRow = kPitch;
    target.layout.rowsPerImage = 4;
    WGPUExtent3D extent = WGPUExtent3D{4, 4, 1};
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &source, &target, &extent);
    SubmitAndWait(encoder);

    std::string why;
    std::vector<uint8_t> bytes;
    const bool mapped = MapRead(staging, kPitch * 4, &bytes, &why);
    bool rowsOk = mapped;
    bool paddingOk = mapped;
    for (uint32_t row = 0; row < 4 && rowsOk; ++row) {
        rowsOk = BytesEqual(std::vector<uint8_t>(bytes.begin() + row * kPitch,
                                                 bytes.begin() + row * kPitch + 16),
                            std::vector<uint8_t>(kC6Rows[row], kC6Rows[row] + 16), &why);
    }
    for (uint32_t row = 0; row < 4 && paddingOk; ++row) {
        for (uint32_t i = 16; i < kPitch; ++i) {
            if (bytes[row * kPitch + i] != 0xAA) {
                paddingOk = false;
                break;
            }
        }
    }
    std::string packedWhy;
    const bool packedWrong =
        mapped && !BytesEqual(std::vector<uint8_t>(bytes.begin() + 16, bytes.begin() + 32),
                              std::vector<uint8_t>(kC6Rows[1], kC6Rows[1] + 16), &packedWhy);
    std::string detail = "rows=" + std::string(rowsOk ? "true" : "false") +
                         " padding_untouched=" + (paddingOk ? "true" : "false") +
                         " packed_offset_rejected=" + (packedWrong ? "true" : "false");
    if (!rowsOk) {
        detail += " why=" + why;
    }
    Record("C6-pitch256", rowsOk && paddingOk && packedWrong, detail);
    Record("N2-negative-pitch", packedWrong,
           "reading row 1 at the packed offset 16 does not equal the true row 1 (only offset "
           "256 does)");

    wgpuCommandEncoderRelease(encoder);
    wgpuTextureDestroy(texture);
    wgpuTextureRelease(texture);
    wgpuBufferDestroy(staging);
    wgpuBufferRelease(staging);
}

void CheckSurface() {
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector source =
        WGPU_EMSCRIPTEN_SURFACE_SOURCE_CANVAS_HTML_SELECTOR_INIT;
    source.selector = View("#surface");
    WGPUSurfaceDescriptor descriptor = WGPU_SURFACE_DESCRIPTOR_INIT;
    descriptor.nextInChain = &source.chain;
    WGPUSurface surface = wgpuInstanceCreateSurface(g_instance, &descriptor);
    if (!surface) {
        Record("C7-surface", false, "wgpuInstanceCreateSurface returned null");
        return;
    }
    WGPUSurfaceCapabilities capabilities = WGPU_SURFACE_CAPABILITIES_INIT;
    const WGPUStatus status = wgpuSurfaceGetCapabilities(surface, g_adapter, &capabilities);
    if (status != WGPUStatus_Success || capabilities.formatCount == 0) {
        Record("C7-surface", false,
               "surface capabilities failed or exposed no format (status " +
                   std::to_string(static_cast<int>(status)) + ")");
        wgpuSurfaceRelease(surface);
        return;
    }
    g_canvasFormat = capabilities.formats[0];
    g_canvasFormatName = g_canvasFormat == WGPUTextureFormat_BGRA8Unorm ? "bgra8unorm"
                                                                        : "rgba8unorm";

    WGPUSurfaceConfiguration configuration = WGPU_SURFACE_CONFIGURATION_INIT;
    configuration.device = g_device;
    configuration.format = g_canvasFormat;
    configuration.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    configuration.width = 64;
    configuration.height = 64;
    configuration.alphaMode =
        capabilities.alphaModeCount > 0 ? capabilities.alphaModes[0] : WGPUCompositeAlphaMode_Auto;
    configuration.presentMode = capabilities.presentModeCount > 0 ? capabilities.presentModes[0]
                                                                  : WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(surface, &configuration);

    WGPUSurfaceTexture surfaceTexture = WGPU_SURFACE_TEXTURE_INIT;
    wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
    const bool acquired = surfaceTexture.status == WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal ||
                          surfaceTexture.status == WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal;
    if (!acquired || !surfaceTexture.texture) {
        Record("C7-surface", false,
               "getCurrentTexture status " +
                   std::to_string(static_cast<int>(surfaceTexture.status)));
        wgpuSurfaceRelease(surface);
        return;
    }

    WGPURenderPassColorAttachment attachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    attachment.view = wgpuTextureCreateView(surfaceTexture.texture, nullptr);
    attachment.loadOp = WGPULoadOp_Clear;
    attachment.storeOp = WGPUStoreOp_Store;
    attachment.clearValue = WGPUColor{1.0, 0.5, 0.0, 1.0};

    WGPUCommandEncoderDescriptor encoderDescriptor = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_device, &encoderDescriptor);
    WGPURenderPassDescriptor passDescriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    passDescriptor.colorAttachmentCount = 1;
    passDescriptor.colorAttachments = &attachment;
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDescriptor);
    wgpuRenderPassEncoderEnd(pass);

    WGPUBuffer readback = MakeBuffer(kPitch * 64, WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
                                     "q7a-surface-readback");
    WGPUTexelCopyTextureInfo from = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    from.texture = surfaceTexture.texture;
    WGPUTexelCopyBufferInfo to = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    to.buffer = readback;
    to.layout.bytesPerRow = kPitch;
    to.layout.rowsPerImage = 64;
    WGPUExtent3D extent = WGPUExtent3D{64, 64, 1};
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &from, &to, &extent);
    SubmitAndWait(encoder);

    std::string why;
    std::vector<uint8_t> bytes;
    const bool mapped = MapRead(readback, kPitch * 64, &bytes, &why);
    const std::vector<uint8_t> expected =
        g_canvasFormat == WGPUTextureFormat_BGRA8Unorm
            ? std::vector<uint8_t>{0, 128, 255, 255}
            : std::vector<uint8_t>{255, 128, 0, 255};
    bool pixelOk = false;
    if (mapped && bytes.size() >= 4) {
        pixelOk = BytesEqual(std::vector<uint8_t>(bytes.begin(), bytes.begin() + 4), expected, &why);
    }
    std::string detail = "format=" + std::string(g_canvasFormatName) +
                         "; configured, acquired 64x64, cleared and committed; pixel0=";
    if (mapped && bytes.size() >= 4) {
        detail += std::to_string(bytes[0]) + "," + std::to_string(bytes[1]) + "," +
                  std::to_string(bytes[2]) + "," + std::to_string(bytes[3]);
    } else {
        detail += "missing (" + why + ")";
    }
    Record("C7-surface", pixelOk, detail);

    wgpuCommandEncoderRelease(encoder);
    wgpuTextureViewRelease(attachment.view);
    wgpuTextureRelease(surfaceTexture.texture);
    wgpuBufferDestroy(readback);
    wgpuBufferRelease(readback);
    wgpuSurfaceRelease(surface);
}

void CheckSoftwareAdapter() {
    WGPURequestAdapterStatus status = WGPURequestAdapterStatus_Error;
    WGPUAdapter fallback = RequestAdapter(true, &status);
    if (!fallback) {
        NotRun("A1-software-adapter",
               "requestAdapter({forceFallbackAdapter:true}) returned no adapter (status " +
                   std::to_string(static_cast<int>(status)) + "); the software leg is not a pass");
        return;
    }
    LostState lost;
    WGPUDevice device = RequestDevice(fallback, &lost);
    if (!device) {
        Record("A1-software-adapter", false, "the fallback adapter produced no device");
        wgpuAdapterRelease(fallback);
        return;
    }
    WGPUQueue queue = wgpuDeviceGetQueue(device);
    const uint8_t payload[6] = {4, 8, 15, 16, 23, 42};
    WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    descriptor.size = 8;
    descriptor.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &descriptor);
    wgpuQueueWriteBuffer(queue, buffer, 0, payload, sizeof(payload));
    std::string why;
    std::vector<uint8_t> bytes;
    const bool mapped = MapRead(buffer, 8, &bytes, &why);
    bool ok = false;
    if (mapped) {
        ok = BytesEqual(std::vector<uint8_t>(bytes.begin(), bytes.begin() + 6),
                        std::vector<uint8_t>(payload, payload + 6), &why);
    }
    Record("A1-software-adapter", ok,
           "fallback adapter ran a buffer round trip (" + why + ")");
    wgpuBufferDestroy(buffer);
    wgpuBufferRelease(buffer);
    wgpuDeviceDestroy(device);
    wgpuDeviceRelease(device);
    wgpuAdapterRelease(fallback);
}

void CheckLiveAndLost() {
    // N3: a live device must not report loss.
    WGPURequestAdapterStatus status = WGPURequestAdapterStatus_Error;
    WGPUAdapter adapter = RequestAdapter(false, &status);
    if (!adapter) {
        NotRun("N3-negative-liveness", "a second adapter request failed");
        return;
    }
    LostState lost;
    WGPUDevice device = RequestDevice(adapter, &lost);
    if (!device) {
        NotRun("N3-negative-liveness", "a second device request failed");
        wgpuAdapterRelease(adapter);
        return;
    }
    int waited = 0;
    while (!lost.done && waited < 200) {
        emscripten_sleep(5);
        waited += 5;
    }
    Record("N3-negative-liveness", !lost.done,
           "a live device's lost callback stayed silent for 200 ms, so C8 is not vacuous");
    wgpuDeviceDestroy(device);
    WaitFor([&] { return lost.done; }, 5000);
    wgpuDeviceRelease(device);
    wgpuAdapterRelease(adapter);
}

void CheckMainDeviceLoss(LostState* lost) {
    wgpuDeviceDestroy(g_device);
    const bool fired = WaitFor([&] { return lost->done; }, 5000);
    const int destroyed = static_cast<int>(WGPUDeviceLostReason_Destroyed);
    Record("C8-device-loss", fired && lost->reason == destroyed,
           "device.destroy() reported loss reason " + std::to_string(lost->reason) +
               " (destroyed=" + std::to_string(destroyed) + ")");
}

void CheckComparatorControl() {
    const std::vector<uint32_t> expected = ExpectedCompute();
    std::vector<uint32_t> mutated = expected;
    mutated[5] = mutated[5] + 1;
    std::string why;
    const bool equal =
        BytesEqual(std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(expected.data()),
                                        reinterpret_cast<const uint8_t*>(expected.data()) +
                                            expected.size() * 4),
                   std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(mutated.data()),
                                        reinterpret_cast<const uint8_t*>(mutated.data()) +
                                            mutated.size() * 4),
                   &why);
    Record("N1-negative-comparator", !equal,
           "a one-unit mutation of the expected compute result is rejected: " + why);
}

// ---------------------------------------------------------------------------
// Reporting
// ---------------------------------------------------------------------------

std::string Json(const std::string& value) {
    std::string out;
    for (char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                    out += buffer;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::string BuildJson(const char* nonce, const char* userAgent, const char* origin,
                     bool secure) {
    int pass = 0;
    int fail = 0;
    int notRun = 0;
    for (const Result& result : g_results) {
        if (result.status == "PASS") {
            ++pass;
        } else if (result.status == "FAIL") {
            ++fail;
        } else {
            ++notRun;
        }
    }

    std::string json = "{\"env\":{";
    json += "\"runNonce\":\"" + Json(nonce) + "\",";
    json += "\"userAgent\":\"" + Json(userAgent) + "\",\"secureContext\":" +
            (secure ? "true" : "false") + ",\"origin\":\"" + Json(origin) +
            "\",\"hasGpu\":true,";
    json += "\"adapter\":{\"vendor\":\"" + Json(g_vendor) + "\",\"architecture\":\"" +
            Json(g_architecture) + "\",\"device\":\"" + Json(g_deviceName) +
            "\",\"description\":\"" + Json(g_description) + "\"},";
    json += "\"preferredCanvasFormat\":\"" + std::string(g_canvasFormatName) + "\"";
    json += "},\"summary\":{\"total\":" + std::to_string(g_results.size()) +
            ",\"pass\":" + std::to_string(pass) + ",\"fail\":" + std::to_string(fail) +
            ",\"notRun\":" + std::to_string(notRun) + "},\"checks\":[";
    for (size_t i = 0; i < g_results.size(); ++i) {
        if (i) {
            json += ",";
        }
        json += "{\"id\":\"" + Json(g_results[i].id) + "\",\"status\":\"" +
                Json(g_results[i].status) + "\",\"detail\":\"" + Json(g_results[i].detail) +
                "\",\"observed\":null}";
    }
    json += "]}";
    return json;
}

}  // namespace

int main() {
    static_assert(sizeof(void*) == 4, "this harness must be built for wasm32");
    static char nonce[128];
    static char userAgent[512];
    static char origin[256];
    EM_ASM({
        var u = new URLSearchParams(location.search);
        stringToUTF8(u.get("run") || "", $0, 128);
        stringToUTF8(navigator.userAgent, $1, 512);
        stringToUTF8(location.origin, $2, 256);
    }, nonce, userAgent, origin);

    g_instance = wgpuCreateInstance(nullptr);
    if (!g_instance) {
        Record("C1-device", false, "wgpuCreateInstance returned null");
        for (const char* id : {"C2-buffer", "C3-bind-group", "C4-mrt", "C5-compute",
                               "C6-pitch256", "C7-surface", "C8-device-loss",
                               "N1-negative-comparator", "N2-negative-pitch",
                               "N3-negative-liveness", "A1-software-adapter", "E-uncaptured"}) {
            NotRun(id, "no WebGPU instance");
        }
    } else {
        WGPURequestAdapterStatus adapterStatus = WGPURequestAdapterStatus_Error;
        g_adapter = RequestAdapter(false, &adapterStatus);
        if (!g_adapter) {
            Record("C1-device", false,
                   "requestAdapter returned null (status " +
                       std::to_string(static_cast<int>(adapterStatus)) + ")");
            for (const char* id : {"C2-buffer", "C3-bind-group", "C4-mrt", "C5-compute",
                                   "C6-pitch256", "C7-surface", "C8-device-loss",
                                   "N1-negative-comparator", "N2-negative-pitch",
                                   "N3-negative-liveness", "A1-software-adapter",
                                   "E-uncaptured"}) {
                NotRun(id, "no adapter");
            }
        } else {
            WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;
            wgpuAdapterGetInfo(g_adapter, &info);
            g_vendor = ToStd(info.vendor);
            g_architecture = ToStd(info.architecture);
            g_deviceName = ToStd(info.device);
            g_description = ToStd(info.description);
            g_vendorId = info.vendorID;
            g_deviceId = info.deviceID;

            // The T089 availability rule, exercised for real: a browser adapter may
            // expose numeric ids (Dawn does) or none at all, and dedicated memory has
            // no WebGPU source at all.
            g_reportedFacts = 0;
            if (g_vendorId != 0) {
                g_reportedFacts |= static_cast<uint32_t>(doom3::rhi::AdapterFact::VendorId);
            }
            if (g_deviceId != 0) {
                g_reportedFacts |= static_cast<uint32_t>(doom3::rhi::AdapterFact::DeviceId);
            }

            static LostState mainLost;
            g_device = RequestDevice(g_adapter, &mainLost);
            if (!g_device) {
                Record("C1-device", false, "requestDevice returned null");
            } else {
                g_queue = wgpuDeviceGetQueue(g_device);
                const bool factsConsistent =
                    ((g_reportedFacts & static_cast<uint32_t>(doom3::rhi::AdapterFact::VendorId)) ==
                         0 ||
                     g_vendorId != 0) &&
                    ((g_reportedFacts & static_cast<uint32_t>(doom3::rhi::AdapterFact::DeviceId)) ==
                         0 ||
                     g_deviceId != 0);
                std::string detail = "adapter '" + g_vendor + "'/'" + g_architecture +
                                     "' created a device; reportedFacts=0x";
                char hex[16];
                std::snprintf(hex, sizeof(hex), "%x", g_reportedFacts);
                detail += hex;
                detail += " vendorId=" + std::to_string(g_vendorId) +
                          " deviceId=" + std::to_string(g_deviceId) +
                          " facts_consistent=" + (factsConsistent ? "true" : "false");
                Record("C1-device", factsConsistent, detail);

                // C2: buffer round trip.
                {
                    const uint8_t payload[16] = {1, 2, 3, 5, 8, 13, 21, 34,
                                                 55, 89, 144, 233, 0, 127, 128, 255};
                    WGPUBuffer buffer =
                        MakeBuffer(16, WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
                                   "q7a-buffer");
                    wgpuQueueWriteBuffer(g_queue, buffer, 0, payload, sizeof(payload));
                    std::string why;
                    std::vector<uint8_t> bytes;
                    const bool mapped = MapRead(buffer, 16, &bytes, &why);
                    bool ok = false;
                    if (mapped) {
                        ok = BytesEqual(bytes, std::vector<uint8_t>(payload, payload + 16), &why);
                    }
                    Record("C2-buffer", ok, "buffer write + mapAsync round trip: " + why);
                    wgpuBufferDestroy(buffer);
                    wgpuBufferRelease(buffer);
                }

                CheckCompute();
                CheckMrt();
                CheckPitch();
                CheckSurface();
                CheckSoftwareAdapter();
                CheckComparatorControl();
                CheckLiveAndLost();

                if (g_deviceErrors == 0) {
                    Record("E-uncaptured", true,
                           "the device raised no uncaptured error during the whole run");
                } else {
                    Record("E-uncaptured", false,
                           "uncaptured errors: " + std::to_string(g_deviceErrors) + " " +
                               g_deviceErrorText);
                }

                CheckMainDeviceLoss(&mainLost);
            }
            wgpuAdapterRelease(g_adapter);
        }
        wgpuInstanceRelease(g_instance);
    }

    const bool secure = EM_ASM_INT({ return isSecureContext ? 1 : 0; }) != 0;
    const std::string json = BuildJson(nonce, userAgent, origin, secure);
    EM_ASM({ window.q7aPublish(UTF8ToString($0)); }, json.c_str());
    std::printf("q7a emscripten harness finished\n");
    return 0;
}
