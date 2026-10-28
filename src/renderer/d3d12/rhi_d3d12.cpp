#include "d3d12/d3d12_private.h"

#include <windows.h>

#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace doom3 {
namespace rhi {

namespace {

constexpr DWORD kShutdownTimeoutMilliseconds = 30000;
constexpr size_t kMaxCapturedMessages = 8;
constexpr uint64_t kRemovedSentinel = std::numeric_limits<uint64_t>::max();

// Process-lifetime, non-recycling IDs prevent aliases across devices and
// reinitializations. Saturation is permanent rather than wrapping to an old ID.
std::atomic<uint64_t> gNextSubmissionId{1};
std::atomic<uint64_t> gNextCompletionToken{1};

bool AllocateSubmissionId(uint32_t* id) {
    uint64_t next = gNextSubmissionId.load(std::memory_order_relaxed);
    while (next <= std::numeric_limits<uint32_t>::max()) {
        if (gNextSubmissionId.compare_exchange_weak(
                next, next + 1, std::memory_order_relaxed)) {
            *id = static_cast<uint32_t>(next);
            return true;
        }
    }
    return false;
}

bool AllocateCompletionToken(uint64_t* token) {
    uint64_t next = gNextCompletionToken.load(std::memory_order_relaxed);
    while (next != 0) {
        if (gNextCompletionToken.compare_exchange_weak(
                next, next + 1, std::memory_order_relaxed)) {
            *token = next;
            return true;
        }
    }
    return false;
}

const char* SeverityName(D3D12_MESSAGE_SEVERITY severity) {
    switch (severity) {
        case D3D12_MESSAGE_SEVERITY_CORRUPTION:
            return "CORRUPTION";
        case D3D12_MESSAGE_SEVERITY_ERROR:
            return "ERROR";
        case D3D12_MESSAGE_SEVERITY_WARNING:
            return "WARNING";
        case D3D12_MESSAGE_SEVERITY_INFO:
            return "INFO";
        case D3D12_MESSAGE_SEVERITY_MESSAGE:
            return "MESSAGE";
    }
    return "UNKNOWN";
}

std::string HrString(HRESULT hr) {
    char buffer[64] = {};
    std::snprintf(buffer, sizeof(buffer), "hr=0x%08lX",
                  static_cast<unsigned long>(static_cast<uint32_t>(hr)));
    return buffer;
}

std::string ErrorWith(const char* operation, HRESULT hr) {
    return std::string(operation) + " failed (" + HrString(hr) + ")";
}

Status Fail(Status status, std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
    return status;
}

std::string WideToUtf8(const wchar_t* value) {
    if (value == nullptr || *value == L'\0') {
        return {};
    }
    const int length =
        WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) {
        return {};
    }
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, &result[0], length, nullptr, nullptr);
    result.resize(static_cast<size_t>(length - 1));
    return result;
}

constexpr D3D12_RESOURCE_STATES kTextureInitialState =
    D3D12_RESOURCE_STATE_RENDER_TARGET;
constexpr D3D12_RESOURCE_STATES kReadbackInitialState = D3D12_RESOURCE_STATE_COPY_DEST;

const char* PrivateStateName(D3D12_RESOURCE_STATES state) {
    switch (state) {
        case D3D12_RESOURCE_STATE_COMMON:
            return "COMMON";
        case D3D12_RESOURCE_STATE_RENDER_TARGET:
            return "RENDER_TARGET";
        case D3D12_RESOURCE_STATE_COPY_SOURCE:
            return "COPY_SOURCE";
        case D3D12_RESOURCE_STATE_COPY_DEST:
            return "COPY_DEST";
        default:
            return "UNKNOWN";
    }
}

// One submission owns everything the GPU may still reference. Nothing in this
// record is destroyed until the completion value proves the last use ended, or
// the whole record is moved to process-lifetime quarantine.
struct Q1aRecord {
    uint32_t id = 0;
    uint64_t token = 0;
    uint64_t serial = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t rowPitch = 0;
    uint32_t tightRowStride = 0;
    uint64_t readbackBytes = 0;
    bool gated = false;
    bool gateReleased = false;
    bool signalled = false;
    bool released = false;
    std::vector<d3d12::JournalEntry> journal;

    ComPtr<ID3D12Fence> gateFence;
    ComPtr<ID3D12Resource> texture;
    ComPtr<ID3D12Resource> readback;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;
};

// Deliberately never destroyed. If completion cannot be proved, dropping these
// objects could free memory the GPU still reads. A process-lifetime quarantine
// is the honest alternative to a clean-looking teardown.
struct QuarantineStore {
    std::vector<Q1aRecord> records;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandQueue> gateQueue;
    ComPtr<ID3D12Fence> completion;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIFactory1> factory;
};

QuarantineStore* Quarantine() {
    static QuarantineStore* store = new QuarantineStore();
    return store;
}

}  // namespace

struct Device::Impl {
    ComPtr<IDXGIFactory1> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandQueue> gateQueue;
    ComPtr<ID3D12Fence> completion;

    AdapterInfo adapterInfo;
    d3d12::DiagnosticReport diagnostics;
    std::map<uint32_t, Q1aRecord> records;

