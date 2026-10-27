#include "rhi/rhi.h"

#include <windows.h>

#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace doom3 {
namespace rhi {

namespace {

constexpr DWORD kWaitTimeoutMilliseconds = 30000;
constexpr size_t kMaxCapturedMessages = 8;

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

// The resource creation states are named once and used both for the D3D12
// calls and for the public state journal. The journal is therefore derived
// from the states actually passed to D3D12, not a separate hand-written story.
constexpr D3D12_RESOURCE_STATES kRenderTargetInitialState =
    D3D12_RESOURCE_STATE_RENDER_TARGET;
constexpr D3D12_RESOURCE_STATES kReadbackInitialState = D3D12_RESOURCE_STATE_COPY_DEST;

ResourceState ToResourceState(D3D12_RESOURCE_STATES state) {
    switch (state) {
        case D3D12_RESOURCE_STATE_COMMON:
            return ResourceState::Common;
        case D3D12_RESOURCE_STATE_RENDER_TARGET:
            return ResourceState::RenderTarget;
        case D3D12_RESOURCE_STATE_COPY_SOURCE:
            return ResourceState::CopySource;
        case D3D12_RESOURCE_STATE_COPY_DEST:
            return ResourceState::CopyDest;
        default:
            return ResourceState::Unknown;
    }
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

}  // namespace

struct Device::Impl {
    struct Record {
        uint32_t id = 0;
        uint64_t serial = 0;
        uint64_t gateValue = 0;
        bool gated = false;
        bool gateReleased = false;
        bool released = false;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t rowPitch = 0;
        uint32_t tightRowPitch = 0;
        uint64_t readbackBytes = 0;

        ComPtr<ID3D12Resource> texture;
        ComPtr<ID3D12Resource> readback;
        ComPtr<ID3D12DescriptorHeap> rtvHeap;
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12GraphicsCommandList> commandList;
    };

    ComPtr<IDXGIFactory1> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandQueue> gateQueue;
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12Fence> gateFence;

    AdapterInfo adapterInfo;
    Diagnostics diagnostics;
    std::vector<StateTransition> journal;
    std::map<uint32_t, Record> records;

    uint64_t nextFenceValue = 1;
    uint64_t nextGateValue = 1;
    uint32_t nextSubmissionId = 1;
    bool initialized = false;

    Record* FindRecord(uint32_t id) {
        auto it = records.find(id);
        return it == records.end() ? nullptr : &it->second;
    }

    const Record* FindRecord(uint32_t id) const {
        auto it = records.find(id);
        return it == records.end() ? nullptr : &it->second;
    }

    void ReleaseRecord(Record& record) {
        record.commandList.Reset();
        record.allocator.Reset();
        record.readback.Reset();
        record.rtvHeap.Reset();
        record.texture.Reset();
        record.released = true;
    }

    void ReleaseObjects() {
        queue.Reset();
        gateQueue.Reset();
        fence.Reset();
        gateFence.Reset();
        device.Reset();
        adapter.Reset();
        factory.Reset();
    }

