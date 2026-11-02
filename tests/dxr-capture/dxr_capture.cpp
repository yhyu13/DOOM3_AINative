// Q6 DXR path-traced capture (spec 004 T069, issue #68).
//
// Renders the capture fixtures in capture_scene.cpp with a real DXR pipeline,
// writes PNGs plus SHA-256 hashes, and records convergence, noise, timing and
// analytic checks. It is a reference renderer, not the engine and not a Q6
// engine frame.
//
// Exit codes:
//   0  every fixture executed and its declared checks passed
//   1  an unexpected HRESULT, a device removal or a write failure
//   2  DXR is unavailable on every adapter (no dispatch attempted)
//   3  the run executed but at least one fixture failed its declared checks
//
// A failed check is never rewritten into a pass: the verdict string and the
// exit code are derived from the same recorded values.

#define NOMINMAX
#include <windows.h>

#include <bcrypt.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "capture_scene.h"
#include "image_io.h"
#include "rhi/pathtrace_denoise.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {

constexpr uint32_t kMaterialStride = 80;
constexpr uint32_t kInstanceStride = 16;
constexpr uint32_t kMaxDispatchSamples = 16;
constexpr UINT64 kFenceTimeoutMs = 120000;
constexpr uint32_t kSeed = 0x51A7E001u;

struct Config {
    std::string outDir = ".";
    std::string adapterName;
    uint32_t width = 320;
    uint32_t height = 180;
    uint32_t sppCap = 0;
    bool skipTiming = false;
};

struct GpuMaterial {
    float albedoRoughness[4];
    float tintIor[4];
    float emissionKind[4];
    float alphaUv[4];
    float uvOriginScale[4];
};
static_assert(sizeof(GpuMaterial) == kMaterialStride, "material layout mismatch");

struct GpuInstance {
    uint32_t materialIndex;
    uint32_t positionBase;
    uint32_t pad0;
    uint32_t pad1;
};
static_assert(sizeof(GpuInstance) == kInstanceStride, "instance layout mismatch");

struct FrameConstants {
    float camPos[4];
    float camFwd[4];
    float camRight[4];
    float camUp[4];
    float extent[4];
    float envUpper[4];
    float envLower[4];
    float lightPos[4];
    float lightColor[4];
    uint32_t counts[4];  // sampleIndex, maxDepth, width, height
    uint32_t seed[4];    // base, frame, unused, unused
    uint32_t misc[4];    // samplesThisDispatch, unused, unused, unused
};

std::string HrText(HRESULT hr) {
    char buffer[32] = {};
    sprintf_s(buffer, "0x%08X", static_cast<unsigned int>(hr));
    return std::string(buffer);
}

bool Check(HRESULT hr, const char* what) {
    if (FAILED(hr)) {
        std::printf("ERROR %s failed: %s\n", what, HrText(hr).c_str());
        return false;
    }
    return true;
}

std::string Narrow(const wchar_t* wide) {
    if (wide == nullptr) {
        return std::string();
    }
    const int needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return std::string();
    }
    std::string out(static_cast<size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, &out[0], needed, nullptr, nullptr);
    return out;
}

const char* TierName(D3D12_RAYTRACING_TIER tier) {
    switch (tier) {
        case D3D12_RAYTRACING_TIER_NOT_SUPPORTED: return "NOT_SUPPORTED";
        case D3D12_RAYTRACING_TIER_1_0: return "1_0";
        case D3D12_RAYTRACING_TIER_1_1: return "1_1";
        default: return "UNKNOWN";
    }
}

const char* ShaderModelName(D3D_SHADER_MODEL model) {
    switch (model) {
        case D3D_SHADER_MODEL_6_0: return "6.0";
        case D3D_SHADER_MODEL_6_1: return "6.1";
        case D3D_SHADER_MODEL_6_2: return "6.2";
        case D3D_SHADER_MODEL_6_3: return "6.3";
        case D3D_SHADER_MODEL_6_4: return "6.4";
        case D3D_SHADER_MODEL_6_5: return "6.5";
        case D3D_SHADER_MODEL_6_6: return "6.6";
        case D3D_SHADER_MODEL_6_7: return "6.7";
        case D3D_SHADER_MODEL_6_8: return "6.8";
        default: return "unknown";
    }
}

std::string ExeDirectory() {
    char path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string full(path, length);
    const size_t slash = full.find_last_of("\\/");
    return slash == std::string::npos ? std::string(".") : full.substr(0, slash);
}

bool ReadFileBytes(const std::string& path, std::vector<uint8_t>* bytes) {
    FILE* file = nullptr;
    if (fopen_s(&file, path.c_str(), "rb") != 0 || file == nullptr) {
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    const long length = ftell(file);
    if (length <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    bytes->resize(static_cast<size_t>(length));
    const size_t read = fread(bytes->data(), 1, bytes->size(), file);
    fclose(file);
    return read == bytes->size();
}

std::string Sha256Hex(const void* data, size_t size) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectSize = 0;
    DWORD digestSize = 0;
    DWORD written = 0;
    BYTE object[1024] = {};
    BYTE digest[32] = {};
    std::string hex;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
        return "unavailable";
    }
    BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize),
                      sizeof(objectSize), &written, 0);
    BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&digestSize),
                      sizeof(digestSize), &written, 0);
    if (BCryptCreateHash(algorithm, &hash, object, sizeof(object), nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return "unavailable";
    }
    BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<void*>(data)),
                   static_cast<ULONG>(std::min<size_t>(size, 0xFFFFFFFFu)), 0);
    BCryptFinishHash(hash, digest, sizeof(digest), 0);
    char buffer[3] = {};
    for (BYTE byte : digest) {
        sprintf_s(buffer, "%02x", byte);
        hex += buffer;
    }
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return hex;
}

ComPtr<ID3D12Resource> CreateBuffer(ID3D12Device* device, UINT64 size, D3D12_RESOURCE_FLAGS flags,
                                    D3D12_RESOURCE_STATES initialState,
                                    D3D12_HEAP_TYPE heapType) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = heapType;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = flags;
    ComPtr<ID3D12Resource> resource;
    const HRESULT hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                                       initialState, nullptr,
                                                       IID_PPV_ARGS(&resource));
    if (FAILED(hr)) {
        std::printf("ERROR CreateCommittedResource(%llu bytes) failed: %s\n",
                    static_cast<unsigned long long>(size), HrText(hr).c_str());
        return nullptr;
    }
    return resource;
}

ComPtr<ID3D12Resource> CreateTexture2D(ID3D12Device* device, uint32_t width, uint32_t height,
                                       DXGI_FORMAT format) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    ComPtr<ID3D12Resource> resource;
    const HRESULT hr = device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
        IID_PPV_ARGS(&resource));
    if (FAILED(hr)) {
        std::printf("ERROR CreateCommittedResource(texture) failed: %s\n", HrText(hr).c_str());
        return nullptr;
    }
    return resource;
}

UINT Align(UINT value, UINT alignment) { return (value + alignment - 1) / alignment * alignment; }

struct FloatImage {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<float> rgb;
    std::vector<float> depth;
};

float Luma(const float* pixel) {
    return 0.2126f * pixel[0] + 0.7152f * pixel[1] + 0.0722f * pixel[2];
}

uint64_t CountNan(const FloatImage& image) {
    uint64_t count = 0;
    for (size_t i = 0; i < image.rgb.size(); i += 3) {
        if (!std::isfinite(image.rgb[i]) || !std::isfinite(image.rgb[i + 1]) ||
            !std::isfinite(image.rgb[i + 2])) {
            ++count;
        }
    }
    return count;
}

double MeanAbsDiff(const FloatImage& a, const FloatImage& b) {
    if (a.rgb.size() != b.rgb.size() || a.rgb.empty()) {
        return -1.0;
    }
    double sum = 0.0;
    for (size_t i = 0; i < a.rgb.size(); ++i) {
        sum += std::fabs(static_cast<double>(a.rgb[i]) - static_cast<double>(b.rgb[i]));
    }
    return sum / static_cast<double>(a.rgb.size());
}

struct PixelRect {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    bool valid = false;
};

double MeanAbsDiffRect(const FloatImage& a, const FloatImage& b, const PixelRect& rect) {
    if (!rect.valid || a.rgb.size() != b.rgb.size()) {
        return -1.0;
    }
    double sum = 0.0;
    uint64_t count = 0;
    for (int y = rect.y0; y <= rect.y1; ++y) {
        for (int x = rect.x0; x <= rect.x1; ++x) {
            const size_t index = (static_cast<size_t>(y) * a.width + x) * 3;
            for (int c = 0; c < 3; ++c) {
                sum += std::fabs(static_cast<double>(a.rgb[index + c]) -
                                 static_cast<double>(b.rgb[index + c]));
                ++count;
            }
        }
    }
    return count == 0 ? -1.0 : sum / static_cast<double>(count);
}

PixelRect WholeImage(const FloatImage& image) {
    PixelRect rect;
    rect.x0 = 0;
    rect.y0 = 0;
    rect.x1 = static_cast<int>(image.width) - 1;
    rect.y1 = static_cast<int>(image.height) - 1;
    rect.valid = image.width > 0 && image.height > 0;
    return rect;
}

PixelRect ProjectRect(const q6cap::Camera& camera, const q6cap::Vec3& center,
                      const q6cap::Vec3& u, const q6cap::Vec3& v, float halfU, float halfV) {
    PixelRect rect;
    const q6cap::Vec3 corners[4] = {
        q6cap::Add(center, q6cap::Add(q6cap::Scale(u, -halfU), q6cap::Scale(v, -halfV))),
        q6cap::Add(center, q6cap::Add(q6cap::Scale(u, halfU), q6cap::Scale(v, -halfV))),
        q6cap::Add(center, q6cap::Add(q6cap::Scale(u, halfU), q6cap::Scale(v, halfV))),
        q6cap::Add(center, q6cap::Add(q6cap::Scale(u, -halfU), q6cap::Scale(v, halfV))),
    };
    float minX = 1.0e9f, minY = 1.0e9f, maxX = -1.0e9f, maxY = -1.0e9f;
    for (const q6cap::Vec3& corner : corners) {
        float px = 0.0f, py = 0.0f;
        if (!q6cap::ProjectPoint(camera, corner, &px, &py)) {
            return rect;
        }
        minX = std::min(minX, px);
        minY = std::min(minY, py);
        maxX = std::max(maxX, px);
        maxY = std::max(maxY, py);
    }
    rect.x0 = std::max(0, static_cast<int>(std::floor(minX)));
    rect.y0 = std::max(0, static_cast<int>(std::floor(minY)));
    rect.x1 = std::min(static_cast<int>(camera.width) - 1, static_cast<int>(std::ceil(maxX)));
    rect.y1 = std::min(static_cast<int>(camera.height) - 1, static_cast<int>(std::ceil(maxY)));
    rect.valid = rect.x1 > rect.x0 && rect.y1 > rect.y0;
    return rect;
}

PixelRect PatchFor(const std::string& fixture, const q6cap::Camera& camera) {
    if (fixture == "static") {
        return ProjectRect(camera, q6cap::Vec3{-2.2f, 5.0f, 1.8f}, q6cap::Vec3{1, 0, 0},
                           q6cap::Vec3{0, 0, 1}, 0.4f, 0.4f);
    }
    if (fixture == "alpha-emissive") {
        return ProjectRect(camera, q6cap::Vec3{0.0f, 3.2f, 1.3f}, q6cap::Vec3{1, 0, 0},
                           q6cap::Vec3{0, 0, 1}, 0.5f, 0.3f);
    }
    if (fixture == "transmission") {
        return ProjectRect(camera, q6cap::Vec3{-2.0f, 3.5f, 1.3f}, q6cap::Vec3{1, 0, 0},
                           q6cap::Vec3{0, 0, 1}, 0.3f, 0.3f);
    }
    return PixelRect();
}