    uint64_t nextCompletionValue = 1;
    bool initialized = false;

    bool CompletionValueReached(uint64_t serial) const {
        if (!completion || !device) {
            return false;
        }
        if (FAILED(device->GetDeviceRemovedReason())) {
            return false;
        }
        const uint64_t completed = completion->GetCompletedValue();
        return completed != kRemovedSentinel && completed >= serial;
    }

    bool Lost(std::string* error) const {
        if (!device) {
            return false;
        }
        const HRESULT reason = device->GetDeviceRemovedReason();
        if (FAILED(reason)) {
            if (error != nullptr) {
                *error = "device removed before ownership was proved (" +
                         HrString(reason) + ")";
            }
            return true;
        }
        return false;
    }

    Q1aRecord* FindRecord(const Submission& submission) {
        auto it = records.find(submission.id);
        if (it == records.end() || it->second.released ||
            it->second.token != submission.completionToken) {
            return nullptr;
        }
        return &it->second;
    }

    const Q1aRecord* FindRecord(const Submission& submission) const {
        auto it = records.find(submission.id);
        if (it == records.end() || it->second.released ||
            it->second.token != submission.completionToken) {
            return nullptr;
        }
        return &it->second;
    }

    static void ReleaseRecord(Q1aRecord& record) {
        record.commandList.Reset();
        record.gateFence.Reset();
        record.allocator.Reset();
        record.readback.Reset();
        record.rtvHeap.Reset();
        record.texture.Reset();
        record.released = true;
    }

    void ReleaseObjects() {
        queue.Reset();
        gateQueue.Reset();
        completion.Reset();
        device.Reset();
        adapter.Reset();
        factory.Reset();
    }

    bool WaitForCompletionValue(uint64_t serial, DWORD timeoutMilliseconds) {
        if (!completion || !device) {
            return false;
        }
        if (FAILED(device->GetDeviceRemovedReason())) {
            return false;
        }
        const uint64_t completed = completion->GetCompletedValue();
        if (completed == kRemovedSentinel) {
            return false;
        }
        if (completed >= serial) {
            return true;
        }
        HANDLE eventHandle = CreateEventExW(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        if (eventHandle == nullptr) {
            return false;
        }
        const HRESULT setEvent = completion->SetEventOnCompletion(serial, eventHandle);
        if (FAILED(setEvent)) {
            CloseHandle(eventHandle);
            return false;
        }
        const DWORD wait = WaitForSingleObject(eventHandle, timeoutMilliseconds);
        CloseHandle(eventHandle);
        if (wait != WAIT_OBJECT_0) {
            return false;
        }
        if (FAILED(device->GetDeviceRemovedReason())) {
            return false;
        }
        const uint64_t after = completion->GetCompletedValue();
        return after != kRemovedSentinel && after >= serial;
    }

    void UpdateDiagnostics() {
        if (!device) {
            return;
        }
        ComPtr<ID3D12InfoQueue> infoQueue;
        if (FAILED(device.As(&infoQueue)) || !infoQueue) {
            return;
        }
        const UINT64 messageCount = infoQueue->GetNumStoredMessages();
        diagnostics.messageCount =
            messageCount > 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32_t>(messageCount);
        diagnostics.errorCount = 0;
        diagnostics.warningCount = 0;
        diagnostics.messages.clear();
        for (UINT64 index = 0; index < messageCount; ++index) {
            SIZE_T byteLength = 0;
            if (FAILED(infoQueue->GetMessage(index, nullptr, &byteLength)) ||
                byteLength == 0) {
                continue;
            }
            void* storage = std::malloc(byteLength);
            if (storage == nullptr) {
                continue;
            }
            auto* message = static_cast<D3D12_MESSAGE*>(storage);
            SIZE_T actualLength = byteLength;
            if (SUCCEEDED(infoQueue->GetMessage(index, message, &actualLength))) {
                if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
                    ++diagnostics.errorCount;
                } else if (message->Severity == D3D12_MESSAGE_SEVERITY_WARNING) {
                    ++diagnostics.warningCount;
                }
                if (diagnostics.messages.size() < kMaxCapturedMessages) {
                    diagnostics.messages.push_back(
                        std::string(SeverityName(message->Severity)) + ": " +
                        (message->pDescription != nullptr ? message->pDescription : ""));
                }
            }
            std::free(storage);
        }
    }