    bool WaitForSerial(uint64_t serial) {
        if (!fence) {
            return false;
        }
        if (fence->GetCompletedValue() >= serial) {
            return true;
        }
        HANDLE eventHandle = CreateEventExW(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        if (eventHandle == nullptr) {
            return false;
        }
        const HRESULT setEvent = fence->SetEventOnCompletion(serial, eventHandle);
        if (FAILED(setEvent)) {
            CloseHandle(eventHandle);
            return false;
        }
        const DWORD wait = WaitForSingleObject(eventHandle, kWaitTimeoutMilliseconds);
        CloseHandle(eventHandle);
        return wait == WAIT_OBJECT_0 && fence->GetCompletedValue() >= serial;
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
        adapterInfo.luid =
            (static_cast<uint64_t>(static_cast<uint32_t>(description.AdapterLuid.HighPart))
             << 32) |
            static_cast<uint64_t>(static_cast<uint32_t>(description.AdapterLuid.LowPart));
        return Status::Ok;
    }

    Status CreateBackendObjects(const DeviceOptions& options, std::string* error) {
        const HRESULT deviceResult = D3D12CreateDevice(
            adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
            reinterpret_cast<void**>(device.GetAddressOf()));
        if (FAILED(deviceResult)) {
            const Status status = options.adapterClass == AdapterClass::Software
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
            return Fail(Status::Error, error, ErrorWith("CreateCommandQueue(main)", hr));
        }
        hr = device->CreateCommandQueue(
            &queueDescription, __uuidof(ID3D12CommandQueue),
            reinterpret_cast<void**>(gateQueue.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error, ErrorWith("CreateCommandQueue(gate)", hr));
        }
        hr = device->CreateFence(
            0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
            reinterpret_cast<void**>(fence.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error, ErrorWith("CreateFence(completion)", hr));
        }
        hr = device->CreateFence(
            0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
            reinterpret_cast<void**>(gateFence.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error, ErrorWith("CreateFence(gate)", hr));
        }
        return Status::Ok;
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

const char* ToString(ResourceState value) {
    switch (value) {
    case ResourceState::Unknown:
        return "Unknown";
    case ResourceState::RenderTarget:
        return "RENDER_TARGET";
    case ResourceState::CopySource:
        return "COPY_SOURCE";
    case ResourceState::CopyDest:
        return "COPY_DEST";
    case ResourceState::Common:
        return "COMMON";
    }
    return "unknown";
}

const char* ToString(DiagnosticState value) {
    switch (value) {
    case DiagnosticState::NotRequested:
        return "NOT_REQUESTED";
    case DiagnosticState::Run:
        return "RUN";
    case DiagnosticState::Skipped:
        return "SKIPPED";
    case DiagnosticState::NotRun:
        return "NOT_RUN";
    case DiagnosticState::Failed:
        return "FAILED";
    }
    return "unknown";
}

Device::Device() : impl_(std::make_unique<Impl>()) {}

Device::~Device() {
    Shutdown();
}

Status Device::Initialize(const DeviceOptions& options, std::string* error) {
    Impl& impl = *impl_;
    if (impl.initialized) {
        return Fail(Status::InvalidArgument, error, "Device::Initialize called twice");
    }

    impl.diagnostics = Diagnostics{};
    impl.journal.clear();
    impl.records.clear();
    impl.nextFenceValue = 1;
    impl.nextGateValue = 1;
    impl.nextSubmissionId = 1;
    if (options.enableDebugLayer) {
        impl.diagnostics.debugLayer = DiagnosticState::NotRun;
    }
    if (options.enableGpuBasedValidation) {
        impl.diagnostics.gpuBasedValidation = DiagnosticState::NotRun;
    }

    Status status = impl.SelectAdapter(options, error);
    if (status != Status::Ok) {
        impl.ReleaseObjects();
        return status;
    }

    // GPU-based validation is only reachable through the debug interface, so a
    // GBV-only request must still attempt enablement instead of reporting an
    // un-attempted skip. When GBV is requested without an explicit debug-layer
    // request, the debug layer is enabled implicitly and recorded as Run with
    // that reason.
    if (options.enableDebugLayer || options.enableGpuBasedValidation) {
        ComPtr<ID3D12Debug> debug;
        const HRESULT debugResult = D3D12GetDebugInterface(
            __uuidof(ID3D12Debug), reinterpret_cast<void**>(debug.GetAddressOf()));
        if (FAILED(debugResult)) {
            if (options.enableDebugLayer) {
                impl.diagnostics.debugLayer = DiagnosticState::Skipped;
                impl.diagnostics.debugLayerDetail =
                    ErrorWith("D3D12GetDebugInterface", debugResult);
            }
            if (options.enableGpuBasedValidation) {
                impl.diagnostics.gpuBasedValidation = DiagnosticState::Skipped;
                impl.diagnostics.gpuBasedValidationDetail =
                    ErrorWith("D3D12GetDebugInterface (GPU-based validation)",
                              debugResult);
            }
        } else {
            debug->EnableDebugLayer();
            impl.diagnostics.debugLayer = DiagnosticState::Run;
            impl.diagnostics.debugLayerDetail =
                options.enableDebugLayer
                    ? "ID3D12Debug::EnableDebugLayer invoked"
                    : "ID3D12Debug::EnableDebugLayer invoked implicitly because "
                      "GPU-based validation was requested";
            if (options.enableGpuBasedValidation) {
                ComPtr<ID3D12Debug1> debug1;
                const HRESULT debug1Result = debug.As(&debug1);
                if (FAILED(debug1Result) || !debug1) {
                    impl.diagnostics.gpuBasedValidation = DiagnosticState::Skipped;
                    impl.diagnostics.gpuBasedValidationDetail =
                        ErrorWith("ID3D12Debug1 query", debug1Result);
                } else {
                    debug1->SetEnableGPUBasedValidation(TRUE);
                    impl.diagnostics.gpuBasedValidation = DiagnosticState::Run;
                    impl.diagnostics.gpuBasedValidationDetail =
                        "ID3D12Debug1::SetEnableGPUBasedValidation(TRUE) invoked";
                }
            }
        }
    }

    status = impl.CreateBackendObjects(options, error);
    if (status != Status::Ok) {
        impl.ReleaseObjects();
        return status;
    }

    impl.initialized = true;
    impl.UpdateDiagnostics();
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

Status Device::Shutdown() {
    Impl& impl = *impl_;
    Status result = Status::Ok;

    for (auto& entry : impl.records) {
        Impl::Record& record = entry.second;
        if (!record.released && record.gated && !record.gateReleased && impl.gateQueue &&
            impl.gateFence) {
            const HRESULT signalResult =
                impl.gateQueue->Signal(impl.gateFence.Get(), record.gateValue);
            if (SUCCEEDED(signalResult)) {
                record.gateReleased = true;
            }
        }
    }
    for (auto& entry : impl.records) {
        Impl::Record& record = entry.second;
        if (!record.released) {
            if (impl.fence && record.serial != 0 && !impl.WaitForSerial(record.serial)) {
                // The serial never completed. Report it rather than claiming a
                // clean shutdown; the fixture still drops its references, which
                // is test-process teardown, not a reusable device.
                result = Status::Error;
            }
            impl.ReleaseRecord(record);
        }
    }
    impl.records.clear();
    impl.ReleaseObjects();
    impl.initialized = false;
    return result;
}

bool Device::IsInitialized() const {
    return impl_ && impl_->initialized;
}

const AdapterInfo& Device::adapter() const {
    return impl_->adapterInfo;
}

const std::vector<StateTransition>& Device::stateJournal() const {
    return impl_->journal;
}

const Diagnostics& Device::diagnostics() const {
    return impl_->diagnostics;
}

Status Device::SubmitClearReadback(const ClearReadbackDesc& desc,
                                   const SubmissionOptions& options,
                                   Submission* submission, std::string* error) {
    Impl& impl = *impl_;
    if (submission == nullptr) {
        return Fail(Status::InvalidArgument, error, "submission output is null");
    }
    *submission = Submission{};
    if (!impl.initialized || !impl.device || !impl.queue || !impl.fence ||
        !impl.gateFence || !impl.gateQueue) {
        return Fail(Status::Error, error, "device is not initialized");
    }
    if (desc.width == 0 || desc.height == 0) {
        return Fail(Status::InvalidArgument, error, "clear/readback dimensions are zero");
    }
    if (desc.width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
        desc.height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION) {
        return Fail(Status::InvalidArgument, error,
                    "clear/readback dimensions exceed the D3D12 texture limit");
    }
    if (desc.rows.size() != desc.height) {
        return Fail(Status::InvalidArgument, error,
                    "ClearReadbackDesc::rows must contain exactly one entry per row");
    }

    Impl::Record record;
    record.id = impl.nextSubmissionId++;
    record.serial = impl.nextFenceValue++;
    record.gated = options.waitForCpuGate;
    record.gateValue = options.waitForCpuGate ? impl.nextGateValue++ : 0;
    record.width = desc.width;
    record.height = desc.height;
    record.tightRowPitch = desc.width * 4;
    record.readbackBytes = 0;

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
    hr = impl.device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &textureDescription,
        kRenderTargetInitialState, &clearValue, __uuidof(ID3D12Resource),
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
    hr = impl.device->CreateDescriptorHeap(
        &rtvHeapDescription, __uuidof(ID3D12DescriptorHeap),
        reinterpret_cast<void**>(record.rtvHeap.GetAddressOf()));
    if (FAILED(hr)) {
        return Fail(Status::Error, error, ErrorWith("CreateDescriptorHeap(RTV)", hr));
    }

    D3D12_RENDER_TARGET_VIEW_DESC rtvDescription{};
    rtvDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtvDescription.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDescription.Texture2D.MipSlice = 0;
    rtvDescription.Texture2D.PlaneSlice = 0;
    impl.device->CreateRenderTargetView(record.texture.Get(), &rtvDescription,
                                        record.rtvHeap->GetCPUDescriptorHandleForHeapStart());

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rowCount = 0;
    UINT64 rowSizeInBytes = 0;
    UINT64 totalBytes = 0;
    impl.device->GetCopyableFootprints(&textureDescription, 0, 1, 0, &footprint, &rowCount,
                                       &rowSizeInBytes, &totalBytes);
    record.rowPitch = footprint.Footprint.RowPitch;
    record.readbackBytes = totalBytes;
    if (record.rowPitch < record.tightRowPitch || totalBytes == 0) {
        return Fail(Status::Error, error,
                    "GetCopyableFootprints returned an invalid readback footprint");
    }
    if (rowCount != desc.height) {
        return Fail(Status::Error, error,
                    "GetCopyableFootprints returned an unexpected row count");
    }
    // D3D12's totalBytes excludes the padding after the last row. Q1a keeps a
    // raw row-pitched staging copy, so reserve the full rowPitch * height span
    // as well; the copy still writes only the footprint total.
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
    hr = impl.device->CreateCommittedResource(
        &readbackHeap, D3D12_HEAP_FLAG_NONE, &bufferDescription,
        kReadbackInitialState, nullptr, __uuidof(ID3D12Resource),
        reinterpret_cast<void**>(record.readback.GetAddressOf()));
    if (FAILED(hr)) {
        return Fail(Status::Error, error,
                    ErrorWith("CreateCommittedResource(readback)", hr));
    }

    hr = impl.device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
        reinterpret_cast<void**>(record.allocator.GetAddressOf()));
    if (FAILED(hr)) {
        return Fail(Status::Error, error, ErrorWith("CreateCommandAllocator", hr));
    }
    hr = impl.device->CreateCommandList(
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
        const FLOAT color[4] = {row.red / 255.0f, row.green / 255.0f, row.blue / 255.0f,
                                row.alpha / 255.0f};
        const D3D12_RECT rect{0, static_cast<LONG>(y), static_cast<LONG>(desc.width),
                              static_cast<LONG>(y + 1)};
        record.commandList->ClearRenderTargetView(rtvHandle, color, 1, &rect);
    }
    D3D12_RESOURCE_BARRIER transition{};
    transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    transition.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    transition.Transition.pResource = record.texture.Get();
    transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    transition.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    transition.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    record.commandList->ResourceBarrier(1, &transition);

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

    if (record.gated) {
        hr = impl.queue->Wait(impl.gateFence.Get(), record.gateValue);
        if (FAILED(hr)) {
            return Fail(Status::Error, error, ErrorWith("CommandQueue::Wait(gate)", hr));
        }
    }
    ID3D12CommandList* commandLists[] = {record.commandList.Get()};
    impl.queue->ExecuteCommandLists(1, commandLists);
    const HRESULT signalResult = impl.queue->Signal(impl.fence.Get(), record.serial);
    const uint32_t id = record.id;
    const uint64_t serial = record.serial;
    const bool gated = record.gated;
    impl.records.emplace(id, std::move(record));
    if (FAILED(signalResult)) {
        return Fail(Status::Error, error, ErrorWith("CommandQueue::Signal", signalResult));
    }

    impl.journal.push_back(
        StateTransition{"render-target", ResourceState::Unknown,
                        ToResourceState(kRenderTargetInitialState),
                        "created as a D3D12 render target"});
    impl.journal.push_back(
        StateTransition{"render-target", ToResourceState(transition.Transition.StateBefore),
                        ToResourceState(transition.Transition.StateAfter),
                        "per-row clear recorded; transitioned for texture copy"});
    impl.journal.push_back(
        StateTransition{"readback-buffer", ResourceState::Unknown,
                        ToResourceState(kReadbackInitialState),
                        "created as a D3D12 copy destination"});
    impl.UpdateDiagnostics();

    submission->id = id;
    submission->completionSerial = serial;
    submission->gated = gated;
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

Status Device::ReleaseGate(const Submission& submission, std::string* error) {
    Impl& impl = *impl_;
    Impl::Record* record = impl.FindRecord(submission.id);
    if (record == nullptr || record->released) {
        return Fail(Status::InvalidArgument, error, "unknown or released submission");
    }
    if (!record->gated) {
        return Fail(Status::InvalidArgument, error,
                    "ReleaseGate called for an ungated submission");
    }
    if (record->gateReleased) {
        if (error != nullptr) {
            error->clear();
        }
        return Status::Ok;
    }
    if (!impl.gateQueue || !impl.gateFence) {
        return Fail(Status::Error, error, "gate queue or gate fence is unavailable");
    }
    const HRESULT hr = impl.gateQueue->Signal(impl.gateFence.Get(), record->gateValue);
    if (FAILED(hr)) {
        return Fail(Status::Error, error, ErrorWith("CommandQueue::Signal(gate)", hr));
    }
    record->gateReleased = true;
    impl.UpdateDiagnostics();
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

Status Device::Wait(const Submission& submission, std::string* error) {
    Impl& impl = *impl_;
    Impl::Record* record = impl.FindRecord(submission.id);
    if (record == nullptr || record->released) {
        return Fail(Status::InvalidArgument, error, "unknown or released submission");
    }
    if (record->gated && !record->gateReleased) {
        return Fail(Status::InvalidArgument, error,
                    "submission is gated; ReleaseGate must be called before Wait");
    }
    if (!impl.WaitForSerial(record->serial)) {
        return Fail(Status::Error, error, "timed out or failed waiting for the completion fence");
    }
    impl.UpdateDiagnostics();
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

Status Device::Readback(const Submission& submission, ReadbackResult* result,
                        std::string* error) {
    Impl& impl = *impl_;
    if (result == nullptr) {
        return Fail(Status::InvalidArgument, error, "readback result is null");
    }
    *result = ReadbackResult{};
    Impl::Record* record = impl.FindRecord(submission.id);
    if (record == nullptr || record->released) {
        return Fail(Status::InvalidArgument, error, "unknown or released submission");
    }
    if (!impl.fence || impl.fence->GetCompletedValue() < record->serial) {
        return Fail(Status::InvalidArgument, error,
                    "readback requested before the completion serial was reached");
    }
    if (!record->readback) {
        return Fail(Status::InvalidArgument, error, "readback resource is unavailable");
    }

    void* mapped = nullptr;
    const HRESULT mapResult = record->readback->Map(0, nullptr, &mapped);
    if (FAILED(mapResult) || mapped == nullptr) {
        return Fail(Status::Error, error, ErrorWith("ID3D12Resource::Map", mapResult));
    }

    result->width = record->width;
    result->height = record->height;
    result->rowPitch = record->rowPitch;
    result->tightRowPitch = record->tightRowPitch;
    result->adapter = impl.adapterInfo;
    const size_t stagingBytes =
        static_cast<size_t>(record->rowPitch) * static_cast<size_t>(record->height);
    if (stagingBytes > record->readbackBytes) {
        record->readback->Unmap(0, nullptr);
        return Fail(Status::Error, error,
                    "staging row pitch exceeds the readback allocation (rowPitch=" +
                        std::to_string(record->rowPitch) + ", height=" +
                        std::to_string(record->height) + ", stagingBytes=" +
                        std::to_string(stagingBytes) + ", allocation=" +
                        std::to_string(record->readbackBytes) + ")");
    }
    const auto* bytes = static_cast<const uint8_t*>(mapped);
    result->staging.assign(bytes, bytes + stagingBytes);
    result->pixels.resize(static_cast<size_t>(record->tightRowPitch) * record->height);
    for (uint32_t y = 0; y < record->height; ++y) {
        std::memcpy(result->pixels.data() + static_cast<size_t>(y) * record->tightRowPitch,
                    bytes + static_cast<size_t>(y) * record->rowPitch,
                    record->tightRowPitch);
    }
    record->readback->Unmap(0, nullptr);
    impl.UpdateDiagnostics();
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

ReleaseStatus Device::TryRelease(const Submission& submission, std::string* error) {
    Impl& impl = *impl_;
    Impl::Record* record = impl.FindRecord(submission.id);
    if (record == nullptr || record->released) {
        if (error != nullptr) {
            *error = "unknown or already released submission";
        }
        return ReleaseStatus::InvalidSubmission;
    }
    if (record->gated && !record->gateReleased) {
        if (error != nullptr) {
            *error = "submission is gated; completion is outstanding";
        }
        return ReleaseStatus::RefusedOutstanding;
    }
    if (!impl.fence || impl.fence->GetCompletedValue() < record->serial) {
        if (error != nullptr) {
            *error = "completion serial is outstanding";
        }
        return ReleaseStatus::RefusedOutstanding;
    }
    impl.ReleaseRecord(*record);
    if (error != nullptr) {
        error->clear();
    }
    return ReleaseStatus::Released;
}

uint64_t Device::CompletedSerial(const Submission& submission) const {
    const Impl& impl = *impl_;
    const Impl::Record* record = impl.FindRecord(submission.id);
    if (record == nullptr || record->released || !impl.fence) {
        return 0;
    }
    return impl.fence->GetCompletedValue();
}

bool Device::IsCompleted(const Submission& submission) const {
    const Impl& impl = *impl_;
    const Impl::Record* record = impl.FindRecord(submission.id);
    if (record == nullptr || record->released || !impl.fence) {
        return false;
    }
    return impl.fence->GetCompletedValue() >= record->serial;
}

Status Device::ClearAndReadback(const ClearReadbackDesc& desc, ReadbackResult* result,
                                std::string* error) {
    if (result == nullptr) {
        return Fail(Status::InvalidArgument, error, "readback result is null");
    }
    Submission submission;
    Status status =
        SubmitClearReadback(desc, SubmissionOptions{}, &submission, error);
    if (status != Status::Ok) {
        return status;
    }
    status = Wait(submission, error);
    if (status != Status::Ok) {
        TryRelease(submission, nullptr);
        return status;
    }
    status = Readback(submission, result, error);
    const ReleaseStatus release = TryRelease(submission, nullptr);
    if (status == Status::Ok && release != ReleaseStatus::Released) {
        return Fail(Status::Error, error, "completed submission was not released");
    }
    return status;
}

}  // namespace rhi
}  // namespace doom3