double MeanInRect(const FloatImage& image, const PixelRect& rect, int channel) {
    if (!rect.valid) {
        return -1.0;
    }
    double sum = 0.0;
    uint64_t count = 0;
    for (int y = rect.y0; y <= rect.y1; ++y) {
        for (int x = rect.x0; x <= rect.x1; ++x) {
            const size_t index = (static_cast<size_t>(y) * image.width + x) * 3;
            sum += image.rgb[index + channel];
            ++count;
        }
    }
    return count == 0 ? -1.0 : sum / static_cast<double>(count);
}

double VarianceInRect(const FloatImage& image, const PixelRect& rect, int channel) {
    if (!rect.valid) {
        return -1.0;
    }
    double sum = 0.0;
    double sumSquares = 0.0;
    uint64_t count = 0;
    for (int y = rect.y0; y <= rect.y1; ++y) {
        for (int x = rect.x0; x <= rect.x1; ++x) {
            const size_t index = (static_cast<size_t>(y) * image.width + x) * 3;
            const double value = image.rgb[index + channel];
            sum += value;
            sumSquares += value * value;
            ++count;
        }
    }
    if (count == 0) {
        return -1.0;
    }
    const double mean = sum / static_cast<double>(count);
    return sumSquares / static_cast<double>(count) - mean * mean;
}

double P99Relative(const FloatImage& a, const FloatImage& b, const PixelRect& rect) {
    if (!rect.valid || a.rgb.size() != b.rgb.size()) {
        return -1.0;
    }
    std::vector<double> errors;
    for (int y = rect.y0; y <= rect.y1; ++y) {
        for (int x = rect.x0; x <= rect.x1; ++x) {
            const size_t index = (static_cast<size_t>(y) * a.width + x) * 3;
            for (int c = 0; c < 3; ++c) {
                const double reference = std::max<double>(b.rgb[index + c], 0.01);
                errors.push_back(std::fabs(static_cast<double>(a.rgb[index + c]) -
                                           static_cast<double>(b.rgb[index + c])) /
                                 reference);
            }
        }
    }
    if (errors.empty()) {
        return -1.0;
    }
    const size_t rank = static_cast<size_t>(std::floor(0.99 * (errors.size() - 1)));
    std::nth_element(errors.begin(), errors.begin() + static_cast<ptrdiff_t>(rank),
                     errors.end());
    return errors[rank];
}

std::vector<uint8_t> ToDisplayRgba8(const FloatImage& image) {
    std::vector<uint8_t> bytes(static_cast<size_t>(image.width) * image.height * 4);
    auto encode = [](float value) {
        if (!(value > 0.0f)) {
            value = 0.0f;
        }
        const float mapped = value / (1.0f + value);
        const float srgb = mapped <= 0.0031308f ? 12.92f * mapped
                                                : 1.055f * std::pow(mapped, 1.0f / 2.4f) - 0.055f;
        return static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, srgb * 255.0f + 0.5f)));
    };
    for (size_t i = 0; i < static_cast<size_t>(image.width) * image.height; ++i) {
        bytes[i * 4 + 0] = encode(image.rgb[i * 3 + 0]);
        bytes[i * 4 + 1] = encode(image.rgb[i * 3 + 1]);
        bytes[i * 4 + 2] = encode(image.rgb[i * 3 + 2]);
        bytes[i * 4 + 3] = 255;
    }
    return bytes;
}

bool WriteImagePng(const std::string& path, const FloatImage& image, std::string* error) {
    const std::vector<uint8_t> bytes = ToDisplayRgba8(image);
    return q4::WritePngRgba8(path, image.width, image.height, bytes.data(), image.width * 4,
                             error);
}

std::string JoinPath(const std::string& directory, const std::string& name) {
    if (directory.empty()) {
        return name;
    }
    const char last = directory.back();
    return (last == '\\' || last == '/') ? directory + name : directory + "\\" + name;
}

bool EnsureDirectory(const std::string& directory) {
    if (directory.empty()) {
        return true;
    }
    if (CreateDirectoryA(directory.c_str(), nullptr) != 0) {
        return true;
    }
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

// ---------------------------------------------------------------------------
// Device and pipeline
// ---------------------------------------------------------------------------

struct Renderer {
    ComPtr<IDXGIAdapter1> adapter;
    DXGI_ADAPTER_DESC1 adapterDesc{};
    std::string adapterName;
    ComPtr<ID3D12Device5> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList4> list;
    ComPtr<ID3D12Fence> fence;
    HANDLE fenceEvent = nullptr;
    UINT64 fenceValue = 0;
    UINT64 timestampFrequency = 0;
    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12StateObject> stateObject;
    ComPtr<ID3D12StateObjectProperties> stateProperties;
    ComPtr<ID3D12Resource> shaderBindingTable;
    D3D12_GPU_VIRTUAL_ADDRESS sbtAddress = 0;
    ComPtr<ID3D12DescriptorHeap> heap;
    UINT descriptorSize = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpu{};
    D3D12_CPU_DESCRIPTOR_HANDLE uavCpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE uavGpu{};
    bool debugLayer = false;
    std::string driverVersion = "unreported";
    std::string shaderModel = "unknown";

    bool Execute() {
        if (!Check(list->Close(), "Close")) {
            return false;
        }
        ID3D12CommandList* lists[1] = {list.Get()};
        queue->ExecuteCommandLists(1, lists);
        ++fenceValue;
        if (!Check(queue->Signal(fence.Get(), fenceValue), "Signal")) {
            return false;
        }
        if (!Check(fence->SetEventOnCompletion(fenceValue, fenceEvent),
                   "SetEventOnCompletion")) {
            return false;
        }
        const DWORD result = WaitForSingleObject(fenceEvent, static_cast<DWORD>(kFenceTimeoutMs));
        if (result != WAIT_OBJECT_0) {
            std::printf("ERROR GPU wait timed out or failed (%lu)\n", result);
            return false;
        }
        const HRESULT removed = device->GetDeviceRemovedReason();
        if (FAILED(removed)) {
            std::printf("ERROR device removed: %s\n", HrText(removed).c_str());
            return false;
        }
        return true;
    }

    bool Begin() {
        if (!Check(allocator->Reset(), "Reset(allocator)")) {
            return false;
        }
        return Check(list->Reset(allocator.Get(), nullptr), "Reset(list)");
    }
};

bool CreateRootSignature(ID3D12Device5* device, ID3D12RootSignature** out) {
    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 4;
    srvRange.BaseShaderRegister = 0;

    D3D12_DESCRIPTOR_RANGE uavRange{};
    uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange.NumDescriptors = 2;
    uavRange.BaseShaderRegister = 0;

    D3D12_ROOT_PARAMETER parameters[3] = {};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].Constants.ShaderRegister = 0;
    parameters[0].Constants.Num32BitValues = sizeof(FrameConstants) / 4;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable.NumDescriptorRanges = 1;
    parameters[1].DescriptorTable.pDescriptorRanges = &srvRange;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[2].DescriptorTable.NumDescriptorRanges = 1;
    parameters[2].DescriptorTable.pDescriptorRanges = &uavRange;
    parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = 3;
    desc.pParameters = parameters;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature,
                                             &error);
    if (FAILED(hr)) {
        std::printf("ERROR D3D12SerializeRootSignature failed: %s %s\n", HrText(hr).c_str(),
                    error ? static_cast<const char*>(error->GetBufferPointer()) : "");
        return false;
    }
    hr = device->CreateRootSignature(0, signature->GetBufferPointer(),
                                     signature->GetBufferSize(), IID_PPV_ARGS(out));
    return Check(hr, "CreateRootSignature");
}

bool CreatePipeline(ID3D12Device5* device, ID3D12RootSignature* rootSignature,
                    const std::vector<uint8_t>& shaderBytes, ID3D12StateObject** stateObject,
                    ID3D12StateObjectProperties** properties) {
    D3D12_DXIL_LIBRARY_DESC library{};
    library.DXILLibrary.pShaderBytecode = shaderBytes.data();
    library.DXILLibrary.BytecodeLength = shaderBytes.size();

    D3D12_HIT_GROUP_DESC hitGroup{};
    hitGroup.HitGroupExport = L"hitgroup";
    hitGroup.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
    hitGroup.AnyHitShaderImport = L"AnyHit";
    hitGroup.ClosestHitShaderImport = L"ClosestHit";

    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig{};
    shaderConfig.MaxPayloadSizeInBytes = 64;
    shaderConfig.MaxAttributeSizeInBytes = 8;

    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig{};
    pipelineConfig.MaxTraceRecursionDepth = 1;

    D3D12_GLOBAL_ROOT_SIGNATURE globalRootSignature{};
    globalRootSignature.pGlobalRootSignature = rootSignature;

    D3D12_STATE_SUBOBJECT subobjects[5] = {};
    subobjects[0].Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    subobjects[0].pDesc = &library;
    subobjects[1].Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
    subobjects[1].pDesc = &hitGroup;
    subobjects[2].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    subobjects[2].pDesc = &shaderConfig;
    subobjects[3].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    subobjects[3].pDesc = &pipelineConfig;
    subobjects[4].Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    subobjects[4].pDesc = &globalRootSignature;

    D3D12_STATE_OBJECT_DESC desc{};
    desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    desc.NumSubobjects = 5;
    desc.pSubobjects = subobjects;

    const HRESULT hr = device->CreateStateObject(&desc, IID_PPV_ARGS(stateObject));
    if (FAILED(hr)) {
        std::printf("ERROR CreateStateObject failed: %s\n", HrText(hr).c_str());
        return false;
    }
    return Check((*stateObject)->QueryInterface(IID_PPV_ARGS(properties)),
                 "QueryInterface(ID3D12StateObjectProperties)");
}