    Status SelectAdapter(const DeviceOptions& options, std::string* error) {
        if (options.adapterClass != AdapterClass::Hardware &&
            options.adapterClass != AdapterClass::Software) {
            return Fail(Status::InvalidArgument, error, "unknown adapter class");
        }

        HRESULT hr = CreateDXGIFactory1(
            __uuidof(IDXGIFactory1), reinterpret_cast<void**>(factory.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error, ErrorWith("CreateDXGIFactory1", hr));
        }

        std::vector<ComPtr<IDXGIAdapter1>> candidates;
        if (options.adapterClass == AdapterClass::Hardware) {
            for (UINT index = 0;; ++index) {
                ComPtr<IDXGIAdapter1> candidate;
                hr = factory->EnumAdapters1(index, candidate.GetAddressOf());
                if (hr == DXGI_ERROR_NOT_FOUND) {
                    break;
                }
                if (FAILED(hr)) {
                    return Fail(Status::Error, error, ErrorWith("EnumAdapters1", hr));
                }
                DXGI_ADAPTER_DESC1 description{};
                if (FAILED(candidate->GetDesc1(&description))) {
                    continue;
                }
                if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
                    continue;
                }
                candidates.push_back(std::move(candidate));
            }
        } else {
            ComPtr<IDXGIFactory4> factory4;
            if (SUCCEEDED(factory.As(&factory4)) && factory4) {
                ComPtr<IDXGIAdapter1> warp;
                hr = factory4->EnumWarpAdapter(
                    __uuidof(IDXGIAdapter1),
                    reinterpret_cast<void**>(warp.GetAddressOf()));
                if (SUCCEEDED(hr) && warp) {
                    candidates.push_back(std::move(warp));
                }
            }
            for (UINT index = 0;; ++index) {
                ComPtr<IDXGIAdapter1> candidate;
                hr = factory->EnumAdapters1(index, candidate.GetAddressOf());
                if (hr == DXGI_ERROR_NOT_FOUND) {
                    break;
                }
                if (FAILED(hr)) {
                    return Fail(Status::Error, error, ErrorWith("EnumAdapters1", hr));
                }
                DXGI_ADAPTER_DESC1 description{};
                if (FAILED(candidate->GetDesc1(&description))) {
                    continue;
                }
                if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
                    continue;
                }
                bool duplicate = false;
                for (const auto& existing : candidates) {
                    DXGI_ADAPTER_DESC1 existingDescription{};
                    if (SUCCEEDED(existing->GetDesc1(&existingDescription)) &&
                        existingDescription.AdapterLuid.HighPart ==
                            description.AdapterLuid.HighPart &&
                        existingDescription.AdapterLuid.LowPart ==
                            description.AdapterLuid.LowPart) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) {
                    candidates.push_back(std::move(candidate));
                }
            }
        }

        if (options.adapterIndex >= candidates.size()) {
            return Fail(Status::Unsupported, error,
                        std::string("adapterIndex ") + std::to_string(options.adapterIndex) +
                            " is out of range for " + ToString(options.adapterClass) +
                            "; found " + std::to_string(candidates.size()) + " adapter(s)");
        }

        adapter = std::move(candidates[options.adapterIndex]);
        DXGI_ADAPTER_DESC1 description{};
        hr = adapter->GetDesc1(&description);
        if (FAILED(hr)) {
            return Fail(Status::Error, error, ErrorWith("IDXGIAdapter1::GetDesc1", hr));
        }
        adapterInfo.adapterClass = options.adapterClass;
        adapterInfo.name = WideToUtf8(description.Description);
        adapterInfo.vendorId = description.VendorId;
        adapterInfo.deviceId = description.DeviceId;
        adapterInfo.dedicatedVideoMemory =
            static_cast<uint64_t>(description.DedicatedVideoMemory);
        return Status::Ok;
    }

    Status CreateBackendObjects(std::string* error) {
        const HRESULT deviceResult = D3D12CreateDevice(
            adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
            reinterpret_cast<void**>(device.GetAddressOf()));
        if (FAILED(deviceResult)) {
            const Status status = adapterInfo.adapterClass == AdapterClass::Software
                                      ? Status::Unsupported
                                      : Status::Error;
            return Fail(status, error, ErrorWith("D3D12CreateDevice", deviceResult));
        }

        D3D12_COMMAND_QUEUE_DESC queueDescription{};
        queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDescription.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        queueDescription.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDescription.NodeMask = 0;
        HRESULT hr = device->CreateCommandQueue(
            &queueDescription, __uuidof(ID3D12CommandQueue),
            reinterpret_cast<void**>(queue.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error, ErrorWith("CreateCommandQueue", hr));
        }
        hr = device->CreateCommandQueue(
            &queueDescription, __uuidof(ID3D12CommandQueue),
            reinterpret_cast<void**>(gateQueue.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error, ErrorWith("CreateCommandQueue(gate)", hr));
        }
        hr = device->CreateFence(
            0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
            reinterpret_cast<void**>(completion.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error, ErrorWith("CreateFence", hr));
        }
        return Status::Ok;
    }

    void ConfigureDiagnostics(bool requestDebugLayer, bool requestGpuValidation) {
        diagnostics = d3d12::DiagnosticReport{};
        if (!requestDebugLayer && !requestGpuValidation) {
            return;
        }
        diagnostics.debugLayer = d3d12::CheckState::NotRun;
        if (requestGpuValidation) {
            diagnostics.gpuBasedValidation = d3d12::CheckState::NotRun;
        }

        ComPtr<ID3D12Debug> debug;
        const HRESULT debugResult = D3D12GetDebugInterface(
            __uuidof(ID3D12Debug), reinterpret_cast<void**>(debug.GetAddressOf()));
        if (FAILED(debugResult) || !debug) {
            diagnostics.debugLayer = d3d12::CheckState::Skipped;
            diagnostics.debugLayerDetail =
                ErrorWith("D3D12GetDebugInterface", debugResult);
            if (requestGpuValidation) {
                diagnostics.gpuBasedValidation = d3d12::CheckState::Skipped;
                diagnostics.gpuBasedValidationDetail =
                    ErrorWith("D3D12GetDebugInterface (GPU-based validation)", debugResult);
            }
            return;
        }

        debug->EnableDebugLayer();
        diagnostics.debugLayer = d3d12::CheckState::Run;
        diagnostics.debugLayerDetail =
            requestDebugLayer
                ? "ID3D12Debug::EnableDebugLayer invoked"
                : "ID3D12Debug::EnableDebugLayer invoked implicitly because GPU-based "
                  "validation was requested";
        if (!requestGpuValidation) {
            return;
        }

        ComPtr<ID3D12Debug1> debug1;
        const HRESULT debug1Result = debug.As(&debug1);
        if (FAILED(debug1Result) || !debug1) {
            diagnostics.gpuBasedValidation = d3d12::CheckState::Skipped;
            diagnostics.gpuBasedValidationDetail = ErrorWith("ID3D12Debug1 query", debug1Result);
            return;
        }
        debug1->SetEnableGPUBasedValidation(TRUE);
        diagnostics.gpuBasedValidation = d3d12::CheckState::Run;
        diagnostics.gpuBasedValidationDetail =
            "ID3D12Debug1::SetEnableGPUBasedValidation(TRUE) invoked";
    }

    Status Initialize(const DeviceOptions& options, bool requestDebugLayer,
                      bool requestGpuValidation, std::string* error) {
        if (initialized) {
            return Fail(Status::InvalidArgument, error, "Device::Initialize called twice");
        }
        records.clear();
        nextCompletionValue = 1;
        ConfigureDiagnostics(requestDebugLayer, requestGpuValidation);

        Status status = SelectAdapter(options, error);
        if (status != Status::Ok) {
            ReleaseObjects();
            return status;
        }
        status = CreateBackendObjects(error);
        if (status != Status::Ok) {
            ReleaseObjects();
            return status;
        }
        initialized = true;
        if (error != nullptr) {
            error->clear();
        }
        return Status::Ok;
    }

    Status Submit(const ClearReadbackDesc& desc, bool gated, Submission* submission,
                  std::string* error) {
        if (submission == nullptr) {
            return Fail(Status::InvalidArgument, error, "submission output is null");
        }
        *submission = Submission{};
        if (!initialized || !device || !queue || !gateQueue || !completion) {
            return Fail(Status::Error, error, "device is not initialized");
        }
        if (Lost(error)) {
            return Status::DeviceLost;
        }
        if (desc.width == 0 || desc.height == 0) {
            return Fail(Status::InvalidArgument, error, "clear/readback dimensions are zero");
        }
        if (desc.width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
            desc.height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION) {
            return Fail(Status::InvalidArgument, error,
                        "clear/readback dimensions exceed the texture limit");
        }
        if (desc.width >
            std::numeric_limits<uint32_t>::max() / 4) {
            return Fail(Status::InvalidArgument, error, "row stride overflows uint32");
        }
        if (desc.rows.size() != desc.height) {
            return Fail(Status::InvalidArgument, error,
                        "ClearReadbackDesc::rows must contain exactly one entry per row");
        }
        if (nextCompletionValue == kRemovedSentinel) {
            return Fail(Status::Error, error, "completion value space exhausted");
        }

        Q1aRecord record;
        if (!AllocateSubmissionId(&record.id)) {
            return Fail(Status::Error, error, "submission identity space exhausted");
        }
        if (!AllocateCompletionToken(&record.token)) {
            return Fail(Status::Error, error, "completion token space exhausted");
        }
        record.serial = nextCompletionValue++;
        record.gated = gated;
        record.width = desc.width;
        record.height = desc.height;
        record.tightRowStride = desc.width * 4;

        HRESULT hr = S_OK;
        D3D12_RESOURCE_DESC textureDescription{};
        textureDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        textureDescription.Alignment = 0;
        textureDescription.Width = desc.width;
        textureDescription.Height = desc.height;
        textureDescription.DepthOrArraySize = 1;
        textureDescription.MipLevels = 1;
        textureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDescription.SampleDesc.Count = 1;
        textureDescription.SampleDesc.Quality = 0;
        textureDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        textureDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        D3D12_HEAP_PROPERTIES defaultHeap{};
        defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
        defaultHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        defaultHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        defaultHeap.CreationNodeMask = 1;
        defaultHeap.VisibleNodeMask = 1;
        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        clearValue.Color[0] = 0.0f;
        clearValue.Color[1] = 0.0f;
        clearValue.Color[2] = 0.0f;
        clearValue.Color[3] = 0.0f;
        hr = device->CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE, &textureDescription,
            kTextureInitialState, &clearValue, __uuidof(ID3D12Resource),
            reinterpret_cast<void**>(record.texture.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error,
                        ErrorWith("CreateCommittedResource(render target)", hr));
        }

        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDescription{};
        rtvHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDescription.NumDescriptors = 1;
        rtvHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        rtvHeapDescription.NodeMask = 0;
        hr = device->CreateDescriptorHeap(
            &rtvHeapDescription, __uuidof(ID3D12DescriptorHeap),
            reinterpret_cast<void**>(record.rtvHeap.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error, ErrorWith("CreateDescriptorHeap", hr));
        }

        D3D12_RENDER_TARGET_VIEW_DESC rtvDescription{};
        rtvDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        rtvDescription.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDescription.Texture2D.MipSlice = 0;
        rtvDescription.Texture2D.PlaneSlice = 0;
        device->CreateRenderTargetView(record.texture.Get(), &rtvDescription,
                                       record.rtvHeap->GetCPUDescriptorHandleForHeapStart());

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT rowCount = 0;
        UINT64 rowSizeInBytes = 0;
        UINT64 totalBytes = 0;
        device->GetCopyableFootprints(&textureDescription, 0, 1, 0, &footprint, &rowCount,
                                      &rowSizeInBytes, &totalBytes);
        record.rowPitch = footprint.Footprint.RowPitch;
        if (record.rowPitch < record.tightRowStride || totalBytes == 0 ||
            rowCount != desc.height) {
            return Fail(Status::Error, error,
                        "GetCopyableFootprints returned an unexpected readback layout");
        }
        const UINT64 rowSpanBytes =
            static_cast<UINT64>(record.rowPitch) * static_cast<UINT64>(desc.height);
        const UINT64 allocationBytes = std::max(totalBytes, rowSpanBytes);
        record.readbackBytes = allocationBytes;

        D3D12_RESOURCE_DESC bufferDescription{};
        bufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDescription.Alignment = 0;
        bufferDescription.Width = allocationBytes;
        bufferDescription.Height = 1;
        bufferDescription.DepthOrArraySize = 1;
        bufferDescription.MipLevels = 1;
        bufferDescription.Format = DXGI_FORMAT_UNKNOWN;
        bufferDescription.SampleDesc.Count = 1;
        bufferDescription.SampleDesc.Quality = 0;
        bufferDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferDescription.Flags = D3D12_RESOURCE_FLAG_NONE;
        D3D12_HEAP_PROPERTIES readbackHeap{};
        readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
        readbackHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        readbackHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        readbackHeap.CreationNodeMask = 1;
        readbackHeap.VisibleNodeMask = 1;
        hr = device->CreateCommittedResource(
            &readbackHeap, D3D12_HEAP_FLAG_NONE, &bufferDescription,
            kReadbackInitialState, nullptr, __uuidof(ID3D12Resource),
            reinterpret_cast<void**>(record.readback.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error,
                        ErrorWith("CreateCommittedResource(readback)", hr));
        }

        hr = device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
            reinterpret_cast<void**>(record.allocator.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error, ErrorWith("CreateCommandAllocator", hr));
        }
        hr = device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, record.allocator.Get(), nullptr,
            __uuidof(ID3D12GraphicsCommandList),
            reinterpret_cast<void**>(record.commandList.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error, ErrorWith("CreateCommandList", hr));
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
            record.rtvHeap->GetCPUDescriptorHandleForHeapStart();
        for (uint32_t y = 0; y < desc.height; ++y) {
            const ClearRow& row = desc.rows[y];
            const FLOAT color[4] = {row.red / 255.0f, row.green / 255.0f,
                                    row.blue / 255.0f, row.alpha / 255.0f};
            const D3D12_RECT rect{0, static_cast<LONG>(y), static_cast<LONG>(desc.width),
                                  static_cast<LONG>(y + 1)};
            record.commandList->ClearRenderTargetView(rtvHandle, color, 1, &rect);
        }
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = record.texture.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        record.commandList->ResourceBarrier(1, &barrier);

        D3D12_TEXTURE_COPY_LOCATION destination{};
        destination.pResource = record.readback.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint = footprint;
        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = record.texture.Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        source.SubresourceIndex = 0;
        record.commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        hr = record.commandList->Close();
        if (FAILED(hr)) {
            return Fail(Status::Error, error, ErrorWith("ID3D12GraphicsCommandList::Close", hr));
        }

        // Journal and output storage are prepared before the submission so no
        // allocation happens between ExecuteCommandLists and the published
        // token. The journal is backend-private and read only by private tests.
        record.journal.reserve(3);
        record.journal.push_back(d3d12::JournalEntry{
            "render-target", "UNKNOWN", PrivateStateName(kTextureInitialState),
            "created for render-target use"});
        record.journal.push_back(d3d12::JournalEntry{
            "render-target", PrivateStateName(barrier.Transition.StateBefore),
            PrivateStateName(barrier.Transition.StateAfter),
            "per-row clear recorded, then moved for the copy"});
        record.journal.push_back(d3d12::JournalEntry{
            "readback-buffer", "UNKNOWN", PrivateStateName(kReadbackInitialState),
            "created as the copy destination"});

        if (record.gated) {
            hr = device->CreateFence(
                0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
                reinterpret_cast<void**>(record.gateFence.GetAddressOf()));
            if (FAILED(hr)) {
                return Fail(Status::Error, error, ErrorWith("CreateFence(gate)", hr));
            }
            hr = queue->Wait(record.gateFence.Get(), 1);
            if (FAILED(hr)) {
                // Nothing was submitted; releasing the local record is safe.
                return Fail(Status::Error, error, ErrorWith("CommandQueue::Wait(gate)", hr));
            }
        }

        const uint32_t id = record.id;
        const uint64_t token = record.token;
        auto inserted = records.emplace(id, std::move(record));
        if (!inserted.second) {
            return Fail(Status::Error, error, "submission identity collision");
        }
        Q1aRecord& stored = inserted.first->second;
        ID3D12CommandList* commandLists[] = {stored.commandList.Get()};
        queue->ExecuteCommandLists(1, commandLists);
        const HRESULT signalResult = queue->Signal(completion.Get(), stored.serial);
        if (FAILED(signalResult)) {
            // The list may be running and its signal was lost. Keep the record
            // out of reach of callers; Shutdown quarantines it instead of
            // destroying objects the GPU may still use.
            stored.signalled = false;
            std::string reason = ErrorWith("CommandQueue::Signal", signalResult);
            const HRESULT removed = device->GetDeviceRemovedReason();
            if (FAILED(removed)) {
                reason += "; " + HrString(removed);
            }
            return Fail(Status::Error, error, reason);
        }
        stored.signalled = true;

        submission->id = id;
        submission->completionToken = token;
        if (error != nullptr) {
            error->clear();
        }
        return Status::Ok;
    }

    Status ReleaseGate(const Submission& submission, std::string* error) {
        Q1aRecord* record = FindRecord(submission);
        if (record == nullptr) {
            return Fail(Status::InvalidArgument, error, "unknown or released submission");
        }
        if (!record->gated) {
            return Fail(Status::InvalidArgument, error,
                        "release gate called for an ungated submission");
        }
        if (record->gateReleased) {
            if (error != nullptr) {
                error->clear();
            }
            return Status::Ok;
        }
        if (!gateQueue || !record->gateFence) {
            return Fail(Status::Error, error, "gate queue or gate object is unavailable");
        }
        const HRESULT hr = gateQueue->Signal(record->gateFence.Get(), 1);
        if (FAILED(hr)) {
            return Fail(Status::Error, error, ErrorWith("CommandQueue::Signal(gate)", hr));
        }
        record->gateReleased = true;
        if (error != nullptr) {
            error->clear();
        }
        return Status::Ok;
    }

    Status Readback(const Submission& submission, ReadbackResult* result,
                    std::string* error) {
        if (result == nullptr) {
            return Fail(Status::InvalidArgument, error, "readback result is null");
        }
        *result = ReadbackResult{};
        Q1aRecord* record = FindRecord(submission);
        if (record == nullptr) {
            return Fail(Status::InvalidArgument, error, "unknown or released submission");
        }
        if (!record->signalled) {
            return Fail(Status::Error, error,
                        "submission was submitted without a completion signal");
        }
        if (Lost(error)) {
            return Status::DeviceLost;
        }
        if (!CompletionValueReached(record->serial)) {
            return Fail(Status::NotReady, error,
                        "readback requested before the completion token was reached");
        }
        if (!record->readback) {
            return Fail(Status::Error, error, "readback storage is unavailable");
        }

        void* mapped = nullptr;
        const HRESULT mapResult = record->readback->Map(0, nullptr, &mapped);
        if (FAILED(mapResult) || mapped == nullptr) {
            if (Lost(error)) {
                return Status::DeviceLost;
            }
            return Fail(Status::Error, error,
                        mapResult == S_OK
                            ? std::string("ID3D12Resource::Map returned a null pointer")
                            : ErrorWith("ID3D12Resource::Map", mapResult));
        }
        const size_t stagingBytes =
            static_cast<size_t>(record->rowPitch) * static_cast<size_t>(record->height);
        if (stagingBytes > record->readbackBytes) {
            record->readback->Unmap(0, nullptr);
            return Fail(Status::Error, error,
                        "staging layout exceeds the readback allocation");
        }
        const auto* bytes = static_cast<const uint8_t*>(mapped);
        result->width = record->width;
        result->height = record->height;
        result->rowStride = record->tightRowStride;
        result->adapter = adapterInfo;
        try {
            result->pixels.resize(
                static_cast<size_t>(record->tightRowStride) * record->height);
            for (uint32_t y = 0; y < record->height; ++y) {
                std::memcpy(result->pixels.data() +
                                static_cast<size_t>(y) * record->tightRowStride,
                            bytes + static_cast<size_t>(y) * record->rowPitch,
                            record->tightRowStride);
            }
        } catch (const std::bad_alloc&) {
            record->readback->Unmap(0, nullptr);
            *result = ReadbackResult{};
            return Fail(Status::OutOfMemory, error,
                        "caller-owned readback bytes could not be allocated");
        }
        record->readback->Unmap(0, nullptr);
        if (error != nullptr) {
            error->clear();
        }
        return Status::Ok;
    }

    ReleaseStatus TryRelease(const Submission& submission, std::string* error) {
        Q1aRecord* record = FindRecord(submission);
        if (record == nullptr) {
            if (error != nullptr) {
                *error = "unknown or already released submission";
            }
            return ReleaseStatus::InvalidSubmission;
        }
        if (!record->signalled) {
            if (error != nullptr) {
                *error = "completion was never signalled; ownership is unproven";
            }
            return ReleaseStatus::RefusedOutstanding;
        }
        if (Lost(error)) {
            return ReleaseStatus::RefusedOutstanding;
        }
        if (!CompletionValueReached(record->serial)) {
            if (error != nullptr) {
                *error = "completion token is outstanding";
            }
            return ReleaseStatus::RefusedOutstanding;
        }
        const uint32_t id = submission.id;
        ReleaseRecord(*record);
        records.erase(id);
        if (error != nullptr) {
            error->clear();
        }
        return ReleaseStatus::Released;
    }

    bool IsComplete(const Submission& submission) const {
        const Q1aRecord* record = FindRecord(submission);
        if (record == nullptr || !record->signalled) {
            return false;
        }
        return CompletionValueReached(record->serial);
    }

    Status Shutdown() {
        Status result = Status::Ok;
        bool quarantine = false;

        for (auto& entry : records) {
            Q1aRecord& record = entry.second;
            if (!record.released && record.gated && !record.gateReleased && gateQueue &&
                record.gateFence) {
                const HRESULT signalResult = gateQueue->Signal(record.gateFence.Get(), 1);
                if (SUCCEEDED(signalResult)) {
                    record.gateReleased = true;
                } else {
                    quarantine = true;
                    result = Status::Error;
                }
            }
        }

        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(kShutdownTimeoutMilliseconds);
        for (auto& entry : records) {
            Q1aRecord& record = entry.second;
            if (!record.released) {
                if (!record.signalled) {
                    quarantine = true;
                    if (result == Status::Ok) {
                        result = Status::Error;
                    }
                    continue;
                }
                if (record.gated && !record.gateReleased) {
                    // The queue is still held on a gate that could not be
                    // opened; waiting would burn the whole teardown budget.
                    quarantine = true;
                    if (result == Status::Ok) {
                        result = Status::Error;
                    }
                    continue;
                }
                if (FAILED(device ? device->GetDeviceRemovedReason() : E_FAIL)) {
                    quarantine = true;
                    if (result == Status::Ok) {
                        result = Status::DeviceLost;
                    }
                    continue;
                }
                const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now());
                const bool completed =
                    remaining.count() > 0 &&
                    WaitForCompletionValue(record.serial,
                                           static_cast<DWORD>(remaining.count()));
                if (!completed) {
                    quarantine = true;
                    if (result == Status::Ok) {
                        result = Status::Error;
                    }
                    continue;
                }
                ReleaseRecord(record);
            }
        }
        if (quarantine) {
            // Do not destroy objects whose last use is unproven. Move the
            // records and the whole backend object set to a leaked store.
            QuarantineStore* store = Quarantine();
            for (auto& entry : records) {
                store->records.push_back(std::move(entry.second));
            }
            records.clear();
            store->device = std::move(device);
            store->queue = std::move(queue);
            store->gateQueue = std::move(gateQueue);
            store->completion = std::move(completion);
            store->adapter = std::move(adapter);
            store->factory = std::move(factory);
            initialized = false;
            return result;
        }

        records.clear();
        ReleaseObjects();
        initialized = false;
        return result;
    }
};

