// Q6 DXR capability probe (spec 004 T069, issue #68).
//
// This is evidence tooling, not a renderer and not a Q6 engine frame. It
// reports the selected adapter, its LUID and UMD driver version, the real
// D3D12_FEATURE_D3D12_OPTIONS5.RaytracingTier, whether a DXR device and
// acceleration structures can be created, and the result of a trivial
// DispatchRays (8x8 camera rays at one triangle, read back from a UAV).
//
// Exit codes:
//   0  device + tier + acceleration structures + dispatch all executed
//   2  device or ray-tracing tier unsupported; no dispatch attempted
//   1  the probe itself failed (unexpected HRESULT or wrong readback)
//
// Every result line is printed as KEY=VALUE so the evidence file can quote it.

#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {

constexpr UINT kDispatchWidth = 8;
constexpr UINT kDispatchHeight = 8;
constexpr UINT kReadbackRowPitch = 256;
constexpr UINT64 kFenceTimeoutMs = 30000;

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

std::string HrText(HRESULT hr) {
    char buffer[32] = {};
    sprintf_s(buffer, "0x%08X", static_cast<unsigned int>(hr));
    return std::string(buffer);
}

bool Check(HRESULT hr, const char* what) {
    if (FAILED(hr)) {
        std::printf("%s failed: %s\n", what, HrText(hr).c_str());
        return false;
    }
    return true;
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

ComPtr<ID3D12Resource> CreateBuffer(ID3D12Device* device, UINT64 size,
                                    D3D12_RESOURCE_FLAGS flags,
                                    D3D12_RESOURCE_STATES initialState,
                                    D3D12_HEAP_TYPE heapType) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = heapType;
    heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap.CreationNodeMask = 0;
    heap.VisibleNodeMask = 0;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = flags;

    ComPtr<ID3D12Resource> resource;
    const HRESULT hr = device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc, initialState, nullptr,
        IID_PPV_ARGS(&resource));
    if (FAILED(hr)) {
        std::printf("CreateCommittedResource(%llu bytes) failed: %s\n",
                    static_cast<unsigned long long>(size), HrText(hr).c_str());
        return nullptr;
    }
    return resource;
}

struct AccelerationStructure {
    ComPtr<ID3D12Resource> resource;
    ComPtr<ID3D12Resource> scratch;
    // Vertex or instance upload data must outlive the command list that reads
    // it; otherwise a build recorded here would read freed memory on execute.
    ComPtr<ID3D12Resource> inputBuffer;
    UINT64 size = 0;
};

bool CreateBottomLevel(ID3D12Device5* device5, ID3D12GraphicsCommandList4* commandList,
                       AccelerationStructure* out) {
    const float vertices[9] = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };

    ComPtr<ID3D12Resource> vertexBuffer = CreateBuffer(
        device5, sizeof(vertices), D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
    if (!vertexBuffer) {
        return false;
    }
    D3D12_RANGE writeRange{0, sizeof(vertices)};
    void* mapped = nullptr;
    if (!Check(vertexBuffer->Map(0, &writeRange, &mapped), "Map(vertices)")) {
        return false;
    }
    std::memcpy(mapped, vertices, sizeof(vertices));
    vertexBuffer->Unmap(0, nullptr);
    out->inputBuffer = vertexBuffer;

    D3D12_RAYTRACING_GEOMETRY_DESC geometry{};
    geometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometry.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    geometry.Triangles.VertexBuffer.StartAddress =
        vertexBuffer->GetGPUVirtualAddress();
    geometry.Triangles.VertexBuffer.StrideInBytes = sizeof(float) * 3;
    geometry.Triangles.VertexCount = 3;
    geometry.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometry.Triangles.IndexBuffer = 0;
    geometry.Triangles.IndexCount = 0;
    geometry.Triangles.IndexFormat = DXGI_FORMAT_UNKNOWN;
    geometry.Triangles.Transform3x4 = 0;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.NumDescs = 1;
    inputs.pGeometryDescs = &geometry;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild{};
    device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);
    std::printf("BLAS_PREBUILD_RESULT_BYTES=%llu\n",
                static_cast<unsigned long long>(prebuild.ResultDataMaxSizeInBytes));
    if (prebuild.ResultDataMaxSizeInBytes == 0) {
        std::printf("BLAS prebuild size is zero\n");
        return false;
    }

    out->size = prebuild.ResultDataMaxSizeInBytes;
    out->resource = CreateBuffer(
        device5, prebuild.ResultDataMaxSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        D3D12_HEAP_TYPE_DEFAULT);
    out->scratch = CreateBuffer(
        device5, prebuild.ScratchDataSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT);
    if (!out->resource || !out->scratch) {
        return false;
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
    build.Inputs = inputs;
    build.ScratchAccelerationStructureData = out->scratch->GetGPUVirtualAddress();
    build.DestAccelerationStructureData = out->resource->GetGPUVirtualAddress();
    build.SourceAccelerationStructureData = 0;
    commandList->BuildRaytracingAccelerationStructure(&build, 0, nullptr);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = out->resource.Get();
    commandList->ResourceBarrier(1, &barrier);
    return true;
}