bool SelectAdapter(const Config& config, Renderer* renderer) {
    ComPtr<IDXGIFactory6> factory;
    if (!Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2")) {
        return false;
    }
    struct Entry {
        ComPtr<IDXGIAdapter1> adapter;
        DXGI_ADAPTER_DESC1 desc{};
        std::string name;
        bool software = false;
    };
    std::vector<Entry> adapters;
    for (UINT index = 0;; ++index) {
        ComPtr<IDXGIAdapter1> adapter;
        const HRESULT hr = factory->EnumAdapters1(index, &adapter);
        if (hr == DXGI_ERROR_NOT_FOUND || FAILED(hr)) {
            break;
        }
        Entry entry;
        entry.adapter = adapter;
        adapter->GetDesc1(&entry.desc);
        entry.name = Narrow(entry.desc.Description);
        entry.software = (entry.desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
        adapters.push_back(entry);
    }
    for (size_t i = 0; i < adapters.size(); ++i) {
        std::printf("ADAPTER[%zu]=%s LUID=0x%08lX%08lX VRAM_MB=%llu SOFTWARE=%d\n", i,
                    adapters[i].name.c_str(),
                    static_cast<unsigned long>(adapters[i].desc.AdapterLuid.HighPart),
                    static_cast<unsigned long>(adapters[i].desc.AdapterLuid.LowPart),
                    static_cast<unsigned long long>(adapters[i].desc.DedicatedVideoMemory /
                                                    (1024 * 1024)),
                    adapters[i].software ? 1 : 0);
    }
    if (adapters.empty()) {
        std::printf("ADAPTER_COUNT=0\n");
        return false;
    }
    int selected = -1;
    if (!config.adapterName.empty()) {
        for (size_t i = 0; i < adapters.size(); ++i) {
            if (adapters[i].name.find(config.adapterName) != std::string::npos) {
                selected = static_cast<int>(i);
                break;
            }
        }
    }
    if (selected < 0) {
        for (size_t i = 0; i < adapters.size(); ++i) {
            if (adapters[i].software) {
                continue;
            }
            if (selected < 0 ||
                adapters[i].desc.DedicatedVideoMemory >
                    adapters[static_cast<size_t>(selected)].desc.DedicatedVideoMemory) {
                selected = static_cast<int>(i);
            }
        }
    }
    if (selected < 0) {
        selected = 0;
    }
    renderer->adapter = adapters[static_cast<size_t>(selected)].adapter;
    renderer->adapterDesc = adapters[static_cast<size_t>(selected)].desc;
    renderer->adapterName = adapters[static_cast<size_t>(selected)].name;
    std::printf("ADAPTER_COUNT=%zu\n", adapters.size());
    std::printf("SELECTED_ADAPTER_NAME=%s\n", renderer->adapterName.c_str());
    std::printf("SELECTED_ADAPTER_LUID=0x%08lX%08lX\n",
                static_cast<unsigned long>(renderer->adapterDesc.AdapterLuid.HighPart),
                static_cast<unsigned long>(renderer->adapterDesc.AdapterLuid.LowPart));
    return true;
}

bool InitializeRenderer(const Config& config, Renderer* renderer) {
    if (!SelectAdapter(config, renderer)) {
        return false;
    }
    LARGE_INTEGER umd{};
    if (SUCCEEDED(renderer->adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umd))) {
        char buffer[64] = {};
        sprintf_s(buffer, "%u.%u.%u.%u", static_cast<unsigned>((umd.HighPart >> 16) & 0xFFFF),
                  static_cast<unsigned>(umd.HighPart & 0xFFFF),
                  static_cast<unsigned>((umd.LowPart >> 16) & 0xFFFF),
                  static_cast<unsigned>(umd.LowPart & 0xFFFF));
        renderer->driverVersion = buffer;
    }
    std::printf("DRIVER_UMD=%s\n", renderer->driverVersion.c_str());

    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        renderer->debugLayer = true;
    }
    std::printf("DEBUG_LAYER_ENABLED=%d\n", renderer->debugLayer ? 1 : 0);

    ComPtr<ID3D12Device> baseDevice;
    if (!Check(D3D12CreateDevice(renderer->adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                 IID_PPV_ARGS(&baseDevice)),
               "D3D12CreateDevice")) {
        return false;
    }
    if (!Check(baseDevice.As(&renderer->device), "QueryInterface(ID3D12Device5)")) {
        return false;
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5{};
    if (!Check(renderer->device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5,
                                                     sizeof(options5)),
               "CheckFeatureSupport(OPTIONS5)")) {
        return false;
    }
    std::printf("RAYTRACING_TIER=%s (%d)\n", TierName(options5.RaytracingTier),
                static_cast<int>(options5.RaytracingTier));
    if (options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
        std::printf("DXR_AVAILABLE=0\n");
        return false;
    }
    std::printf("DXR_AVAILABLE=1\n");

    D3D_SHADER_MODEL shaderModel = D3D_SHADER_MODEL_6_0;
    for (D3D_SHADER_MODEL candidate :
         {D3D_SHADER_MODEL_6_8, D3D_SHADER_MODEL_6_7, D3D_SHADER_MODEL_6_6,
          D3D_SHADER_MODEL_6_5, D3D_SHADER_MODEL_6_4, D3D_SHADER_MODEL_6_3,
          D3D_SHADER_MODEL_6_2, D3D_SHADER_MODEL_6_1, D3D_SHADER_MODEL_6_0}) {
        D3D12_FEATURE_DATA_SHADER_MODEL data{candidate};
        if (SUCCEEDED(renderer->device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &data,
                                                            sizeof(data)))) {
            shaderModel = data.HighestShaderModel;
            break;
        }
    }
    renderer->shaderModel = ShaderModelName(shaderModel);
    std::printf("SHADER_MODEL=%s\n", renderer->shaderModel.c_str());

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (!Check(renderer->device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&renderer->queue)),
               "CreateCommandQueue")) {
        return false;
    }
    renderer->queue->GetTimestampFrequency(&renderer->timestampFrequency);
    if (!Check(renderer->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                        IID_PPV_ARGS(&renderer->allocator)),
               "CreateCommandAllocator")) {
        return false;
    }
    if (!Check(renderer->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                   renderer->allocator.Get(), nullptr,
                                                   IID_PPV_ARGS(&renderer->list)),
               "CreateCommandList")) {
        return false;
    }
    // CreateCommandList returns an open list; close it so the first Begin() can
    // reset the allocator.
    if (!Check(renderer->list->Close(), "Close(initial list)")) {
        return false;
    }
    if (!Check(renderer->device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                             IID_PPV_ARGS(&renderer->fence)),
               "CreateFence")) {
        return false;
    }
    renderer->fenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    if (renderer->fenceEvent == nullptr) {
        std::printf("ERROR CreateEventEx failed\n");
        return false;
    }

    const std::string shaderPath = JoinPath(ExeDirectory(), "pathtrace_capture.cso");
    std::vector<uint8_t> shaderBytes;
    if (!ReadFileBytes(shaderPath, &shaderBytes)) {
        std::printf("ERROR could not read %s\n", shaderPath.c_str());
        return false;
    }
    std::printf("SHADER_BYTES=%llu\n", static_cast<unsigned long long>(shaderBytes.size()));

    if (!CreateRootSignature(renderer->device.Get(), &renderer->rootSignature)) {
        return false;
    }
    if (!CreatePipeline(renderer->device.Get(), renderer->rootSignature.Get(), shaderBytes,
                        &renderer->stateObject, &renderer->stateProperties)) {
        return false;
    }

    renderer->shaderBindingTable = CreateBuffer(
        renderer->device.Get(), 256, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_HEAP_TYPE_UPLOAD);
    if (!renderer->shaderBindingTable) {
        return false;
    }
    void* mapped = nullptr;
    D3D12_RANGE range{0, 256};
    if (!Check(renderer->shaderBindingTable->Map(0, &range, &mapped), "Map(SBT)")) {
        return false;
    }
    std::memset(mapped, 0, 256);
    const void* rayGen = renderer->stateProperties->GetShaderIdentifier(L"RayGen");
    const void* miss = renderer->stateProperties->GetShaderIdentifier(L"Miss");
    const void* hitGroup = renderer->stateProperties->GetShaderIdentifier(L"hitgroup");
    if (rayGen == nullptr || miss == nullptr || hitGroup == nullptr) {
        std::printf("ERROR missing shader identifier\n");
        return false;
    }
    std::memcpy(static_cast<uint8_t*>(mapped) + 0, rayGen, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    std::memcpy(static_cast<uint8_t*>(mapped) + 64, miss,
                D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    std::memcpy(static_cast<uint8_t*>(mapped) + 128, hitGroup,
                D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    renderer->shaderBindingTable->Unmap(0, nullptr);
    renderer->sbtAddress = renderer->shaderBindingTable->GetGPUVirtualAddress();

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 6;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (!Check(renderer->device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&renderer->heap)),
               "CreateDescriptorHeap")) {
        return false;
    }
    renderer->descriptorSize = renderer->device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    renderer->srvCpu = renderer->heap->GetCPUDescriptorHandleForHeapStart();
    renderer->srvGpu = renderer->heap->GetGPUDescriptorHandleForHeapStart();
    renderer->uavCpu = renderer->srvCpu;
    renderer->uavCpu.ptr += static_cast<SIZE_T>(renderer->descriptorSize) * 4;
    renderer->uavGpu = renderer->srvGpu;
    renderer->uavGpu.ptr += static_cast<UINT64>(renderer->descriptorSize) * 4;
    return true;
}

// ---------------------------------------------------------------------------
// Acceleration structures and one scene run
// ---------------------------------------------------------------------------

struct ObjectGpu {
    ComPtr<ID3D12Resource> blas;
    ComPtr<ID3D12Resource> blasUpdated;
    ComPtr<ID3D12Resource> active;
    ComPtr<ID3D12Resource> scratch;
    uint32_t vertexBase = 0;
    uint32_t vertexCount = 0;
    D3D12_RAYTRACING_GEOMETRY_DESC geometry{};
};

struct RunOptions {
    bool timing = false;
    std::string pngPath;
};

class SceneRun {
public:
    explicit SceneRun(Renderer& renderer) : renderer_(renderer) {}

    bool Render(const q6cap::Scene& scene, uint32_t samplesPerPixel, const RunOptions& options,
                FloatImage* image, std::string* error) {
        const uint32_t width = scene.camera.width;
        const uint32_t height = scene.camera.height;
        if (width == 0 || height == 0 || samplesPerPixel == 0) {
            *error = "invalid extent or sample count";
            return false;
        }
        timestampsMs_.clear();
        timing_ = options.timing;
        if (!CreateResources(scene, width, height, error)) {
            return false;
        }
        if (!UploadPose(scene, 0, error)) {
            return false;
        }

        // Phase A: build every bottom level from the rest pose.
        if (!renderer_.Begin()) {
            *error = "cannot begin command list";
            return false;
        }
        for (ObjectGpu& object : objects_) {
            RecordBottomLevelBuild(&object);
        }
        if (timing_ && !CreateTimingObjects(samplesPerPixel, error)) {
            return false;
        }
        if (!renderer_.Execute()) {
            *error = "BLAS build execution failed";
            return false;
        }

        // Phase B: a skinned target pose needs a refit from the rest-pose BLAS.
        // The upload content is read at execution time, so the refit must be a
        // separate submission after the build has executed.
        int targetFrame = 0;
        for (const q6cap::Object& object : scene.objects) {
            if (object.skinned && object.poseFrame > targetFrame) {
                targetFrame = object.poseFrame;
            }
        }
        if (targetFrame > 0) {
            if (!UploadPose(scene, targetFrame, error)) {
                return false;
            }
            if (!renderer_.Begin()) {
                *error = "cannot begin command list";
                return false;
            }
            for (size_t i = 0; i < scene.objects.size(); ++i) {
                if (scene.objects[i].skinned) {
                    RecordBottomLevelUpdate(&objects_[i]);
                }
            }
            if (!renderer_.Execute()) {
                *error = "BLAS update execution failed";
                return false;
            }
            for (size_t i = 0; i < scene.objects.size(); ++i) {
                if (scene.objects[i].skinned) {
                    objects_[i].active = objects_[i].blasUpdated;
                }
            }
        }

        // Phase C: top level, dispatch, readback.
        if (!renderer_.Begin()) {
            *error = "cannot begin command list";
            return false;
        }
        if (!BuildTopLevel(scene, error)) {
            return false;
        }
        if (!Dispatch(scene, samplesPerPixel, options.timing, error)) {
            return false;
        }
        if (!CopyResults(width, height)) {
            *error = "readback copy failed";
            return false;
        }
        if (!renderer_.Execute()) {
            *error = "render execution failed";
            return false;
        }
        if (!ReadBack(image, error)) {
            return false;
        }
        if (timing_) {
            if (!ReadTimestamps(samplesPerPixel)) {
                *error = "timestamp readback failed";
                return false;
            }
        }
        if (!options.pngPath.empty()) {
            if (!WriteImagePng(options.pngPath, *image, error)) {
                return false;
            }
        }
        return true;
    }