const char* ToString(AdapterClass value) {
    switch (value) {
    case AdapterClass::Hardware:
        return "hardware";
    case AdapterClass::Software:
        return "software";
    }
    return "unknown";
}

const char* ToString(Status value) {
    switch (value) {
    case Status::Ok:
        return "Ok";
    case Status::NotReady:
        return "NotReady";
    case Status::InvalidArgument:
        return "InvalidArgument";
    case Status::Unsupported:
        return "Unsupported";
    case Status::DeviceLost:
        return "DeviceLost";
    case Status::OutOfMemory:
        return "OutOfMemory";
    case Status::Error:
        return "Error";
    }
    return "unknown";
}

const char* ToString(ReleaseStatus value) {
    switch (value) {
    case ReleaseStatus::Released:
        return "Released";
    case ReleaseStatus::RefusedOutstanding:
        return "RefusedOutstanding";
    case ReleaseStatus::InvalidSubmission:
        return "InvalidSubmission";
    }
    return "unknown";
}

namespace d3d12 {

const char* ToString(CheckState value) {
    switch (value) {
    case CheckState::NotRequested:
        return "NOT_REQUESTED";
    case CheckState::Run:
        return "RUN";
    case CheckState::Skipped:
        return "SKIPPED";
    case CheckState::NotRun:
        return "NOT_RUN";
    case CheckState::Failed:
        return "FAILED";
    }
    return "unknown";
}

Status InitializeWithDiagnostics(Device& device, const DeviceOptions& options,
                                 const DiagnosticRequest& request, std::string* error) {
    return BackendAccess::Initialize(device, options, request.enableDebugLayer,
                                     request.enableGpuBasedValidation, error);
}

Status SubmitWithGate(Device& device, const ClearReadbackDesc& desc,
                      Submission* submission, std::string* error) {
    return BackendAccess::SubmitGated(device, desc, submission, error);
}

Status ReleaseGate(Device& device, const Submission& submission, std::string* error) {
    return BackendAccess::ReleaseGate(device, submission, error);
}

DiagnosticReport ReadDiagnostics(const Device& device) {
    Device& mutableDevice = const_cast<Device&>(device);
    return BackendAccess::Diagnostics(mutableDevice);
}

std::vector<JournalEntry> ReadJournal(const Device& device, const Submission& submission) {
    return BackendAccess::Journal(device, submission);
}

RawReadback ReadRawReadback(const Device& device, const Submission& submission,
                            std::string* error) {
    return BackendAccess::RawReadback(device, submission, error);
}

FenceObservation ObserveFence(const Device& device, const Submission& submission) {
    return BackendAccess::Observe(device, submission);
}

}  // namespace d3d12

