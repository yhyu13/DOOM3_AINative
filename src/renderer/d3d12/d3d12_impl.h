#pragma once

// D3D12-private implementation vocabulary shared by the backend translation
// units. Not reachable from engine, scene or material code.

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

namespace d3d12 {
namespace detail {
constexpr DWORD kShutdownTimeoutMilliseconds = 30000;
constexpr size_t kMaxCapturedMessages = 8;
constexpr uint64_t kRemovedSentinel = std::numeric_limits<uint64_t>::max();

// Process-lifetime, non-recycling IDs prevent aliases across devices and
// reinitializations. Saturation is permanent rather than wrapping to an old ID.
inline std::atomic<uint64_t> gNextSubmissionId{1};
inline std::atomic<uint64_t> gNextCompletionToken{1};

inline bool AllocateSubmissionId(uint32_t* id) {
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

inline bool AllocateCompletionToken(uint64_t* token) {
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

inline const char* SeverityName(D3D12_MESSAGE_SEVERITY severity) {
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

inline std::string HrString(HRESULT hr) {
    char buffer[64] = {};
    std::snprintf(buffer, sizeof(buffer), "hr=0x%08lX",
                  static_cast<unsigned long>(static_cast<uint32_t>(hr)));
    return buffer;
}

inline std::string ErrorWith(const char* operation, HRESULT hr) {
    return std::string(operation) + " failed (" + HrString(hr) + ")";
}

inline Status Fail(Status status, std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
    return status;
}

inline std::string WideToUtf8(const wchar_t* value) {
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

inline const char* PrivateStateName(D3D12_RESOURCE_STATES state) {
    switch (state) {
        case D3D12_RESOURCE_STATE_COMMON:
            return "COMMON";
        case D3D12_RESOURCE_STATE_RENDER_TARGET:
            return "RENDER_TARGET";
        case D3D12_RESOURCE_STATE_COPY_SOURCE:
            return "COPY_SOURCE";
        case D3D12_RESOURCE_STATE_COPY_DEST:
            return "COPY_DEST";
        case D3D12_RESOURCE_STATE_DEPTH_WRITE:
            return "DEPTH_WRITE";
        case D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:
            return "PIXEL_SHADER_RESOURCE";
        case D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
            return "UNORDERED_ACCESS";
        default:
            return "UNKNOWN";
    }
}

inline void ChangeUse(ID3D12GraphicsCommandList* list, ID3D12Resource* resource,
                      D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    if (list == nullptr || resource == nullptr || before == after) {
        return;
    }
    D3D12_RESOURCE_BARRIER change{};
    change.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    change.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    change.Transition.pResource = resource;
    change.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    change.Transition.StateBefore = before;
    change.Transition.StateAfter = after;
    list->ResourceBarrier(1, &change);
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



// Q1a submission record. One submission owns everything the device may still
// use; nothing is destroyed until the completion token proves the last use
// ended, or the whole record is quarantined.
struct SubmissionRecord {
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

    // Q1b work. `colors`/`colorReadbacks` are indexed by frame attachment.
    uint32_t attachmentCount = 0;
    std::vector<ComPtr<ID3D12Resource>> colors;
    std::vector<ComPtr<ID3D12Resource>> colorReadbacks;
    std::vector<uint32_t> colorPitches;
    std::vector<uint32_t> colorStrides;
    ComPtr<ID3D12Resource> depth;
    ComPtr<ID3D12DescriptorHeap> colorViews;
    ComPtr<ID3D12DescriptorHeap> depthView;
    ComPtr<ID3D12Resource> buffer;
    ComPtr<ID3D12Resource> bufferReadback;
    uint32_t bufferElementCount = 0;
    uint32_t bufferElementSize = 0;
    uint32_t bufferReadbackBytes = 0;
    uint32_t slotIndex = 0;
    uint64_t slotCompletedAtAcquire = 0;
    uint32_t historyGeneration = 0;
};

// Deliberately never destroyed. If completion cannot be proved, dropping these
// objects could free memory the GPU still reads. A process-lifetime quarantine
// is the honest alternative to a clean-looking teardown.
struct QuarantineStore {
    std::vector<SubmissionRecord> records;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandQueue> gateQueue;
    ComPtr<ID3D12Fence> completion;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIFactory1> factory;
};

inline QuarantineStore* NewQuarantineStore() {
    // A fresh leaked store per quarantined device preserves every unproven
    // object set instead of overwriting an earlier one.
    return new QuarantineStore();
}

// The backend services Q1b state needs. Device::Impl implements this, so Q1b
// state never names the private impl type and no D3D12 vocabulary reaches the
// public header.
struct Q1bHost {
    virtual ~Q1bHost() = default;
    virtual ID3D12Device* Device() = 0;
    virtual ID3D12CommandQueue* Queue() = 0;
    virtual ID3D12Fence* Completion() = 0;
    virtual IDXGIFactory1* Factory() = 0;
    virtual const AdapterInfo& Adapter() = 0;
    virtual uint64_t NextSerial() = 0;
    virtual uint64_t LastSerial() = 0;
    virtual bool CompletionReached(uint64_t serial) = 0;
    virtual bool WaitSerial(uint64_t serial, DWORD milliseconds) = 0;
    virtual bool DeviceRemoved(std::string* error) = 0;
    virtual SubmissionRecord* FindHostRecord(const Submission& submission) = 0;
    virtual SubmissionRecord* AddHostRecord(SubmissionRecord&& record,
                                            std::string* error) = 0;
    virtual Status ReadbackClear(const Submission& submission, ReadbackResult* result,
                                 std::string* error) = 0;
};

struct Q1bState;
}  // namespace detail
}  // namespace d3d12

using Q1aRecord = d3d12::detail::SubmissionRecord;
using d3d12::detail::SubmissionRecord;
using d3d12::detail::QuarantineStore;
using d3d12::detail::NewQuarantineStore;
using d3d12::detail::Fail;
using d3d12::detail::ErrorWith;
using d3d12::detail::HrString;
using d3d12::detail::WideToUtf8;
using d3d12::detail::PrivateStateName;
using d3d12::detail::AllocateSubmissionId;
using d3d12::detail::AllocateCompletionToken;
using d3d12::detail::SeverityName;
using d3d12::detail::kShutdownTimeoutMilliseconds;
using d3d12::detail::kMaxCapturedMessages;
using d3d12::detail::kRemovedSentinel;
using d3d12::detail::kTextureInitialState;
using d3d12::detail::kReadbackInitialState;

struct Device::Impl : public d3d12::detail::Q1bHost {

    // Q1bHost: the services Q1b state uses without naming this impl type.
    ID3D12Device* Device() override { return device.Get(); }
    ID3D12CommandQueue* Queue() override { return queue.Get(); }
    ID3D12Fence* Completion() override { return completion.Get(); }
    IDXGIFactory1* Factory() override { return factory.Get(); }
    const AdapterInfo& Adapter() override { return adapterInfo; }
    uint64_t NextSerial() override {
        if (nextCompletionValue == kRemovedSentinel) {
            return 0;
        }
        return nextCompletionValue++;
    }
    uint64_t LastSerial() override;
    bool CompletionReached(uint64_t serial) override {
        return CompletionValueReached(serial);
    }
    bool WaitSerial(uint64_t serial, DWORD milliseconds) override {
        return WaitForCompletionValue(serial, milliseconds);
    }
    bool DeviceRemoved(std::string* error) override { return Lost(error); }
    d3d12::detail::SubmissionRecord* FindHostRecord(
        const Submission& submission) override {
        return FindRecord(submission);
    }
    d3d12::detail::SubmissionRecord* AddHostRecord(
        d3d12::detail::SubmissionRecord&& record, std::string* error) override {
        const uint32_t id = record.id;
        auto inserted = records.emplace(id, std::move(record));
        if (!inserted.second) {
            if (error != nullptr) {
                *error = "submission identity collision";
            }
            return nullptr;
        }
        return &inserted.first->second;
    }
    Status ReadbackClear(const Submission& submission, ReadbackResult* result,
                         std::string* error) override {
        return Readback(submission, result, error);
    }

    ComPtr<IDXGIFactory1> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandQueue> gateQueue;
    ComPtr<ID3D12Fence> completion;

    AdapterInfo adapterInfo;
    d3d12::DiagnosticReport diagnostics;
    std::map<uint32_t, Q1aRecord> records;
    std::unique_ptr<d3d12::detail::Q1bState> q1b;
    Status RequireQ1b(std::string* error) const {
        if (!initialized || !q1b) {
            return d3d12::detail::Fail(Status::Error, error, "device is not initialized");
        }
        return Status::Ok;
    }
    Status CreateQ1bState(std::string* error);
    bool ShutdownQ1bState();

    uint64_t nextCompletionValue = 1;
    bool initialized = false;
    // Sticky after a teardown whose last use could not be proved. A later
    // Shutdown keeps reporting failure until a new Initialize resets it.
    bool cleanupIncomplete = false;
    // Private fault injection. A simulated removal must behave like a real one
    // for submission, completion, release and teardown decisions.
    bool simulatedRemoval = false;

    bool CompletionValueReached(uint64_t serial) const {
        if (simulatedRemoval) {
            return false;
        }
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
        if (simulatedRemoval) {
            if (error != nullptr) {
                *error = "device removed (fault injected by the private test hook)";
            }
            return true;
        }
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
        record.colors.clear();
        record.colorReadbacks.clear();
        record.colorPitches.clear();
        record.colorStrides.clear();
        record.depth.Reset();
        record.buffer.Reset();
        record.bufferReadback.Reset();
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
        if (simulatedRemoval) {
            return false;
        }
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
        adapterInfo.reportedFacts =
            static_cast<uint32_t>(AdapterFact::VendorId) |
            static_cast<uint32_t>(AdapterFact::DeviceId) |
            static_cast<uint32_t>(AdapterFact::DedicatedMemory);
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
        cleanupIncomplete = false;
        simulatedRemoval = false;
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
        status = CreateQ1bState(error);
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

        // Journal and map storage are prepared before the submission so no
        // allocation happens between ExecuteCommandLists and the published
        // token. The journal is backend-private and read only by private tests.
        const uint32_t id = record.id;
        const uint64_t token = record.token;
        Q1aRecord* storedRecord = nullptr;
        try {
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
            auto inserted = records.emplace(id, std::move(record));
            if (!inserted.second) {
                return Fail(Status::Error, error, "submission identity collision");
            }
            storedRecord = &inserted.first->second;
        } catch (const std::bad_alloc&) {
            return Fail(Status::OutOfMemory, error,
                        "submission bookkeeping could not be allocated");
        }
        Q1aRecord& stored = *storedRecord;

        if (stored.gated) {
            hr = device->CreateFence(
                0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
                reinterpret_cast<void**>(stored.gateFence.GetAddressOf()));
            if (FAILED(hr)) {
                records.erase(id);
                return Fail(Status::Error, error, ErrorWith("CreateFence(gate)", hr));
            }
            hr = queue->Wait(stored.gateFence.Get(), 1);
            if (FAILED(hr)) {
                // Nothing was submitted; removing the record is safe.
                records.erase(id);
                return Fail(Status::Error, error, ErrorWith("CommandQueue::Wait(gate)", hr));
            }
        }
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
        if (cleanupIncomplete) {
            return Status::Error;
        }
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
            if (q1b) {
                q1b.release();
            }
            // Do not destroy objects whose last use is unproven. Move the
            // records and the whole backend object set to a leaked store.
            QuarantineStore* store = NewQuarantineStore();
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
            cleanupIncomplete = true;
            return result;
        }

        if (!ShutdownQ1bState()) {
            QuarantineStore* store = NewQuarantineStore();
            store->device = std::move(device);
            store->queue = std::move(queue);
            store->gateQueue = std::move(gateQueue);
            store->completion = std::move(completion);
            store->adapter = std::move(adapter);
            store->factory = std::move(factory);
            initialized = false;
            cleanupIncomplete = true;
            return Status::Error;
        }
        records.clear();
        ReleaseObjects();
        initialized = false;
        return result;
    }
};

namespace d3d12 {
namespace detail {

struct ShaderRecord {
    uint32_t id = 0;
    ShaderStage stage = ShaderStage::Vertex;
    std::vector<uint8_t> bytecode;
};

struct PipelineRecord {
    uint32_t id = 0;
    ComPtr<ID3D12RootSignature> layout;
    ComPtr<ID3D12PipelineState> state;
    uint32_t colorCount = 0;
    bool depthTest = false;
    bool depthWrite = false;
    bool sampleDepth = false;
    bool compute = false;
};

struct SurfaceRecord {
    uint32_t id = 0;
    HWND window = nullptr;
    std::wstring className;
    ComPtr<IDXGISwapChain3> chain;
    ComPtr<ID3D12DescriptorHeap> viewHeap;
    std::vector<ComPtr<ID3D12Resource>> backBuffers;
    uint32_t width = 0;
    uint32_t height = 0;
    bool minimized = false;
    uint64_t historyGeneration = 0;
};

struct FrameSlot {
    uint32_t index = 0;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12DescriptorHeap> heap;
    ComPtr<ID3D12Resource> constants;
    uint8_t* mapped = nullptr;
    bool inUse = false;
    uint64_t serial = 0;
};

}  // namespace detail
}  // namespace d3d12

namespace d3d12 {
namespace detail {

struct Q1bState {

    explicit Q1bState(Q1bHost& host);
    ~Q1bState();

    Status CreateShaderModule(const ShaderModuleDesc& desc, ShaderModule* module,
                              std::string* error);
    Status CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, Pipeline* pipeline,
                                  std::string* error);
    Status CreateComputePipeline(const ComputePipelineDesc& desc, Pipeline* pipeline,
                                 std::string* error);
    Status SubmitDrawFrame(const DrawFrameDesc& desc, Submission* submission,
                           std::string* error, bool gated = false);
    Status SubmitCompute(const ComputeDispatchDesc& desc, Submission* submission,
                         std::string* error);
    Status ReadbackAttachment(const Submission& submission, uint32_t attachmentIndex,
                              ReadbackResult* result, std::string* error);
    Status ReadbackBuffer(const Submission& submission, BufferResult* result,
                          std::string* error);
    Status CreateSurface(const SurfaceDesc& desc, Surface* surface, std::string* error);
    Status SubmitSurfaceFrame(const Surface& surface, const ClearRow& color,
                              Submission* submission, std::string* error);
    Status ResizeSurface(const Surface& surface, uint32_t width, uint32_t height,
                         std::string* error);
    Status DestroySurface(const Surface& surface, std::string* error);
    bool DestroyAll();
    bool HasOutstanding() const;
    Status InitializeSlots(std::string* error);
    void DestroySlots();

    Status CompileShader(const ShaderModuleDesc& desc, ShaderRecord* record,
                         std::string* error);
    Status CreateColorAttachment(uint32_t width, uint32_t height,
                                 ComPtr<ID3D12Resource>* texture, uint32_t* tightStride,
                                 uint32_t* rowPitch, ComPtr<ID3D12Resource>* readback,
                                 std::string* error);
    Status CreateDepthAttachment(uint32_t width, uint32_t height,
                                 ComPtr<ID3D12Resource>* texture, std::string* error);
    Status RecordDrawPasses(ID3D12GraphicsCommandList* list, FrameSlot& slot,
                            const DrawFrameDesc& desc,
                            d3d12::detail::SubmissionRecord& record,
                            std::string* error);
    Status CreateComputeResources(const ComputeDispatchDesc& desc, FrameSlot& slot,
                                  ComPtr<ID3D12Resource>* buffer,
                                  ComPtr<ID3D12Resource>* readback,
                                  uint32_t* readbackBytes, std::string* error);
    Status AcquireSlot(FrameSlot** slot, uint64_t* completedAtAcquire, std::string* error);
    void FinishSlot(FrameSlot& slot, uint64_t serial);
    bool WaitForSerial(uint64_t serial, DWORD timeoutMilliseconds);

    Q1bHost* host = nullptr;
    uint32_t nextPipelineId = 1;
    uint32_t nextSurfaceId = 1;
    uint32_t nextShaderId = 1;
    std::map<uint32_t, ShaderRecord> shaders;
    std::map<uint32_t, PipelineRecord> pipelines;
    std::map<uint32_t, SurfaceRecord> surfaces;
    std::vector<FrameSlot> slots;
};

}  // namespace detail
}  // namespace d3d12

}  // namespace rhi
}  // namespace doom3