    const std::vector<double>& timestampsMs() const { return timestampsMs_; }

private:
    bool CreateResources(const q6cap::Scene& scene, uint32_t width, uint32_t height,
                         std::string* error) {
        uint32_t totalVertices = 0;
        for (const q6cap::Object& object : scene.objects) {
            if (object.positions.size() % 3 != 0 || object.positions.empty()) {
                *error = "object '" + object.name + "' is not a triangle list";
                return false;
            }
            totalVertices += static_cast<uint32_t>(object.positions.size());
        }
        positions_ = CreateBuffer(renderer_.device.Get(), 12ull * totalVertices,
                                  D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ,
                                  D3D12_HEAP_TYPE_UPLOAD);
        materials_ = CreateBuffer(renderer_.device.Get(),
                                  scene.materials.size() * kMaterialStride,
                                  D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ,
                                  D3D12_HEAP_TYPE_UPLOAD);
        instances_ = CreateBuffer(renderer_.device.Get(), scene.objects.size() * kInstanceStride,
                                  D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ,
                                  D3D12_HEAP_TYPE_UPLOAD);
        accum_ = CreateTexture2D(renderer_.device.Get(), width, height,
                                 DXGI_FORMAT_R32G32B32A32_FLOAT);
        depth_ = CreateTexture2D(renderer_.device.Get(), width, height, DXGI_FORMAT_R32_FLOAT);
        if (!positions_ || !materials_ || !instances_ || !accum_ || !depth_) {
            *error = "resource creation failed";
            return false;
        }

        void* mapped = nullptr;
        D3D12_RANGE range{0, 0};
        if (!Check(materials_->Map(0, &range, &mapped), "Map(materials)")) {
            *error = "map materials failed";
            return false;
        }
        auto* materials = static_cast<GpuMaterial*>(mapped);
        for (size_t i = 0; i < scene.materials.size(); ++i) {
            const q6cap::Material& source = scene.materials[i];
            std::memset(&materials[i], 0, sizeof(GpuMaterial));
            materials[i].albedoRoughness[0] = source.albedo[0];
            materials[i].albedoRoughness[1] = source.albedo[1];
            materials[i].albedoRoughness[2] = source.albedo[2];
            materials[i].albedoRoughness[3] = source.roughness;
            materials[i].tintIor[0] = source.tint[0];
            materials[i].tintIor[1] = source.tint[1];
            materials[i].tintIor[2] = source.tint[2];
            materials[i].tintIor[3] = source.ior;
            materials[i].emissionKind[0] = source.emission[0];
            materials[i].emissionKind[1] = source.emission[1];
            materials[i].emissionKind[2] = source.emission[2];
            materials[i].emissionKind[3] = static_cast<float>(source.kind);
            materials[i].alphaUv[0] = source.alphaTest ? 1.0f : 0.0f;
            materials[i].alphaUv[1] = source.alphaThreshold;
            materials[i].alphaUv[2] = static_cast<float>(source.uvMode);
            materials[i].uvOriginScale[0] = source.uvOriginX;
            materials[i].uvOriginScale[1] = source.uvOriginZ;
            materials[i].uvOriginScale[2] = source.uvScaleX;
            materials[i].uvOriginScale[3] = source.uvScaleZ;
        }
        materials_->Unmap(0, nullptr);

        if (!Check(instances_->Map(0, &range, &mapped), "Map(instances)")) {
            *error = "map instances failed";
            return false;
        }
        auto* instances = static_cast<GpuInstance*>(mapped);
        uint32_t vertexBase = 0;
        for (size_t i = 0; i < scene.objects.size(); ++i) {
            instances[i].materialIndex = static_cast<uint32_t>(scene.objects[i].material);
            instances[i].positionBase = vertexBase;
            instances[i].pad0 = 0;
            instances[i].pad1 = 0;
            vertexBase += static_cast<uint32_t>(scene.objects[i].positions.size());
        }
        instances_->Unmap(0, nullptr);

        D3D12_SHADER_RESOURCE_VIEW_DESC bufferView{};
        bufferView.Format = DXGI_FORMAT_UNKNOWN;
        bufferView.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        bufferView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        bufferView.Buffer.FirstElement = 0;
        bufferView.Buffer.NumElements = static_cast<UINT>(scene.materials.size());
        bufferView.Buffer.StructureByteStride = kMaterialStride;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu = renderer_.srvCpu;
        cpu.ptr += static_cast<SIZE_T>(renderer_.descriptorSize) * 1;
        renderer_.device->CreateShaderResourceView(materials_.Get(), &bufferView, cpu);

        D3D12_SHADER_RESOURCE_VIEW_DESC instanceView = bufferView;
        instanceView.Buffer.NumElements = static_cast<UINT>(scene.objects.size());
        instanceView.Buffer.StructureByteStride = kInstanceStride;
        cpu = renderer_.srvCpu;
        cpu.ptr += static_cast<SIZE_T>(renderer_.descriptorSize) * 2;
        renderer_.device->CreateShaderResourceView(instances_.Get(), &instanceView, cpu);

        D3D12_SHADER_RESOURCE_VIEW_DESC positionView = bufferView;
        positionView.Buffer.NumElements = totalVertices;
        positionView.Buffer.StructureByteStride = 12;
        cpu = renderer_.srvCpu;
        cpu.ptr += static_cast<SIZE_T>(renderer_.descriptorSize) * 3;
        renderer_.device->CreateShaderResourceView(positions_.Get(), &positionView, cpu);

        D3D12_UNORDERED_ACCESS_VIEW_DESC accumView{};
        accumView.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        accumView.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        renderer_.device->CreateUnorderedAccessView(accum_.Get(), nullptr, &accumView,
                                                    renderer_.uavCpu);
        D3D12_UNORDERED_ACCESS_VIEW_DESC depthView{};
        depthView.Format = DXGI_FORMAT_R32_FLOAT;
        depthView.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        D3D12_CPU_DESCRIPTOR_HANDLE depthCpu = renderer_.uavCpu;
        depthCpu.ptr += static_cast<SIZE_T>(renderer_.descriptorSize);
        renderer_.device->CreateUnorderedAccessView(depth_.Get(), nullptr, &depthView, depthCpu);

        objects_.clear();
        objects_.resize(scene.objects.size());
        uint32_t base = 0;
        for (size_t i = 0; i < scene.objects.size(); ++i) {
            ObjectGpu& object = objects_[i];
            object.vertexBase = base;
            object.vertexCount = static_cast<uint32_t>(scene.objects[i].positions.size());
            base += object.vertexCount;
            if (!BuildBottomLevel(&object, positions_->GetGPUVirtualAddress() +
                                              12ull * object.vertexBase)) {
                *error = "BLAS allocation failed";
                return false;
            }
        }
        return true;
    }

    bool UploadPose(const q6cap::Scene& scene, int frame, std::string* error) {
        void* mapped = nullptr;
        D3D12_RANGE range{0, 0};
        if (!Check(positions_->Map(0, &range, &mapped), "Map(positions)")) {
            *error = "map positions failed";
            return false;
        }
        auto* positions = static_cast<q6cap::Vec3*>(mapped);
        uint32_t index = 0;
        for (const q6cap::Object& object : scene.objects) {
            const std::vector<q6cap::Vec3> posed = q6cap::PoseObject(object, frame);
            if (posed.size() != object.positions.size()) {
                *error = "pose size mismatch";
                return false;
            }
            std::memcpy(positions + index, posed.data(), posed.size() * sizeof(q6cap::Vec3));
            index += static_cast<uint32_t>(posed.size());
        }
        positions_->Unmap(0, nullptr);
        return true;
    }

    bool BuildBottomLevel(ObjectGpu* object, D3D12_GPU_VIRTUAL_ADDRESS vertexBase) {
        object->geometry = {};
        object->geometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        object->geometry.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
        object->geometry.Triangles.VertexBuffer.StartAddress = vertexBase;
        object->geometry.Triangles.VertexBuffer.StrideInBytes = sizeof(float) * 3;
        object->geometry.Triangles.VertexCount = object->vertexCount;
        object->geometry.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.NumDescs = 1;
        inputs.pGeometryDescs = &object->geometry;
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE |
                       D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild{};
        renderer_.device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS updateInputs = inputs;
        updateInputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO updatePrebuild{};
        renderer_.device->GetRaytracingAccelerationStructurePrebuildInfo(&updateInputs,
                                                                        &updatePrebuild);
        if (prebuild.ResultDataMaxSizeInBytes == 0) {
            return false;
        }
        const uint64_t scratchSize = std::max<uint64_t>(prebuild.ScratchDataSizeInBytes,
                                                        updatePrebuild.ScratchDataSizeInBytes);
        object->blas = CreateBuffer(renderer_.device.Get(), prebuild.ResultDataMaxSizeInBytes,
                                    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                                    D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
                                    D3D12_HEAP_TYPE_DEFAULT);
        object->blasUpdated =
            CreateBuffer(renderer_.device.Get(), prebuild.ResultDataMaxSizeInBytes,
                         D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                         D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
                         D3D12_HEAP_TYPE_DEFAULT);
        object->scratch = CreateBuffer(renderer_.device.Get(), scratchSize,
                                       D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                                       D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                       D3D12_HEAP_TYPE_DEFAULT);
        return object->blas && object->blasUpdated && object->scratch;
    }

    void RecordBottomLevelBuild(ObjectGpu* object) {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
        build.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        build.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        build.Inputs.NumDescs = 1;
        build.Inputs.pGeometryDescs = &object->geometry;
        build.Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE |
                             D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
        build.ScratchAccelerationStructureData = object->scratch->GetGPUVirtualAddress();
        build.DestAccelerationStructureData = object->blas->GetGPUVirtualAddress();
        renderer_.list->BuildRaytracingAccelerationStructure(&build, 0, nullptr);
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = object->blas.Get();
        renderer_.list->ResourceBarrier(1, &barrier);
    }

    void RecordBottomLevelUpdate(ObjectGpu* object) {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
        build.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        build.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        build.Inputs.NumDescs = 1;
        build.Inputs.pGeometryDescs = &object->geometry;
        build.Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE |
                             D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE |
                             D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
        build.ScratchAccelerationStructureData = object->scratch->GetGPUVirtualAddress();
        build.DestAccelerationStructureData = object->blasUpdated->GetGPUVirtualAddress();
        build.SourceAccelerationStructureData = object->blas->GetGPUVirtualAddress();
        renderer_.list->BuildRaytracingAccelerationStructure(&build, 0, nullptr);
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = object->blasUpdated.Get();
        renderer_.list->ResourceBarrier(1, &barrier);
    }