bool CreateTopLevel(ID3D12Device5* device5, ID3D12GraphicsCommandList4* commandList,
                    const AccelerationStructure& bottomLevel,
                    AccelerationStructure* out) {
    D3D12_RAYTRACING_INSTANCE_DESC instance{};
    instance.Transform[0][0] = 1.0f;
    instance.Transform[1][1] = 1.0f;
    instance.Transform[2][2] = 1.0f;
    instance.InstanceID = 0;
    instance.InstanceContributionToHitGroupIndex = 0;
    instance.InstanceMask = 0xFF;
    instance.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
    instance.AccelerationStructure = bottomLevel.resource->GetGPUVirtualAddress();

    ComPtr<ID3D12Resource> instanceBuffer = CreateBuffer(
        device5, sizeof(instance), D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
    if (!instanceBuffer) {
        return false;
    }
    D3D12_RANGE writeRange{0, sizeof(instance)};
    void* mapped = nullptr;
    if (!Check(instanceBuffer->Map(0, &writeRange, &mapped), "Map(instance)")) {
        return false;
    }
    std::memcpy(mapped, &instance, sizeof(instance));
    instanceBuffer->Unmap(0, nullptr);
    out->inputBuffer = instanceBuffer;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.NumDescs = 1;
    inputs.InstanceDescs = 0;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild{};
    device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);
    std::printf("TLAS_PREBUILD_RESULT_BYTES=%llu\n",
                static_cast<unsigned long long>(prebuild.ResultDataMaxSizeInBytes));
    if (prebuild.ResultDataMaxSizeInBytes == 0) {
        std::printf("TLAS prebuild size is zero\n");
        return false;
    }

    out->size = prebuild.ResultDataMaxSizeInBytes;
    out->resource = CreateBuffer(
        device5, prebuild.ResultDataMaxSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        D3D12_HEAP_TYPE_DEFAULT);
    out->scratch = CreateBuffer(
        device5, prebuild.ScratchDataSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT);
    if (!out->resource || !out->scratch) {
        return false;
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
    build.Inputs = inputs;
    build.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    build.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    build.Inputs.NumDescs = 1;
    build.Inputs.InstanceDescs = 0;
    build.Inputs.InstanceDescs = instanceBuffer->GetGPUVirtualAddress();
    build.ScratchAccelerationStructureData = out->scratch->GetGPUVirtualAddress();
    build.DestAccelerationStructureData = out->resource->GetGPUVirtualAddress();
    build.SourceAccelerationStructureData = 0;
    commandList->BuildRaytracingAccelerationStructure(&build, 0, nullptr);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = out->resource.Get();
    commandList->ResourceBarrier(1, &barrier);
    return true;
}

bool CreateRootSignature(ID3D12Device5* device5, ID3D12RootSignature** out) {
    // Descriptor tables, not root descriptors: the output is a typed UAV, and
    // D3D12 root UAV/SRV descriptors accept only raw or structured buffers.
    D3D12_DESCRIPTOR_RANGE ranges[2] = {};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 1;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RegisterSpace = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = 0;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].NumDescriptors = 1;
    ranges[1].BaseShaderRegister = 0;
    ranges[1].RegisterSpace = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER parameters[2] = {};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable.NumDescriptorRanges = 1;
    parameters[0].DescriptorTable.pDescriptorRanges = &ranges[0];
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable.NumDescriptorRanges = 1;
    parameters[1].DescriptorTable.pDescriptorRanges = &ranges[1];
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = 2;
    desc.pParameters = parameters;
    desc.NumStaticSamplers = 0;
    desc.pStaticSamplers = nullptr;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(
        &desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hr)) {
        std::printf("D3D12SerializeRootSignature failed: %s %s\n", HrText(hr).c_str(),
                    error ? static_cast<const char*>(error->GetBufferPointer()) : "");
        return false;
    }
    hr = device5->CreateRootSignature(0, signature->GetBufferPointer(),
                                      signature->GetBufferSize(),
                                      IID_PPV_ARGS(out));
    return Check(hr, "CreateRootSignature");
}