Device::Device() : impl_(std::make_unique<Impl>()) {}

Device::~Device() {
    Shutdown();
}

Status Device::Initialize(const DeviceOptions& options, std::string* error) {
    return impl_->Initialize(options, false, false, error);
}

Status Device::Shutdown() {
    return impl_->Shutdown();
}

bool Device::IsInitialized() const {
    return impl_ && impl_->initialized;
}

const AdapterInfo& Device::adapter() const {
    return impl_->adapterInfo;
}

Status Device::SubmitClearReadback(const ClearReadbackDesc& desc, Submission* submission,
                                   std::string* error) {
    return impl_->Submit(desc, false, submission, error);
}

bool Device::IsComplete(const Submission& submission) const {
    return impl_->IsComplete(submission);
}

Status Device::Readback(const Submission& submission, ReadbackResult* result,
                        std::string* error) {
    return impl_->Readback(submission, result, error);
}

ReleaseStatus Device::TryRelease(const Submission& submission, std::string* error) {
    return impl_->TryRelease(submission, error);
}

Status BackendAccess::Initialize(Device& device, const DeviceOptions& options,
                                 bool requestDebugLayer, bool requestGpuValidation,
                                 std::string* error) {
    return device.impl_->Initialize(options, requestDebugLayer, requestGpuValidation, error);
}