    bool BuildTopLevel(const q6cap::Scene& scene, std::string* error) {
        std::vector<D3D12_RAYTRACING_INSTANCE_DESC> descriptions(scene.objects.size());
        for (size_t i = 0; i < scene.objects.size(); ++i) {
            D3D12_RAYTRACING_INSTANCE_DESC& instance = descriptions[i];
            std::memset(&instance, 0, sizeof(instance));
            instance.Transform[0][0] = 1.0f;
            instance.Transform[1][1] = 1.0f;
            instance.Transform[2][2] = 1.0f;
            instance.Transform[0][3] = scene.objects[i].translation.x;
            instance.Transform[1][3] = scene.objects[i].translation.y;
            instance.Transform[2][3] = scene.objects[i].translation.z;
            instance.InstanceID = static_cast<UINT>(i);
            instance.InstanceMask = 0xFF;
            instance.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
            instance.AccelerationStructure = objects_[i].active != nullptr
                                                 ? objects_[i].active->GetGPUVirtualAddress()
                                                 : objects_[i].blas->GetGPUVirtualAddress();
        }
        const UINT64 instanceBytes = descriptions.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
        instanceUpload_ = CreateBuffer(renderer_.device.Get(), instanceBytes,
                                       D3D12_RESOURCE_FLAG_NONE,
                                       D3D12_RESOURCE_STATE_GENERIC_READ,
                                       D3D12_HEAP_TYPE_UPLOAD);
        if (!instanceUpload_) {
            *error = "instance buffer allocation failed";
            return false;
        }
        void* mapped = nullptr;
        D3D12_RANGE range{0, 0};
        if (!Check(instanceUpload_->Map(0, &range, &mapped), "Map(top-level instances)")) {
            *error = "map top-level instances failed";
            return false;
        }
        std::memcpy(mapped, descriptions.data(), instanceBytes);
        instanceUpload_->Unmap(0, nullptr);

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.NumDescs = static_cast<UINT>(descriptions.size());
        inputs.InstanceDescs = instanceUpload_->GetGPUVirtualAddress();
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild{};
        renderer_.device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);
        if (prebuild.ResultDataMaxSizeInBytes == 0) {
            *error = "TLAS prebuild size is zero";
            return false;
        }
        tlas_ = CreateBuffer(renderer_.device.Get(), prebuild.ResultDataMaxSizeInBytes,
                             D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                             D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
                             D3D12_HEAP_TYPE_DEFAULT);
        tlasScratch_ = CreateBuffer(renderer_.device.Get(), prebuild.ScratchDataSizeInBytes,
                                    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                    D3D12_HEAP_TYPE_DEFAULT);
        if (!tlas_ || !tlasScratch_) {
            *error = "TLAS allocation failed";
            return false;
        }
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
        build.Inputs = inputs;
        build.ScratchAccelerationStructureData = tlasScratch_->GetGPUVirtualAddress();
        build.DestAccelerationStructureData = tlas_->GetGPUVirtualAddress();
        renderer_.list->BuildRaytracingAccelerationStructure(&build, 0, nullptr);
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = tlas_.Get();
        renderer_.list->ResourceBarrier(1, &barrier);

        D3D12_SHADER_RESOURCE_VIEW_DESC accelerationView{};
        accelerationView.Format = DXGI_FORMAT_UNKNOWN;
        accelerationView.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        accelerationView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        accelerationView.RaytracingAccelerationStructure.Location =
            tlas_->GetGPUVirtualAddress();
        renderer_.device->CreateShaderResourceView(nullptr, &accelerationView,
                                                   renderer_.srvCpu);
        return true;
    }

    bool CreateTimingObjects(uint32_t samplesPerPixel, std::string* error) {
        D3D12_QUERY_HEAP_DESC desc{};
        desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        desc.Count = samplesPerPixel * 2;
        if (!Check(renderer_.device->CreateQueryHeap(&desc, IID_PPV_ARGS(&queryHeap_)),
                   "CreateQueryHeap")) {
            *error = "timestamp query heap failed";
            return false;
        }
        timestampReadback_ = CreateBuffer(renderer_.device.Get(), sizeof(UINT64) * 2 *
                                                                        samplesPerPixel,
                                          D3D12_RESOURCE_FLAG_NONE,
                                          D3D12_RESOURCE_STATE_COPY_DEST,
                                          D3D12_HEAP_TYPE_READBACK);
        if (!timestampReadback_) {
            *error = "timestamp readback failed";
            return false;
        }
        return true;
    }

    bool Dispatch(const q6cap::Scene& scene, uint32_t samplesPerPixel, bool timing,
                  std::string* error) {
        (void)error;
        float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        renderer_.list->ClearUnorderedAccessViewFloat(renderer_.uavGpu, renderer_.uavCpu,
                                                      accum_.Get(), clear, 0, nullptr);

        ID3D12DescriptorHeap* heaps[1] = {renderer_.heap.Get()};
        renderer_.list->SetDescriptorHeaps(1, heaps);
        renderer_.list->SetComputeRootSignature(renderer_.rootSignature.Get());
        renderer_.list->SetComputeRootDescriptorTable(1, renderer_.srvGpu);
        renderer_.list->SetComputeRootDescriptorTable(2, renderer_.uavGpu);
        renderer_.list->SetPipelineState1(renderer_.stateObject.Get());

        D3D12_DISPATCH_RAYS_DESC dispatch{};
        dispatch.RayGenerationShaderRecord.StartAddress = renderer_.sbtAddress + 0;
        dispatch.RayGenerationShaderRecord.SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        dispatch.MissShaderTable.StartAddress = renderer_.sbtAddress + 64;
        dispatch.MissShaderTable.SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        dispatch.MissShaderTable.StrideInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        dispatch.HitGroupTable.StartAddress = renderer_.sbtAddress + 128;
        dispatch.HitGroupTable.SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        dispatch.HitGroupTable.StrideInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        dispatch.Width = scene.camera.width;
        dispatch.Height = scene.camera.height;
        dispatch.Depth = 1;

        uint32_t queryIndex = 0;
        const uint32_t stride = timing ? 1u : kMaxDispatchSamples;
        for (uint32_t offset = 0; offset < samplesPerPixel; offset += stride) {
            const uint32_t count =
                timing ? 1u : std::min(kMaxDispatchSamples, samplesPerPixel - offset);
            FrameConstants constants{};
            constants.camPos[0] = scene.camera.position.x;
            constants.camPos[1] = scene.camera.position.y;
            constants.camPos[2] = scene.camera.position.z;
            constants.camFwd[0] = scene.camera.forward.x;
            constants.camFwd[1] = scene.camera.forward.y;
            constants.camFwd[2] = scene.camera.forward.z;
            constants.camRight[0] = scene.camera.right.x;
            constants.camRight[1] = scene.camera.right.y;
            constants.camRight[2] = scene.camera.right.z;
            constants.camUp[0] = scene.camera.up.x;
            constants.camUp[1] = scene.camera.up.y;
            constants.camUp[2] = scene.camera.up.z;
            constants.extent[0] = scene.camera.tanHalfFovX;
            constants.extent[1] = scene.camera.tanHalfFovY;
            constants.extent[2] = static_cast<float>(scene.camera.width);
            constants.extent[3] = static_cast<float>(scene.camera.height);
            constants.envUpper[0] = scene.environment.upper[0];
            constants.envUpper[1] = scene.environment.upper[1];
            constants.envUpper[2] = scene.environment.upper[2];
            constants.envUpper[3] = scene.environment.enabled ? 1.0f : 0.0f;
            constants.envLower[0] = scene.environment.lower[0];
            constants.envLower[1] = scene.environment.lower[1];
            constants.envLower[2] = scene.environment.lower[2];
            constants.lightPos[0] = scene.light.position.x;
            constants.lightPos[1] = scene.light.position.y;
            constants.lightPos[2] = scene.light.position.z;
            constants.lightPos[3] = scene.light.enabled ? 1.0f : 0.0f;
            constants.lightColor[0] = scene.light.radiance[0];
            constants.lightColor[1] = scene.light.radiance[1];
            constants.lightColor[2] = scene.light.radiance[2];
            constants.counts[0] = offset;
            constants.counts[1] = scene.maxDepth;
            constants.counts[2] = scene.camera.width;
            constants.counts[3] = scene.camera.height;
            constants.seed[0] = kSeed;
            constants.seed[1] = 0;
            constants.misc[0] = count;

            renderer_.list->SetComputeRoot32BitConstants(0, sizeof(FrameConstants) / 4,
                                                         &constants, 0);
            if (timing) {
                renderer_.list->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
                                         queryIndex * 2);
            }
            renderer_.list->DispatchRays(&dispatch);
            if (timing) {
                renderer_.list->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
                                         queryIndex * 2 + 1);
                ++queryIndex;
            }
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            barrier.UAV.pResource = accum_.Get();
            renderer_.list->ResourceBarrier(1, &barrier);
        }
        if (timing) {
            renderer_.list->ResolveQueryData(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0,
                                             samplesPerPixel * 2, timestampReadback_.Get(), 0);
        }
        return true;
    }

    bool CopyResults(uint32_t width, uint32_t height) {
        accumPitch_ = Align(width * 16, 256);
        depthPitch_ = Align(width * 4, 256);
        width_ = width;
        height_ = height;
        readbackAccum_ = CreateBuffer(renderer_.device.Get(), accumPitch_ * height,
                                      D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST,
                                      D3D12_HEAP_TYPE_READBACK);
        readbackDepth_ = CreateBuffer(renderer_.device.Get(), depthPitch_ * height,
                                      D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST,
                                      D3D12_HEAP_TYPE_READBACK);
        if (!readbackAccum_ || !readbackDepth_) {
            return false;
        }
        D3D12_RESOURCE_BARRIER barriers[2] = {};
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Transition.pResource = accum_.Get();
        barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition.pResource = depth_.Get();
        barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        renderer_.list->ResourceBarrier(2, barriers);

        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = accum_.Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        source.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION destination{};
        destination.pResource = readbackAccum_.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        destination.PlacedFootprint.Footprint.Width = width;
        destination.PlacedFootprint.Footprint.Height = height;
        destination.PlacedFootprint.Footprint.Depth = 1;
        destination.PlacedFootprint.Footprint.RowPitch = accumPitch_;
        renderer_.list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

        source.pResource = depth_.Get();
        destination.pResource = readbackDepth_.Get();
        destination.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32_FLOAT;
        destination.PlacedFootprint.Footprint.RowPitch = depthPitch_;
        renderer_.list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        return true;
    }

    bool ReadBack(FloatImage* image, std::string* error) {
        image->width = width_;
        image->height = height_;
        image->rgb.assign(static_cast<size_t>(width_) * height_ * 3, 0.0f);
        image->depth.assign(static_cast<size_t>(width_) * height_, -1.0f);

        void* mapped = nullptr;
        D3D12_RANGE range{0, static_cast<SIZE_T>(accumPitch_) * height_};
        if (!Check(readbackAccum_->Map(0, &range, &mapped), "Map(accum readback)")) {
            *error = "map accum readback failed";
            return false;
        }
        for (uint32_t y = 0; y < height_; ++y) {
            const float* row = reinterpret_cast<const float*>(
                static_cast<const uint8_t*>(mapped) + static_cast<size_t>(y) * accumPitch_);
            for (uint32_t x = 0; x < width_; ++x) {
                const size_t index = (static_cast<size_t>(y) * width_ + x) * 3;
                image->rgb[index + 0] = row[x * 4 + 0];
                image->rgb[index + 1] = row[x * 4 + 1];
                image->rgb[index + 2] = row[x * 4 + 2];
            }
        }
        D3D12_RANGE noWrite{0, 0};
        readbackAccum_->Unmap(0, &noWrite);

        D3D12_RANGE depthRange{0, static_cast<SIZE_T>(depthPitch_) * height_};
        if (!Check(readbackDepth_->Map(0, &depthRange, &mapped), "Map(depth readback)")) {
            *error = "map depth readback failed";
            return false;
        }
        for (uint32_t y = 0; y < height_; ++y) {
            const float* row = reinterpret_cast<const float*>(
                static_cast<const uint8_t*>(mapped) + static_cast<size_t>(y) * depthPitch_);
            for (uint32_t x = 0; x < width_; ++x) {
                image->depth[static_cast<size_t>(y) * width_ + x] = row[x];
            }
        }
        readbackDepth_->Unmap(0, &noWrite);
        return true;
    }

    bool ReadTimestamps(uint32_t samplesPerPixel) {
        void* mapped = nullptr;
        D3D12_RANGE range{0, sizeof(UINT64) * 2 * samplesPerPixel};
        if (!Check(timestampReadback_->Map(0, &range, &mapped), "Map(timestamps)")) {
            return false;
        }
        const UINT64* values = static_cast<const UINT64*>(mapped);
        for (uint32_t i = 0; i < samplesPerPixel; ++i) {
            if (values[i * 2 + 1] > values[i * 2]) {
                timestampsMs_.push_back(static_cast<double>(values[i * 2 + 1] -
                                                            values[i * 2]) *
                                        1000.0 / static_cast<double>(renderer_.timestampFrequency));
            }
        }
        D3D12_RANGE noWrite{0, 0};
        timestampReadback_->Unmap(0, &noWrite);
        return true;
    }

    Renderer& renderer_;
    bool timing_ = false;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    UINT accumPitch_ = 0;
    UINT depthPitch_ = 0;
    std::vector<ObjectGpu> objects_;
    ComPtr<ID3D12Resource> positions_;
    ComPtr<ID3D12Resource> materials_;
    ComPtr<ID3D12Resource> instances_;
    ComPtr<ID3D12Resource> instanceUpload_;
    ComPtr<ID3D12Resource> accum_;
    ComPtr<ID3D12Resource> depth_;
    ComPtr<ID3D12Resource> tlas_;
    ComPtr<ID3D12Resource> tlasScratch_;
    ComPtr<ID3D12Resource> readbackAccum_;
    ComPtr<ID3D12Resource> readbackDepth_;
    ComPtr<ID3D12QueryHeap> queryHeap_;
    ComPtr<ID3D12Resource> timestampReadback_;
    std::vector<double> timestampsMs_;
};

