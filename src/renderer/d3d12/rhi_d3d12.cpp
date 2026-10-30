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
#include "d3d12/d3d12_impl.h"

namespace doom3 {
namespace rhi {


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
    observation.completedValue = completed;
    observation.deviceRemoved = completed == kRemovedSentinel;
    return observation;
}

const char* ToString(ShaderStage value) {
    switch (value) {
    case ShaderStage::Vertex:
        return "vertex";
    case ShaderStage::Pixel:
        return "pixel";
    case ShaderStage::Compute:
        return "compute";
    }
    return "unknown";
}

namespace d3d12 {
namespace detail {

Q1bState::Q1bState(Q1bHost& services) : host(&services) {}

Q1bState::~Q1bState() { DestroySlots(); }

}  // namespace detail
}  // namespace d3d12

Status Device::Impl::CreateQ1bState(std::string* error) {
    try {
        q1b = std::make_unique<d3d12::detail::Q1bState>(*this);
    } catch (const std::bad_alloc&) {
        return d3d12::detail::Fail(Status::OutOfMemory, error,
                                   "Q1b backend state could not be allocated");
    }
    const Status slots = q1b->InitializeSlots(error);
    if (slots != Status::Ok) {
        q1b.reset();
        return slots;
    }
    return Status::Ok;
}

bool Device::Impl::ShutdownQ1bState() {
    if (!q1b) {
        return true;
    }
    if (!q1b->DestroyAll()) {
        // Ownership could not be proved; leak the Q1b objects rather than
        // releasing memory the device may still use.
        q1b.release();
        return false;
    }
    q1b.reset();
    return true;
}


Status Device::CreateShaderModule(const ShaderModuleDesc& desc, ShaderModule* module,
                                  std::string* error) {
    if (module == nullptr) {
        return d3d12::detail::Fail(Status::InvalidArgument, error,
                                   "shader module output is null");
    }
    *module = ShaderModule{};
    const Status ready = impl_->RequireQ1b(error);
    if (ready != Status::Ok) {
        return ready;
    }
    return impl_->q1b->CreateShaderModule(desc, module, error);
}

Status Device::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, Pipeline* pipeline,
                                      std::string* error) {
    if (pipeline == nullptr) {
        return d3d12::detail::Fail(Status::InvalidArgument, error,
                                   "pipeline output is null");
    }
    *pipeline = Pipeline{};
    const Status ready = impl_->RequireQ1b(error);
    if (ready != Status::Ok) {
        return ready;
    }
    return impl_->q1b->CreateGraphicsPipeline(desc, pipeline, error);
}

Status Device::CreateComputePipeline(const ComputePipelineDesc& desc, Pipeline* pipeline,
                                     std::string* error) {
    if (pipeline == nullptr) {
        return d3d12::detail::Fail(Status::InvalidArgument, error,
                                   "pipeline output is null");
    }
    *pipeline = Pipeline{};
    const Status ready = impl_->RequireQ1b(error);
    if (ready != Status::Ok) {
        return ready;
    }
    return impl_->q1b->CreateComputePipeline(desc, pipeline, error);
}

Status Device::SubmitDrawFrame(const DrawFrameDesc& desc, Submission* submission,
                               std::string* error) {
    if (submission == nullptr) {
        return d3d12::detail::Fail(Status::InvalidArgument, error,
                                   "submission output is null");
    }
    *submission = Submission{};
    const Status ready = impl_->RequireQ1b(error);
    if (ready != Status::Ok) {
        return ready;
    }
    return impl_->q1b->SubmitDrawFrame(desc, submission, error);
}

Status Device::SubmitCompute(const ComputeDispatchDesc& desc, Submission* submission,
                             std::string* error) {
    if (submission == nullptr) {
        return d3d12::detail::Fail(Status::InvalidArgument, error,
                                   "submission output is null");
    }
    *submission = Submission{};
    const Status ready = impl_->RequireQ1b(error);
    if (ready != Status::Ok) {
        return ready;
    }
    return impl_->q1b->SubmitCompute(desc, submission, error);
}

Status Device::ReadbackAttachment(const Submission& submission, uint32_t attachmentIndex,
                                  ReadbackResult* result, std::string* error) {
    if (result == nullptr) {
        return d3d12::detail::Fail(Status::InvalidArgument, error,
                                   "readback result is null");
    }
    *result = ReadbackResult{};
    const Status ready = impl_->RequireQ1b(error);
    if (ready != Status::Ok) {
        return ready;
    }
    return impl_->q1b->ReadbackAttachment(submission, attachmentIndex, result, error);
}

Status Device::ReadbackBuffer(const Submission& submission, BufferResult* result,
                              std::string* error) {
    if (result == nullptr) {
        return d3d12::detail::Fail(Status::InvalidArgument, error,
                                   "buffer readback result is null");
    }
    *result = BufferResult{};
    const Status ready = impl_->RequireQ1b(error);
    if (ready != Status::Ok) {
        return ready;
    }
    return impl_->q1b->ReadbackBuffer(submission, result, error);
}

Status Device::CreateSurface(const SurfaceDesc& desc, Surface* surface, std::string* error) {
    if (surface == nullptr) {
        return d3d12::detail::Fail(Status::InvalidArgument, error,
                                   "surface output is null");
    }
    *surface = Surface{};
    const Status ready = impl_->RequireQ1b(error);
    if (ready != Status::Ok) {
        return ready;
    }
    return impl_->q1b->CreateSurface(desc, surface, error);
}

Status Device::SubmitSurfaceFrame(const Surface& surface, const ClearRow& color,
                                  Submission* submission, std::string* error) {
    if (submission == nullptr) {
        return d3d12::detail::Fail(Status::InvalidArgument, error,
                                   "submission output is null");
    }
    *submission = Submission{};
    const Status ready = impl_->RequireQ1b(error);
    if (ready != Status::Ok) {
        return ready;
    }
    return impl_->q1b->SubmitSurfaceFrame(surface, color, submission, error);
}

Status Device::ResizeSurface(const Surface& surface, uint32_t width, uint32_t height,
                             std::string* error) {
    const Status ready = impl_->RequireQ1b(error);
    if (ready != Status::Ok) {
        return ready;
    }
    return impl_->q1b->ResizeSurface(surface, width, height, error);
}

Status Device::DestroySurface(const Surface& surface, std::string* error) {
    const Status ready = impl_->RequireQ1b(error);
    if (ready != Status::Ok) {
        return ready;
    }
    return impl_->q1b->DestroySurface(surface, error);
}

uint64_t Device::Impl::LastSerial() {
    uint64_t last = 0;
    if (q1b) {
        for (const auto& slot : q1b->slots) {
            if (slot.inUse && slot.serial > last) {
                last = slot.serial;
            }
        }
    }
    return last;
}

void BackendAccess::InjectDeviceRemoval(Device& device, bool removed) {
    device.impl_->simulatedRemoval = removed;
}

ID3D12Device* BackendAccess::NativeDevice(Device& device) {
    return (device.impl_ && device.impl_->initialized) ? device.impl_->device.Get()
                                                        : nullptr;
}

}  // namespace rhi
}  // namespace doom3