Status BackendAccess::SubmitGated(Device& device, const ClearReadbackDesc& desc,
                                  Submission* submission, std::string* error) {
    return device.impl_->Submit(desc, true, submission, error);
}

Status BackendAccess::ReleaseGate(Device& device, const Submission& submission,
                                  std::string* error) {
    return device.impl_->ReleaseGate(submission, error);
}

d3d12::DiagnosticReport BackendAccess::Diagnostics(Device& device) {
    device.impl_->UpdateDiagnostics();
    return device.impl_->diagnostics;
}

std::vector<d3d12::JournalEntry> BackendAccess::Journal(const Device& device,
                                                        const Submission& submission) {
    const Q1aRecord* record = device.impl_->FindRecord(submission);
    if (record == nullptr) {
        return {};
    }
    return record->journal;
}

d3d12::RawReadback BackendAccess::RawReadback(const Device& device,
                                              const Submission& submission,
                                              std::string* error) {
    d3d12::RawReadback raw;
    const Q1aRecord* record = device.impl_->FindRecord(submission);
    if (record == nullptr) {
        if (error != nullptr) {
            *error = "unknown or released submission";
        }
        return raw;
    }
    if (!record->signalled || !record->readback) {
        if (error != nullptr) {
            *error = "submission has no readable storage";
        }
        return raw;
    }
    Device& mutableDevice = const_cast<Device&>(device);
    if (mutableDevice.impl_->Lost(error)) {
        return raw;
    }
    if (!mutableDevice.impl_->CompletionValueReached(record->serial)) {
        if (error != nullptr) {
            *error = "completion token is outstanding";
        }
        return raw;
    }
    void* mapped = nullptr;
    const HRESULT mapResult = record->readback->Map(0, nullptr, &mapped);
    if (FAILED(mapResult) || mapped == nullptr) {
        if (error != nullptr) {
            *error = ErrorWith("ID3D12Resource::Map", mapResult);
        }
        return raw;
    }
    const size_t stagingBytes =
        static_cast<size_t>(record->rowPitch) * static_cast<size_t>(record->height);
    if (stagingBytes > record->readbackBytes) {
        record->readback->Unmap(0, nullptr);
        if (error != nullptr) {
            *error = "staging layout exceeds the readback allocation";
        }
        return raw;
    }
    const auto* bytes = static_cast<const uint8_t*>(mapped);
    raw.width = record->width;
    raw.height = record->height;
    raw.stagingPitch = record->rowPitch;
    raw.tightRowStride = record->tightRowStride;
    try {
        raw.staging.assign(bytes, bytes + stagingBytes);
    } catch (const std::bad_alloc&) {
        record->readback->Unmap(0, nullptr);
        if (error != nullptr) {
            *error = "raw staging bytes could not be allocated";
        }
        return d3d12::RawReadback{};
    }
    record->readback->Unmap(0, nullptr);
    if (error != nullptr) {
        error->clear();
    }
    return raw;
}

d3d12::FenceObservation BackendAccess::Observe(const Device& device,
                                               const Submission& submission) {
    d3d12::FenceObservation observation;
    const Q1aRecord* record = device.impl_->FindRecord(submission);
    if (record == nullptr || !device.impl_->completion) {
        return observation;
    }
    observation.signalValue = record->serial;
    const uint64_t completed = device.impl_->completion->GetCompletedValue();
    observation.completedValue = completed == kRemovedSentinel ? 0 : completed;
    return observation;
}

}  // namespace rhi
}  // namespace doom3