// ---------------------------------------------------------------------------
// Suite
// ---------------------------------------------------------------------------

struct Plan {
    std::string id;
    std::string fixture;
    int frame = 0;
    bool alphaControl = false;
    std::vector<uint32_t> spp;
    uint32_t imageSpp = 0;
    uint32_t refSpp = 0;
};

struct FrameResult {
    std::string id;
    std::string fixture;
    int frame = 0;
    bool alphaControl = false;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t imageSpp = 0;
    uint32_t refSpp = 0;
    FloatImage image;
    FloatImage aux;  // 1-spp calibration or 64-spp static
    bool hasAux = false;
    double mae = -1.0;
    double p99 = -1.0;
    double scaling = -1.0;
    uint64_t nanPixels = 0;
    bool determinism = false;
    std::string shaPng;
    std::string shaRaw;
    bool accepted = false;
    std::string checks;
};

struct CheckResult {
    bool ok = true;
    std::string reasons;
    std::string detail;

    void Require(bool condition, const std::string& label) {
        if (!condition) {
            ok = false;
            if (!reasons.empty()) {
                reasons += "; ";
            }
            reasons += label;
        }
    }
    void Note(const std::string& label) {
        if (!detail.empty()) {
            detail += "; ";
        }
        detail += label;
    }
};

double CentroidX(const FloatImage& image, bool redMask) {
    double sumX = 0.0;
    uint64_t count = 0;
    for (uint32_t y = 0; y < image.height; ++y) {
        for (uint32_t x = 0; x < image.width; ++x) {
            const size_t index = (static_cast<size_t>(y) * image.width + x) * 3;
            const float r = image.rgb[index + 0];
            const float g = image.rgb[index + 1];
            const float b = image.rgb[index + 2];
            bool selected;
            if (redMask) {
                selected = r > 0.12f && r > 2.0f * g && r > 2.0f * b;
            } else {
                selected = b > 0.08f && b > 1.5f * r && b > 1.2f * g;
            }
            if (selected) {
                sumX += x;
                ++count;
            }
        }
    }
    return count == 0 ? -1.0 : sumX / static_cast<double>(count);
}

double MaxVertexDisplacement(const q6cap::Scene& scene) {
    double maximum = 0.0;
    for (const q6cap::Object& object : scene.objects) {
        if (!object.skinned) {
            continue;
        }
        const std::vector<q6cap::Vec3> rest = q6cap::PoseObject(object, 0);
        const std::vector<q6cap::Vec3> posed = q6cap::PoseObject(object, 6);
        for (size_t i = 0; i < rest.size(); ++i) {
            const double dx = posed[i].x - rest[i].x;
            const double dy = posed[i].y - rest[i].y;
            const double dz = posed[i].z - rest[i].z;
            maximum = std::max(maximum, std::sqrt(dx * dx + dy * dy + dz * dz));
        }
    }
    return maximum;
}

bool HasHdrHoles(const FloatImage& image, uint64_t* count) {
    uint64_t found = 0;
    for (size_t i = 0; i < image.rgb.size(); i += 3) {
        if (image.rgb[i + 0] >= 3.5f && image.rgb[i + 1] >= 2.8f && image.rgb[i + 2] >= 1.9f) {
            ++found;
        }
    }
    if (count != nullptr) {
        *count = found;
    }
    return found > 0;
}

void WriteReport(const std::string& path, const Renderer& renderer, const Config& config,
                 const std::vector<FrameResult>& results, uint32_t tier, const char* tierName,
                 const char* shaderModel, double timingMedian, double timingP95,
                 uint32_t timingPasses, bool allAccepted) {
    FILE* file = nullptr;
    if (fopen_s(&file, path.c_str(), "wb") != 0 || file == nullptr) {
        std::printf("ERROR cannot write %s\n", path.c_str());
        return;
    }
    std::fprintf(file, "{\n");
    std::fprintf(file, "  \"probe\": \"q6_dxr_path_traced_capture\",\n");
    std::fprintf(file, "  \"scope\": \"synthetic capture fixtures; not PT-S1..S5 content and not a Q6 engine frame\",\n");
    std::fprintf(file, "  \"q6_acceptance\": false,\n");
    std::fprintf(file, "  \"rubric_identities\": \"PT-S1..S5 are not satisfied: the captures use synthetic fixtures, not the Q4 content cells, and there is no Q6 engine frame\",\n");
    std::fprintf(file, "  \"input_latency\": \"NOT RUN (no window or input path)\",\n");
    std::fprintf(file, "  \"adapter\": {\"name\": \"%s\", \"luid\": \"0x%08lX%08lX\", "
                       "\"vendor\": %u, \"device\": %u, \"driver_umd\": \"%s\", "
                       "\"vram_mb\": %llu, \"software\": %d},\n",
                 renderer.adapterName.c_str(),
                 static_cast<unsigned long>(renderer.adapterDesc.AdapterLuid.HighPart),
                 static_cast<unsigned long>(renderer.adapterDesc.AdapterLuid.LowPart),
                 renderer.adapterDesc.VendorId, renderer.adapterDesc.DeviceId,
                 renderer.driverVersion.c_str(),
                 static_cast<unsigned long long>(renderer.adapterDesc.DedicatedVideoMemory /
                                                 (1024 * 1024)),
                 (renderer.adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? 1 : 0);
    std::fprintf(file, "  \"raytracing_tier\": %u,\n  \"raytracing_tier_name\": \"%s\",\n",
                 tier, tierName);
    std::fprintf(file, "  \"shader_model\": \"%s\",\n", shaderModel);
    std::fprintf(file, "  \"debug_layer\": %s,\n", renderer.debugLayer ? "true" : "false");
    std::fprintf(file, "  \"width\": %u,\n  \"height\": %u,\n", config.width, config.height);
    std::fprintf(file, "  \"timing\": {\"passes\": %u, \"median_ms\": %.4f, \"p95_ms\": %.4f, "
                       "\"note\": \"offline reference renderer at the declared extent; not a "
                       "playability gate and not the hybrid path\"},\n",
                 timingPasses, timingMedian, timingP95);
    std::fprintf(file, "  \"captures\": [\n");
    for (size_t i = 0; i < results.size(); ++i) {
        const FrameResult& result = results[i];
        std::fprintf(file, "    {\"id\": \"%s\", \"fixture\": \"%s\", \"frame\": %d, "
                           "\"alpha_control\": %s, \"width\": %u, \"height\": %u, "
                           "\"image_spp\": %u, \"reference_spp\": %u, \"mae\": %.6f, "
                           "\"p99_relative\": %.6f, \"sample_scaling\": %.6f, "
                           "\"nan_pixels\": %llu, \"deterministic_rerun\": %s, "
                           "\"sha256_png\": \"%s\", \"sha256_linear_rgb\": \"%s\", "
                           "\"accepted\": %s, \"checks\": \"%s\"}%s\n",
                    result.id.c_str(), result.fixture.c_str(), result.frame,
                    result.alphaControl ? "true" : "false", result.width, result.height,
                    result.imageSpp, result.refSpp, result.mae, result.p99, result.scaling,
                    static_cast<unsigned long long>(result.nanPixels),
                    result.determinism ? "true" : "false", result.shaPng.c_str(),
                    result.shaRaw.c_str(), result.accepted ? "true" : "false",
                    result.checks.c_str(), (i + 1 == results.size()) ? "" : ",");
    }
    std::fprintf(file, "  ],\n");
    std::fprintf(file, "  \"all_accepted\": %s\n", allAccepted ? "true" : "false");
    std::fprintf(file, "}\n");
    std::fclose(file);
    std::printf("REPORT=%s\n", path.c_str());
}

}  // namespace