bool CreatePipeline(ID3D12Device5* device5, ID3D12RootSignature* rootSignature,
                    const std::vector<uint8_t>& shaderBytes,
                    ID3D12StateObject** stateObject,
                    ID3D12StateObjectProperties** properties) {
    D3D12_DXIL_LIBRARY_DESC library{};
    library.DXILLibrary.pShaderBytecode = shaderBytes.data();
    library.DXILLibrary.BytecodeLength = shaderBytes.size();

    D3D12_HIT_GROUP_DESC hitGroup{};
    hitGroup.HitGroupExport = L"hitgroup";
    hitGroup.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
    hitGroup.AnyHitShaderImport = nullptr;
    hitGroup.ClosestHitShaderImport = L"MyClosestHit";
    hitGroup.IntersectionShaderImport = nullptr;

    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig{};
    shaderConfig.MaxPayloadSizeInBytes = 16;
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

    const HRESULT hr = device5->CreateStateObject(&desc, IID_PPV_ARGS(stateObject));
    if (FAILED(hr)) {
        std::printf("CreateStateObject failed: %s\n", HrText(hr).c_str());
        return false;
    }
    return Check(stateObject[0]->QueryInterface(
                     IID_PPV_ARGS(properties)),
                 "QueryInterface(ID3D12StateObjectProperties)");
}

void PrintStoredMessages(ID3D12InfoQueue* infoQueue) {
    if (infoQueue == nullptr) {
        std::printf("DEBUG_LAYER_MESSAGES=unavailable\n");
        return;
    }
    const UINT64 stored = infoQueue->GetNumStoredMessages();
    UINT64 errors = 0;
    UINT64 warnings = 0;
    for (UINT64 i = 0; i < stored; ++i) {
        SIZE_T length = 0;
        if (FAILED(infoQueue->GetMessage(i, nullptr, &length)) || length == 0) {
            continue;
        }
        std::vector<char> buffer(length);
        D3D12_MESSAGE* message = reinterpret_cast<D3D12_MESSAGE*>(buffer.data());
        if (FAILED(infoQueue->GetMessage(i, message, &length))) {
            continue;
        }
        if (message->Severity == D3D12_MESSAGE_SEVERITY_ERROR ||
            message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION) {
            ++errors;
            if (errors <= 3) {
                std::printf("DEBUG_ERROR=%s\n", message->pDescription);
            }
        } else if (message->Severity == D3D12_MESSAGE_SEVERITY_WARNING) {
            ++warnings;
            if (warnings <= 3) {
                std::printf("DEBUG_WARNING=%s\n", message->pDescription);
            }
        }
    }
    std::printf("DEBUG_LAYER_MESSAGES=%llu\n",
                static_cast<unsigned long long>(stored));
    std::printf("DEBUG_ERRORS=%llu\n", static_cast<unsigned long long>(errors));
    std::printf("DEBUG_WARNINGS=%llu\n", static_cast<unsigned long long>(warnings));
}

}  // namespace