int main(int argc, char** argv) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            config.outDir = argv[++i];
        } else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            config.width = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            config.height = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--spp-cap") == 0 && i + 1 < argc) {
            config.sppCap = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--skip-timing") == 0) {
            config.skipTiming = true;
        } else if (std::strcmp(argv[i], "--adapter") == 0 && i + 1 < argc) {
            config.adapterName = argv[++i];
        } else {
            std::printf("unknown argument: %s\n", argv[i]);
            return 1;
        }
    }

    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
        std::printf("ERROR CoInitializeEx failed\n");
        return 1;
    }
    if (!EnsureDirectory(config.outDir)) {
        std::printf("ERROR cannot create output directory %s\n", config.outDir.c_str());
        return 1;
    }

    Renderer renderer;
    std::printf("PROBE=q6_dxr_path_traced_capture\n");
    if (!InitializeRenderer(config, &renderer)) {
        std::printf("RESULT=UNSUPPORTED\nEXIT_CODE=2\n");
        return 2;
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5{};
    renderer.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5,
                                         sizeof(options5));
    std::vector<Plan> plans;
    plans.push_back({"00-flat-denoise", "calibration", 0, false, {64}, 64, 0});
    plans.push_back({"01-calibration", "calibration", 0, false, {1, 4096}, 4096, 4096});
    plans.push_back({"02-static", "static", 0, false, {64, 512, 1024, 4096}, 1024, 4096});
    plans.push_back({"03-rigid-frame0", "rigid", 0, false, {512}, 512, 0});
    plans.push_back({"04-rigid-frame8", "rigid", 8, false, {512}, 512, 0});
    plans.push_back({"05-skinned-frame0", "skinned", 0, false, {512}, 512, 0});
    plans.push_back({"06-skinned-frame6", "skinned", 6, false, {512}, 512, 0});
    plans.push_back(
        {"07-alpha-emissive", "alpha-emissive", 0, false, {512, 1024, 4096}, 1024, 4096});
    plans.push_back({"08-alpha-emissive-control", "alpha-emissive", 0, true, {1024}, 1024, 0});
    plans.push_back(
        {"09-transmission", "transmission", 0, false, {512, 1024, 4096}, 1024, 4096});

    if (config.sppCap > 0) {
        for (Plan& plan : plans) {
            for (uint32_t& value : plan.spp) {
                value = std::min(value, config.sppCap);
            }
            std::sort(plan.spp.begin(), plan.spp.end());
            plan.spp.erase(std::unique(plan.spp.begin(), plan.spp.end()), plan.spp.end());
            plan.imageSpp = std::min(plan.imageSpp, config.sppCap);
            plan.refSpp = std::min(plan.refSpp, config.sppCap);
        }
    }

    std::vector<FrameResult> results;
    std::vector<q6cap::Scene> scenes;
    bool deviceFailure = false;

    for (const Plan& plan : plans) {
        q6cap::Scene scene;
        std::string error;
        if (!q6cap::BuildScene(plan.fixture, config.width, config.height, plan.frame,
                               plan.alphaControl, &scene, &error)) {
            std::printf("ERROR %s\n", error.c_str());
            return 1;
        }
        std::printf("CAPTURE_START=%s fixture=%s frame=%d %ux%u spp=", plan.id.c_str(),
                    plan.fixture.c_str(), plan.frame, scene.camera.width, scene.camera.height);
        for (size_t i = 0; i < plan.spp.size(); ++i) {
            std::printf("%s%u", i == 0 ? "" : ",", plan.spp[i]);
        }
        std::printf("\n");

        FrameResult result;
        result.id = plan.id;
        result.fixture = plan.fixture;
        result.frame = plan.frame;
        result.alphaControl = plan.alphaControl;
        result.width = scene.camera.width;
        result.height = scene.camera.height;
        result.imageSpp = plan.imageSpp;
        result.refSpp = plan.refSpp;

        std::vector<std::pair<uint32_t, FloatImage>> snapshots;
        {
            SceneRun run(renderer);
            for (uint32_t spp : plan.spp) {
                FloatImage image;
                RunOptions options;
                const bool wantsPng =
                    (spp == plan.imageSpp) || (plan.fixture == "static" && spp == 4096);
                if (wantsPng) {
                    options.pngPath =
                        JoinPath(config.outDir, plan.id + "-" + std::to_string(spp) + "spp.png");
                }
                const ULONGLONG start = GetTickCount64();
                if (!run.Render(scene, spp, options, &image, &error)) {
                    std::printf("ERROR %s: %s\n", plan.id.c_str(), error.c_str());
                    deviceFailure = true;
                    break;
                }
                std::printf("CAPTURE_SAMPLE=%s spp=%u wall_ms=%llu\n", plan.id.c_str(), spp,
                            static_cast<unsigned long long>(GetTickCount64() - start));
                snapshots.emplace_back(spp, std::move(image));
            }
        }
        if (deviceFailure) {
            break;
        }

        auto findSnapshot = [&](uint32_t spp) -> FloatImage* {
            for (auto& entry : snapshots) {
                if (entry.first == spp) {
                    return &entry.second;
                }
            }
            return nullptr;
        };
        FloatImage* image = findSnapshot(plan.imageSpp);
        if (image == nullptr && !snapshots.empty()) {
            image = &snapshots.back().second;
        }
        if (image == nullptr) {
            std::printf("ERROR no image for %s\n", plan.id.c_str());
            return 1;
        }
        result.image = *image;
        result.nanPixels = CountNan(result.image);
        result.shaRaw =
            Sha256Hex(result.image.rgb.data(), result.image.rgb.size() * sizeof(float));
        {
            const std::string pngPath =
                JoinPath(config.outDir, plan.id + "-" + std::to_string(plan.imageSpp) + "spp.png");
            std::vector<uint8_t> pngBytes;
            if (ReadFileBytes(pngPath, &pngBytes)) {
                result.shaPng = Sha256Hex(pngBytes.data(), pngBytes.size());
            }
        }

        {
            const uint32_t spp = snapshots.front().first;
            SceneRun rerun(renderer);
            FloatImage repeated;
            RunOptions options;
            if (rerun.Render(scene, spp, options, &repeated, &error)) {
                const std::string firstHash =
                    Sha256Hex(snapshots.front().second.rgb.data(),
                              snapshots.front().second.rgb.size() * sizeof(float));
                const std::string secondHash =
                    Sha256Hex(repeated.rgb.data(), repeated.rgb.size() * sizeof(float));
                result.determinism = (firstHash == secondHash);
            }
        }

        if (plan.refSpp > 0 && plan.refSpp != plan.imageSpp) {
            FloatImage* reference = findSnapshot(plan.refSpp);
            if (reference != nullptr) {
                result.mae = MeanAbsDiff(*image, *reference);
                result.p99 = P99Relative(*image, *reference, PatchFor(plan.fixture, scene.camera));
                // Sample scaling uses the largest rendered level below the
                // image level, so it is mae(512)/mae(1024), not a self-ratio.
                uint32_t previousSpp = 0;
                for (uint32_t level : plan.spp) {
                    if (level < plan.imageSpp && level > previousSpp) {
                        previousSpp = level;
                    }
                }
                FloatImage* previous = previousSpp > 0 ? findSnapshot(previousSpp) : nullptr;
                if (previous != nullptr) {
                    const double previousMae = MeanAbsDiff(*previous, *reference);
                    if (result.mae > 0.0) {
                        result.scaling = previousMae / result.mae;
                    }
                }
            }
        }

        if (plan.fixture == "calibration") {
            FloatImage* one = findSnapshot(1);
            if (one != nullptr) {
                result.aux = *one;
                result.hasAux = true;
            }
        }
        if (plan.fixture == "static") {
            FloatImage* raw = findSnapshot(64);
            if (raw != nullptr) {
                result.aux = *raw;
                result.hasAux = true;
            }
        }
        scenes.push_back(scene);
        results.push_back(std::move(result));
    }

    if (deviceFailure) {
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }

    // ------------------------------------------------------------------
    // Declared checks. Nothing here can turn a failure into a pass.
    // ------------------------------------------------------------------
    std::vector<CheckResult> checks(results.size());

    for (size_t i = 0; i < results.size(); ++i) {
        FrameResult& result = results[i];
        CheckResult& check = checks[i];
        check.Require(result.nanPixels == 0, "nan-pixels==0");
        check.Require(result.determinism, "deterministic-rerun");
        check.Note("nan=" + std::to_string(result.nanPixels));
        if (result.refSpp > 0 && result.refSpp != result.imageSpp) {
            check.Require(result.mae >= 0.0 && result.mae <= 0.01,
                          "mae<=0.01 (observed " + std::to_string(result.mae) + ")");
            check.Require(result.p99 >= 0.0 && result.p99 <= 0.05,
                          "p99-relative<=0.05 (observed " + std::to_string(result.p99) + ")");
            check.Require(result.scaling >= 1.5 && result.scaling <= 2.5,
                          "sample-scaling in [1.5,2.5] (observed " +
                              std::to_string(result.scaling) + ")");
            check.Note("mae=" + std::to_string(result.mae) +
                       " p99=" + std::to_string(result.p99) +
                       " scaling=" + std::to_string(result.scaling));
        }
    }

    // Calibration: mean and single-sample variance against the analytic value.
    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].fixture != "calibration" || !results[i].hasAux) {
            continue;
        }
        CheckResult& check = checks[i];
        const PixelRect whole = WholeImage(results[i].aux);
        const float albedo[3] = {0.6f, 0.4f, 0.2f};
        for (int c = 0; c < 3; ++c) {
            const double mean = MeanInRect(results[i].aux, whole, c);
            const double variance = VarianceInRect(results[i].aux, whole, c);
            const double expectedMean = albedo[c] * 0.5;
            const double expectedVariance = albedo[c] * albedo[c] * 0.25;
            check.Require(std::fabs(mean - expectedMean) <= 0.01,
                          "analytic mean channel " + std::to_string(c));
            check.Require(variance >= 0.0 &&
                              std::fabs(variance / expectedVariance - 1.0) <= 0.12,
                          "analytic single-sample variance channel " + std::to_string(c));
            check.Note("mean" + std::to_string(c) + "=" + std::to_string(mean) +
                       " (expected " + std::to_string(expectedMean) + ") var" +
                       std::to_string(c) + "=" + std::to_string(variance) + " (expected " +
                       std::to_string(expectedVariance) + ")");
        }
    }

    // Static: denoise the 64-spp image with the merged Q6 denoiser and compare
    // the flat-patch variance with the raw image.
    int staticIndex = -1;
    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].fixture == "static") {
            staticIndex = static_cast<int>(i);
        }
    }
    if (staticIndex >= 0 && results[staticIndex].hasAux) {
        FrameResult& result = results[staticIndex];
        // The camera is reconstructed from the same fixture parameters so the
        // patch projection is identical to the render camera.
        q6cap::Scene scene;
        std::string error;
        if (q6cap::BuildScene("static", result.width, result.height, 0, false, &scene, &error)) {
            const PixelRect patch = PatchFor("static", scene.camera);
            std::vector<float> denoised(result.aux.rgb.size(), 0.0f);
            doom3::rhi::DenoiseDesc desc;
            desc.width = result.aux.width;
            desc.height = result.aux.height;
            desc.radius = 2;
            desc.sigmaColor = 0.25f;
            desc.sigmaDepth = 0.5f;
            const bool denoisedOk =
                doom3::rhi::DenoiseBilateral(result.aux.rgb.data(), result.aux.depth.data(), desc,
                                             denoised.data(), &error);
            if (denoisedOk) {
                FloatImage denoisedImage = result.aux;
                denoisedImage.rgb = denoised;
                const std::string denoisedPath =
                    JoinPath(config.outDir, "02-static-64spp-denoised.png");
                if (!WriteImagePng(denoisedPath, denoisedImage, &error)) {
                    std::printf("ERROR denoised PNG: %s\n", error.c_str());
                }
                const double rawVariance = VarianceInRect(result.aux, patch, 0) +
                                           VarianceInRect(result.aux, patch, 1) +
                                           VarianceInRect(result.aux, patch, 2);
                const double denoisedVariance = VarianceInRect(denoisedImage, patch, 0) +
                                                VarianceInRect(denoisedImage, patch, 1) +
                                                VarianceInRect(denoisedImage, patch, 2);
                const double ratio = rawVariance > 0.0 ? denoisedVariance / rawVariance : -1.0;
                // Reported, not gated: this wall patch is geometrically flat
                // but carries a point-light shading gradient, so its variance
                // is not the rubric's flat-radiance field. The gated flat-field
                // measurement is the 00-flat-denoise calibration capture below.
                checks[staticIndex].Note("wall-patch denoise raw_var=" +
                                         std::to_string(rawVariance) +
                                         " denoised_var=" + std::to_string(denoisedVariance) +
                                         " ratio=" + std::to_string(ratio) +
                                         " (shaded patch; not the flat-field gate)");
            } else {
                checks[staticIndex].Require(false, "denoise failed: " + error);
            }
        }
    }

    // Flat-field noise: the calibration wall is uniform in radiance (fixed
    // normal, one hemisphere split), so its 64-spp variance is sampling noise,
    // not a shading gradient. This is the rubric's flat-variance gate.
    int flatIndex = -1;
    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].id == "00-flat-denoise") {
            flatIndex = static_cast<int>(i);
        }
    }
    if (flatIndex >= 0) {
        FrameResult& result = results[flatIndex];
        std::vector<float> denoised(result.image.rgb.size(), 0.0f);
        doom3::rhi::DenoiseDesc desc;
        desc.width = result.image.width;
        desc.height = result.image.height;
        desc.radius = 2;
        desc.sigmaColor = 0.25f;
        desc.sigmaDepth = 0.5f;
        std::string error;
        if (doom3::rhi::DenoiseBilateral(result.image.rgb.data(), result.image.depth.data(),
                                         desc, denoised.data(), &error)) {
            FloatImage denoisedImage = result.image;
            denoisedImage.rgb = denoised;
            const std::string path = JoinPath(config.outDir, "00-flat-denoise-64spp-denoised.png");
            if (!WriteImagePng(path, denoisedImage, &error)) {
                std::printf("ERROR flat denoised PNG: %s\n", error.c_str());
            }
            const PixelRect whole = WholeImage(result.image);
            const double rawVariance = VarianceInRect(result.image, whole, 0) +
                                       VarianceInRect(result.image, whole, 1) +
                                       VarianceInRect(result.image, whole, 2);
            const double denoisedVariance = VarianceInRect(denoisedImage, whole, 0) +
                                            VarianceInRect(denoisedImage, whole, 1) +
                                            VarianceInRect(denoisedImage, whole, 2);
            const double ratio = rawVariance > 0.0 ? denoisedVariance / rawVariance : -1.0;
            checks[flatIndex].Require(ratio >= 0.0 && ratio <= 0.25,
                                      "flat-variance denoised/raw<=0.25 (observed " +
                                          std::to_string(ratio) + ")");
            checks[flatIndex].Note("flat-field raw_var=" + std::to_string(rawVariance) +
                                   " denoised_var=" + std::to_string(denoisedVariance) +
                                   " ratio=" + std::to_string(ratio));
        } else {
            checks[flatIndex].Require(false, "denoise failed: " + error);
        }
    }

    // Rigid: the moving red box must shift the red mask centroid.
    int rigid0 = -1, rigid8 = -1;
    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].fixture == "rigid" && results[i].frame == 0) rigid0 = static_cast<int>(i);
        if (results[i].fixture == "rigid" && results[i].frame == 8) rigid8 = static_cast<int>(i);
    }
    if (rigid0 >= 0 && rigid8 >= 0) {
        const double centroid0 = CentroidX(results[rigid0].image, true);
        const double centroid8 = CentroidX(results[rigid8].image, true);
        const double shift = centroid8 - centroid0;
        const std::string detail = "red centroid frame0=" + std::to_string(centroid0) +
                                   " frame8=" + std::to_string(centroid8) +
                                   " shift=" + std::to_string(shift);
        checks[rigid0].Require(shift > 0.15 * results[rigid0].width,
                               "rigid centroid shift >15% width");
        checks[rigid8].Require(shift > 0.15 * results[rigid8].width,
                               "rigid centroid shift >15% width");
        checks[rigid0].Note(detail);
        checks[rigid8].Note(detail);
    }

    // Skinned: the vertex deformation must be real and the image must change.
    int skin0 = -1, skin6 = -1;
    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].fixture == "skinned" && results[i].frame == 0) skin0 = static_cast<int>(i);
        if (results[i].fixture == "skinned" && results[i].frame == 6) skin6 = static_cast<int>(i);
    }
    if (skin0 >= 0 && skin6 >= 0) {
        q6cap::Scene scene;
        std::string error;
        double displacement = -1.0;
        PixelRect sheetRect;
        if (q6cap::BuildScene("skinned", results[skin0].width, results[skin0].height, 6, false,
                              &scene, &error)) {
            displacement = MaxVertexDisplacement(scene);
            sheetRect = ProjectRect(scene.camera, q6cap::Vec3{0.0f, 1.2f, 1.3f},
                                    q6cap::Vec3{1, 0, 0}, q6cap::Vec3{0, 0, 1}, 1.0f, 1.0f);
        }
        const double diff =
            MeanAbsDiffRect(results[skin0].image, results[skin6].image, sheetRect);
        checks[skin0].Require(displacement > 0.15, "max vertex displacement >0.15");
        checks[skin6].Require(displacement > 0.15, "max vertex displacement >0.15");
        checks[skin0].Require(diff > 0.01, "frame image difference >0.01");
        checks[skin6].Require(diff > 0.01, "frame image difference >0.01");
        checks[skin0].Note("max vertex displacement=" + std::to_string(displacement) +
                           " mean image difference=" + std::to_string(diff));
        checks[skin6].Note("max vertex displacement=" + std::to_string(displacement) +
                           " mean image difference=" + std::to_string(diff));
    }

    // Alpha: HDR pixels visible through the cutout, and none with alpha off.
    int alphaOn = -1, alphaOff = -1;
    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].fixture == "alpha-emissive" && !results[i].alphaControl) {
            alphaOn = static_cast<int>(i);
        }
        if (results[i].fixture == "alpha-emissive" && results[i].alphaControl) {
            alphaOff = static_cast<int>(i);
        }
    }
    if (alphaOn >= 0 && alphaOff >= 0) {
        uint64_t onCount = 0, offCount = 0;
        HasHdrHoles(results[alphaOn].image, &onCount);
        HasHdrHoles(results[alphaOff].image, &offCount);
        checks[alphaOn].Require(onCount > 0, "HDR pixels visible through the cutout");
        checks[alphaOff].Require(offCount == 0, "no HDR pixels with alpha disabled");
        checks[alphaOn].Note("hdr hole pixels alpha-on=" + std::to_string(onCount) +
                             " alpha-off=" + std::to_string(offCount));
        checks[alphaOff].Note("hdr hole pixels alpha-on=" + std::to_string(onCount) +
                              " alpha-off=" + std::to_string(offCount));
    }

    // Transmission: the tint ratio through the tinted panel, and a clear panel
    // as the control. The backdrop is a constant 0.8 radiometric emitter.
    int transmission = -1;
    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].fixture == "transmission") {
            transmission = static_cast<int>(i);
        }
    }
    if (transmission >= 0) {
        q6cap::Scene scene;
        std::string error;
        if (q6cap::BuildScene("transmission", results[transmission].width,
                              results[transmission].height, 0, false, &scene, &error)) {
            const PixelRect tinted = ProjectRect(scene.camera, q6cap::Vec3{-0.8f, 2.0f, 1.3f},
                                                 q6cap::Vec3{1, 0, 0}, q6cap::Vec3{0, 0, 1},
                                                 0.35f, 0.5f);
            const PixelRect clear = ProjectRect(scene.camera, q6cap::Vec3{0.8f, 2.0f, 1.3f},
                                                q6cap::Vec3{1, 0, 0}, q6cap::Vec3{0, 0, 1},
                                                0.35f, 0.5f);
            const double tintedR = MeanInRect(results[transmission].image, tinted, 0);
            const double tintedG = MeanInRect(results[transmission].image, tinted, 1);
            const double tintedB = MeanInRect(results[transmission].image, tinted, 2);
            const double clearR = MeanInRect(results[transmission].image, clear, 0);
            const double clearG = MeanInRect(results[transmission].image, clear, 1);
            const double clearB = MeanInRect(results[transmission].image, clear, 2);
            const double tintedGr = tintedR > 0.0 ? tintedG / tintedR : -1.0;
            const double tintedBr = tintedR > 0.0 ? tintedB / tintedR : -1.0;
            const double clearGr = clearR > 0.0 ? clearG / clearR : -1.0;
            checks[transmission].Require(tintedGr >= 0.45 && tintedGr <= 0.65,
                                         "tinted G/R in [0.45,0.65] (observed " +
                                             std::to_string(tintedGr) + ")");
            checks[transmission].Require(tintedBr >= 0.25 && tintedBr <= 0.40,
                                         "tinted B/R in [0.25,0.40] (observed " +
                                             std::to_string(tintedBr) + ")");
            checks[transmission].Require(clearGr >= 0.90 && clearGr <= 1.10,
                                         "clear G/R in [0.90,1.10] (observed " +
                                             std::to_string(clearGr) + ")");
            checks[transmission].Note("tinted RGB=" + std::to_string(tintedR) + "," +
                                      std::to_string(tintedG) + "," + std::to_string(tintedB) +
                                      " clear RGB=" + std::to_string(clearR) + "," +
                                      std::to_string(clearG) + "," + std::to_string(clearB));
        }
    }

    // Timing: the static fixture at 640x360, one sample per dispatch.
    double timingMedian = -1.0;
    double timingP95 = -1.0;
    uint32_t timingPasses = 0;
    if (!config.skipTiming) {
        q6cap::Scene scene;
        std::string error;
        if (q6cap::BuildScene("static", 640, 360, 0, false, &scene, &error)) {
            SceneRun run(renderer);
            FloatImage image;
            RunOptions options;
            options.timing = true;
            if (run.Render(scene, 32, options, &image, &error)) {
                std::vector<double> samples = run.timestampsMs();
                if (!samples.empty()) {
                    std::sort(samples.begin(), samples.end());
                    timingMedian = samples[samples.size() / 2];
                    timingP95 = samples[static_cast<size_t>(std::floor(
                        0.95 * (samples.size() - 1)))];
                    timingPasses = static_cast<uint32_t>(samples.size());
                }
            } else {
                std::printf("NOTE timing run skipped: %s\n", error.c_str());
            }
        }
    }

    bool allAccepted = true;
    for (size_t i = 0; i < results.size(); ++i) {
        results[i].accepted = checks[i].ok;
        results[i].checks = checks[i].detail;
        if (!checks[i].reasons.empty()) {
            if (!results[i].checks.empty()) {
                results[i].checks += " | ";
            }
            results[i].checks += "FAILED: " + checks[i].reasons;
        }
        allAccepted = allAccepted && results[i].accepted;
        std::printf("VERDICT=%s accepted=%d checks=%s\n", results[i].id.c_str(),
                    results[i].accepted ? 1 : 0, results[i].checks.c_str());
    }

    const std::string reportPath = JoinPath(config.outDir, "report.json");
    WriteReport(reportPath, renderer, config, results,
                static_cast<uint32_t>(options5.RaytracingTier),
                TierName(options5.RaytracingTier), renderer.shaderModel.c_str(), timingMedian,
                timingP95, timingPasses, allAccepted);

    std::printf("TIMING_MEDIAN_MS=%.4f\n", timingMedian);
    std::printf("TIMING_P95_MS=%.4f\n", timingP95);
    std::printf("TIMING_PASSES=%u\n", timingPasses);
    std::printf("INPUT_LATENCY=NOT RUN (no window or input path)\n");
    std::printf("RESULT=%s\n", allAccepted ? "PASS" : "FAIL");
    std::printf("EXIT_CODE=%d\n", allAccepted ? 0 : 3);
    return allAccepted ? 0 : 3;
}