int main() {
    std::printf("PROBE=q6_dxr_capability_dispatch\n");

    ComPtr<IDXGIFactory6> factory;
    if (!Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2")) {
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }

    struct AdapterInfo {
        ComPtr<IDXGIAdapter1> adapter;
        DXGI_ADAPTER_DESC1 desc{};
        std::string name;
        int index = -1;
        bool software = false;
    };
    std::vector<AdapterInfo> adapters;
    for (UINT index = 0;; ++index) {
        ComPtr<IDXGIAdapter1> adapter;
        const HRESULT hr = factory->EnumAdapters1(index, &adapter);
        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (FAILED(hr)) {
            std::printf("EnumAdapters1(%u) failed: %s\n", index, HrText(hr).c_str());
            break;
        }
        AdapterInfo info;
        info.adapter = adapter;
        info.index = static_cast<int>(index);
        adapter->GetDesc1(&info.desc);
        info.name = Narrow(info.desc.Description);
        info.software = (info.desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
        adapters.push_back(info);
    }

    for (const AdapterInfo& info : adapters) {
        std::printf("ADAPTER[%d]=%s\n", info.index, info.name.c_str());
        std::printf("ADAPTER[%d].LUID=0x%08lX%08lX\n", info.index,
                    static_cast<unsigned long>(info.desc.AdapterLuid.HighPart),
                    static_cast<unsigned long>(info.desc.AdapterLuid.LowPart));
        std::printf("ADAPTER[%d].VENDOR_ID=0x%04X DEVICE_ID=0x%04X\n", info.index,
                    info.desc.VendorId, info.desc.DeviceId);
        std::printf("ADAPTER[%d].DEDICATED_VRAM_MB=%llu SOFTWARE=%d\n", info.index,
                    static_cast<unsigned long long>(info.desc.DedicatedVideoMemory / (1024 * 1024)),
                    info.software ? 1 : 0);
    }
    std::printf("ADAPTER_COUNT=%d\n", static_cast<int>(adapters.size()));
    if (adapters.empty()) {
        std::printf("RESULT=UNSUPPORTED\nEXIT_CODE=2\n");
        return 2;
    }

    // Prefer the named discrete adapter when present, else the non-software
    // adapter with the most dedicated VRAM. The name is an identifier only;
    // the tier below is always the queried value, never inferred from it.
    int selected = -1;
    for (size_t i = 0; i < adapters.size(); ++i) {
        if (adapters[i].name.find("9070 XT") != std::string::npos) {
            selected = static_cast<int>(i);
            break;
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

    const AdapterInfo& chosen = adapters[static_cast<size_t>(selected)];
    std::printf("SELECTED_ADAPTER_INDEX=%d\n", chosen.index);
    std::printf("SELECTED_ADAPTER_NAME=%s\n", chosen.name.c_str());
    std::printf("SELECTED_ADAPTER_LUID=0x%08lX%08lX\n",
                static_cast<unsigned long>(chosen.desc.AdapterLuid.HighPart),
                static_cast<unsigned long>(chosen.desc.AdapterLuid.LowPart));

    LARGE_INTEGER umdVersion{};
    const HRESULT driverHr = chosen.adapter->CheckInterfaceSupport(
        __uuidof(IDXGIDevice), &umdVersion);
    if (SUCCEEDED(driverHr)) {
        const unsigned int a = static_cast<unsigned int>((umdVersion.HighPart >> 16) & 0xFFFF);
        const unsigned int b = static_cast<unsigned int>(umdVersion.HighPart & 0xFFFF);
        const unsigned int c = static_cast<unsigned int>((umdVersion.LowPart >> 16) & 0xFFFF);
        const unsigned int d = static_cast<unsigned int>(umdVersion.LowPart & 0xFFFF);
        std::printf("DRIVER_UMD=%u.%u.%u.%u\n", a, b, c, d);
    } else {
        std::printf("DRIVER_UMD=unreported (%s)\n", HrText(driverHr).c_str());
    }

    // Enabling the debug layer is optional; when the Graphics Tools feature is
    // absent the interface is unavailable and the probe continues.
    ComPtr<ID3D12Debug> debugController;
    bool debugLayerEnabled = false;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugLayerEnabled = true;
    }
    std::printf("DEBUG_LAYER_ENABLED=%d\n", debugLayerEnabled ? 1 : 0);

    ComPtr<ID3D12Device> device;
    const HRESULT deviceHr = D3D12CreateDevice(
        chosen.adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(deviceHr)) {
        std::printf("D3D12CreateDevice failed: %s\n", HrText(deviceHr).c_str());
        std::printf("DEVICE_CREATED=0\nRESULT=UNSUPPORTED\nEXIT_CODE=2\n");
        return 2;
    }
    std::printf("DEVICE_CREATED=1\n");

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5{};
    const HRESULT optionsHr = device->CheckFeatureSupport(
        D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));
    if (FAILED(optionsHr)) {
        std::printf("CheckFeatureSupport(OPTIONS5) failed: %s\n", HrText(optionsHr).c_str());
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }
    std::printf("RAYTRACING_TIER=%s (%d)\n", TierName(options5.RaytracingTier),
                static_cast<int>(options5.RaytracingTier));
    std::printf("SRV_ONLY_TILED_TIER3=%d\n",
                static_cast<int>(options5.SRVOnlyTiledResourceTier3));
    std::printf("RENDER_PASSES_TIER=%d\n",
                static_cast<int>(options5.RenderPassesTier));

    D3D_SHADER_MODEL shaderModel = D3D_SHADER_MODEL_6_0;
    for (D3D_SHADER_MODEL candidate :
         {D3D_SHADER_MODEL_6_8, D3D_SHADER_MODEL_6_7, D3D_SHADER_MODEL_6_6,
          D3D_SHADER_MODEL_6_5, D3D_SHADER_MODEL_6_4, D3D_SHADER_MODEL_6_3,
          D3D_SHADER_MODEL_6_2, D3D_SHADER_MODEL_6_1, D3D_SHADER_MODEL_6_0}) {
        D3D12_FEATURE_DATA_SHADER_MODEL data{candidate};
        if (SUCCEEDED(device->CheckFeatureSupport(
                D3D12_FEATURE_SHADER_MODEL, &data, sizeof(data)))) {
            shaderModel = data.HighestShaderModel;
            break;
        }
    }
    std::printf("SHADER_MODEL=%s\n", ShaderModelName(shaderModel));

    if (options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
        std::printf("DXR_DEVICE_CREATED=0\n");
        std::printf("RESULT=UNSUPPORTED\nEXIT_CODE=2\n");
        return 2;
    }

    ComPtr<ID3D12Device5> device5;
    if (!Check(device.As(&device5), "QueryInterface(ID3D12Device5)")) {
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }
    std::printf("DXR_DEVICE_CREATED=1\n");

    ComPtr<ID3D12InfoQueue> infoQueue;
    if (debugLayerEnabled) {
        device.As(&infoQueue);
    }

    ComPtr<ID3D12CommandQueue> queue;
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    if (!Check(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)),
               "CreateCommandQueue")) {
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }

    UINT64 timestampFrequency = 0;
    queue->GetTimestampFrequency(&timestampFrequency);

    ComPtr<ID3D12CommandAllocator> allocator;
    if (!Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                              IID_PPV_ARGS(&allocator)),
               "CreateCommandAllocator")) {
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }

    ComPtr<ID3D12GraphicsCommandList4> commandList;
    if (!Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         allocator.Get(), nullptr,
                                         IID_PPV_ARGS(&commandList)),
               "CreateCommandList")) {
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }

    ComPtr<ID3D12Fence> fence;
    if (!Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)),
               "CreateFence")) {
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }
    HANDLE fenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    if (fenceEvent == nullptr) {
        std::printf("CreateEventEx failed\nRESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }

    const auto waitForGpu = [&](UINT64 value) -> bool {
        if (!Check(queue->Signal(fence.Get(), value), "Signal")) {
            return false;
        }
        if (!Check(fence->SetEventOnCompletion(value, fenceEvent),
                   "SetEventOnCompletion")) {
            return false;
        }
        return WaitForSingleObject(fenceEvent, kFenceTimeoutMs) == WAIT_OBJECT_0;
    };

    AccelerationStructure bottomLevel;
    AccelerationStructure topLevel;
    if (!CreateBottomLevel(device5.Get(), commandList.Get(), &bottomLevel)) {
        std::printf("BLAS_CREATED=0\nRESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }
    if (!CreateTopLevel(device5.Get(), commandList.Get(), bottomLevel, &topLevel)) {
        std::printf("TLAS_CREATED=0\nRESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }
    std::printf("BLAS_CREATED=1\nTLAS_CREATED=1\n");

    ComPtr<ID3D12RootSignature> rootSignature;
    if (!CreateRootSignature(device5.Get(), &rootSignature)) {
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }

    std::vector<uint8_t> shaderBytes;
    const std::string shaderPath = ExeDirectory() + "\\dxr_trivial.cso";
    if (!ReadFileBytes(shaderPath, &shaderBytes)) {
        std::printf("Could not read %s\n", shaderPath.c_str());
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }
    std::printf("SHADER_BYTES=%llu\n",
                static_cast<unsigned long long>(shaderBytes.size()));

    ComPtr<ID3D12StateObject> stateObject;
    ComPtr<ID3D12StateObjectProperties> stateProperties;
    if (!CreatePipeline(device5.Get(), rootSignature.Get(), shaderBytes,
                        &stateObject, &stateProperties)) {
        PrintStoredMessages(infoQueue.Get());
        std::printf("STATE_OBJECT_CREATED=0\nRESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }
    std::printf("STATE_OBJECT_CREATED=1\n");

    struct ShaderIdentifier {
        const char* name;
        const void* data;
    };
    const ShaderIdentifier identifiers[3] = {
        {"MyRayGen", stateProperties->GetShaderIdentifier(L"MyRayGen")},
        {"MyMiss", stateProperties->GetShaderIdentifier(L"MyMiss")},
        {"hitgroup", stateProperties->GetShaderIdentifier(L"hitgroup")},
    };
    bool identifiersOk = true;
    for (const ShaderIdentifier& identifier : identifiers) {
        if (identifier.data == nullptr) {
            std::printf("Missing shader identifier: %s\n", identifier.name);
            identifiersOk = false;
        }
    }
    if (!identifiersOk) {
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }

    ComPtr<ID3D12Resource> shaderBindingTable = CreateBuffer(
        device5.Get(), 256, D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
    if (!shaderBindingTable) {
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }
    D3D12_RANGE sbtRange{0, 256};
    void* sbtMapped = nullptr;
    if (!Check(shaderBindingTable->Map(0, &sbtRange, &sbtMapped), "Map(SBT)")) {
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }
    std::memset(sbtMapped, 0, 256);
    std::memcpy(static_cast<uint8_t*>(sbtMapped) + 0, identifiers[0].data,
                D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    std::memcpy(static_cast<uint8_t*>(sbtMapped) + 64, identifiers[1].data,
                D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    std::memcpy(static_cast<uint8_t*>(sbtMapped) + 128, identifiers[2].data,
                D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    shaderBindingTable->Unmap(0, nullptr);
    const D3D12_GPU_VIRTUAL_ADDRESS sbtAddress =
        shaderBindingTable->GetGPUVirtualAddress();
    std::printf("SBT_CREATED=1\n");

    D3D12_HEAP_PROPERTIES textureHeap{};
    textureHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC textureDesc{};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = kDispatchWidth;
    textureDesc.Height = kDispatchHeight;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    ComPtr<ID3D12Resource> outputTexture;
    if (!Check(device->CreateCommittedResource(
                   &textureHeap, D3D12_HEAP_FLAG_NONE, &textureDesc,
                   D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                   IID_PPV_ARGS(&outputTexture)),
               "CreateCommittedResource(output texture)")) {
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 2;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    if (!Check(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap)),
               "CreateDescriptorHeap")) {
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }
    const UINT descriptorSize =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle =
        descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle =
        descriptorHeap->GetGPUDescriptorHandleForHeapStart();

    D3D12_SHADER_RESOURCE_VIEW_DESC accelerationView{};
    accelerationView.Format = DXGI_FORMAT_UNKNOWN;
    accelerationView.ViewDimension =
        D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    accelerationView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    accelerationView.RaytracingAccelerationStructure.Location =
        topLevel.resource->GetGPUVirtualAddress();
    device->CreateShaderResourceView(nullptr, &accelerationView, cpuHandle);
    const D3D12_GPU_DESCRIPTOR_HANDLE accelerationGpuHandle = gpuHandle;

    cpuHandle.ptr += descriptorSize;
    gpuHandle.ptr += descriptorSize;
    D3D12_UNORDERED_ACCESS_VIEW_DESC outputView{};
    outputView.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    outputView.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    outputView.Texture2D.MipSlice = 0;
    outputView.Texture2D.PlaneSlice = 0;
    device->CreateUnorderedAccessView(outputTexture.Get(), nullptr, &outputView, cpuHandle);
    const D3D12_GPU_DESCRIPTOR_HANDLE outputGpuHandle = gpuHandle;
    std::printf("DESCRIPTOR_HEAP_CREATED=1\n");

    ComPtr<ID3D12Resource> readback = CreateBuffer(
        device5.Get(), kReadbackRowPitch * kDispatchHeight,
        D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_HEAP_TYPE_READBACK);
    ComPtr<ID3D12Resource> timestampReadback = CreateBuffer(
        device5.Get(), sizeof(UINT64) * 2, D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK);
    if (!readback || !timestampReadback) {
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }

    D3D12_QUERY_HEAP_DESC queryHeapDesc{};
    queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    queryHeapDesc.Count = 2;
    ComPtr<ID3D12QueryHeap> queryHeap;
    if (!Check(device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&queryHeap)),
               "CreateQueryHeap")) {
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }

    // The list is still recording: the BLAS/TLAS builds above are recorded and
    // will execute in the same submission as the dispatch, separated by the UAV
    // barriers already inserted. Resetting here would silently drop them.
    ID3D12DescriptorHeap* heaps[1] = {descriptorHeap.Get()};
    commandList->SetDescriptorHeaps(1, heaps);
    commandList->SetComputeRootSignature(rootSignature.Get());
    commandList->SetComputeRootDescriptorTable(0, accelerationGpuHandle);
    commandList->SetComputeRootDescriptorTable(1, outputGpuHandle);
    commandList->SetPipelineState1(stateObject.Get());

    commandList->EndQuery(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
    D3D12_DISPATCH_RAYS_DESC dispatch{};
    dispatch.RayGenerationShaderRecord.StartAddress = sbtAddress + 0;
    dispatch.RayGenerationShaderRecord.SizeInBytes =
        D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    dispatch.MissShaderTable.StartAddress = sbtAddress + 64;
    dispatch.MissShaderTable.SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    dispatch.MissShaderTable.StrideInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    dispatch.HitGroupTable.StartAddress = sbtAddress + 128;
    dispatch.HitGroupTable.SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    dispatch.HitGroupTable.StrideInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    dispatch.CallableShaderTable.StartAddress = 0;
    dispatch.CallableShaderTable.SizeInBytes = 0;
    dispatch.CallableShaderTable.StrideInBytes = 0;
    dispatch.Width = kDispatchWidth;
    dispatch.Height = kDispatchHeight;
    dispatch.Depth = 1;
    commandList->DispatchRays(&dispatch);
    commandList->EndQuery(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);

    D3D12_RESOURCE_BARRIER toCopy{};
    toCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toCopy.Transition.pResource = outputTexture.Get();
    toCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    toCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    toCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    commandList->ResourceBarrier(1, &toCopy);

    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = outputTexture.Get();
    source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    source.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource = readback.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    destination.PlacedFootprint.Offset = 0;
    destination.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    destination.PlacedFootprint.Footprint.Width = kDispatchWidth;
    destination.PlacedFootprint.Footprint.Height = kDispatchHeight;
    destination.PlacedFootprint.Footprint.Depth = 1;
    destination.PlacedFootprint.Footprint.RowPitch = kReadbackRowPitch;
    commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    commandList->ResolveQueryData(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
                                  0, 2, timestampReadback.Get(), 0);

    if (!Check(commandList->Close(), "Close")) {
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }
    ID3D12CommandList* lists[1] = {commandList.Get()};
    queue->ExecuteCommandLists(1, lists);
    if (!waitForGpu(1)) {
        std::printf("GPU wait failed\nRESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }
    std::printf("DISPATCH_EXECUTED=1\n");

    UINT64 timestamps[2] = {};
    D3D12_RANGE timestampRange{0, sizeof(timestamps)};
    void* timestampMapped = nullptr;
    if (SUCCEEDED(timestampReadback->Map(0, &timestampRange, &timestampMapped))) {
        std::memcpy(timestamps, timestampMapped, sizeof(timestamps));
        timestampReadback->Unmap(0, nullptr);
        if (timestampFrequency > 0 && timestamps[1] > timestamps[0]) {
            const double ms = static_cast<double>(timestamps[1] - timestamps[0]) *
                              1000.0 / static_cast<double>(timestampFrequency);
            std::printf("DISPATCH_GPU_MS=%.6f\n", ms);
            std::printf("TIMESTAMP_FREQUENCY=%llu\n",
                        static_cast<unsigned long long>(timestampFrequency));
        }
    }

    UINT32 hitPixels = 0;
    D3D12_RANGE readRange{0, kReadbackRowPitch * kDispatchHeight};
    void* mappedReadback = nullptr;
    if (!Check(readback->Map(0, &readRange, &mappedReadback), "Map(readback)")) {
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }
    for (UINT y = 0; y < kDispatchHeight; ++y) {
        const uint8_t* row = static_cast<const uint8_t*>(mappedReadback) +
                             static_cast<size_t>(y) * kReadbackRowPitch;
        for (UINT x = 0; x < kDispatchWidth; ++x) {
            const uint8_t* pixel = row + x * 4;
            if (pixel[1] > 200 && pixel[0] < 60) {
                ++hitPixels;
            }
        }
    }
    D3D12_RANGE noWrite{0, 0};
    readback->Unmap(0, &noWrite);
    std::printf("DISPATCH_HIT_PIXELS=%u/%u\n", hitPixels, kDispatchWidth * kDispatchHeight);

    PrintStoredMessages(infoQueue.Get());

    if (hitPixels != kDispatchWidth * kDispatchHeight) {
        std::printf("RESULT=ERROR\nEXIT_CODE=1\n");
        return 1;
    }
    std::printf("RESULT=PASS\nEXIT_CODE=0\n");
    return 0;
}
