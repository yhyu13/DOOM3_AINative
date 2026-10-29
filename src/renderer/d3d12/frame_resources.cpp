// Frame slots, descriptor/upload ownership and Q1b readback.
//
// A slot owns one command allocator, one command list, one shader-visible
// descriptor heap and one small upload buffer. A slot is reusable only after
// the completion token of its last submission has been reached. There are four
// slots; a fifth outstanding frame is refused with NotReady instead of
// overwriting memory the device may still read.

#include "d3d12/d3d12_impl.h"

namespace doom3 {
namespace rhi {

using d3d12::detail::Fail;

namespace {

constexpr uint32_t kFrameSlotCount = 4;
constexpr uint32_t kConstantBytes = 256;

}  // namespace

Status d3d12::detail::Q1bState::InitializeSlots(std::string* error) {
    if (!host->Device() || !host->Queue()) {
        return Fail(Status::Error, error, "device is not initialized");
    }
    slots.clear();
    slots.resize(kFrameSlotCount);
    D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
    heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDescription.NumDescriptors = 2;
    heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDescription.NodeMask = 0;

    D3D12_RESOURCE_DESC constantDescription{};
    constantDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    constantDescription.Alignment = 0;
    constantDescription.Width = kConstantBytes;
    constantDescription.Height = 1;
    constantDescription.DepthOrArraySize = 1;
    constantDescription.MipLevels = 1;
    constantDescription.Format = DXGI_FORMAT_UNKNOWN;
    constantDescription.SampleDesc.Count = 1;
    constantDescription.SampleDesc.Quality = 0;
    constantDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    constantDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    uploadHeap.CreationNodeMask = 1;
    uploadHeap.VisibleNodeMask = 1;

    for (uint32_t index = 0; index < kFrameSlotCount; ++index) {
        FrameSlot& slot = slots[index];
        slot.index = index;
        HRESULT hr = host->Device()->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
            reinterpret_cast<void**>(slot.allocator.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error,
                        "CreateCommandAllocator(slot " + std::to_string(index) + ") " +
                            d3d12::detail::HrString(hr));
        }
        hr = host->Device()->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, slot.allocator.Get(), nullptr,
            __uuidof(ID3D12GraphicsCommandList),
            reinterpret_cast<void**>(slot.list.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error,
                        "CreateCommandList(slot " + std::to_string(index) + ") " +
                            d3d12::detail::HrString(hr));
        }
        hr = slot.list->Close();
        if (FAILED(hr)) {
            return Fail(Status::Error, error,
                        "Close(initial command list) " + d3d12::detail::HrString(hr));
        }
        hr = host->Device()->CreateDescriptorHeap(
            &heapDescription, __uuidof(ID3D12DescriptorHeap),
            reinterpret_cast<void**>(slot.heap.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error,
                        "CreateDescriptorHeap(slot " + std::to_string(index) + ") " +
                            d3d12::detail::HrString(hr));
        }
        hr = host->Device()->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &constantDescription,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource),
            reinterpret_cast<void**>(slot.constants.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error,
                        "CreateCommittedResource(slot upload) " +
                            d3d12::detail::HrString(hr));
        }
        hr = slot.constants->Map(0, nullptr, reinterpret_cast<void**>(&slot.mapped));
        if (FAILED(hr)) {
            return Fail(Status::Error, error,
                        "Map(slot upload) " + d3d12::detail::HrString(hr));
        }
        std::memset(slot.mapped, 0, kConstantBytes);
    }
    return Status::Ok;
}

void d3d12::detail::Q1bState::DestroySlots() {
    for (FrameSlot& slot : slots) {
        if (slot.constants && slot.mapped != nullptr) {
            slot.constants->Unmap(0, nullptr);
            slot.mapped = nullptr;
        }
        slot.constants.Reset();
        slot.heap.Reset();
        slot.list.Reset();
        slot.allocator.Reset();
        slot.inUse = false;
    }
    slots.clear();
}

Status d3d12::detail::Q1bState::AcquireSlot(FrameSlot** outSlot,
                                           uint64_t* completedAtAcquire,
                                           std::string* error) {
    if (!host->Completion() || !host->Device()) {
        return Fail(Status::Error, error, "device is not initialized");
    }
    if (slots.empty()) {
        return Fail(Status::Error, error, "frame slots were not initialized");
    }
    for (FrameSlot& slot : slots) {
        if (slot.inUse && host->CompletionReached(slot.serial)) {
            slot.inUse = false;
        }
        if (slot.inUse) {
            continue;
        }
        const uint64_t completed = host->Completion()->GetCompletedValue();
        if (completed == d3d12::detail::kRemovedSentinel) {
            return Fail(Status::DeviceLost, error,
                        "device was removed before a slot was reused");
        }
        const HRESULT allocatorResult = slot.allocator->Reset();
        if (FAILED(allocatorResult)) {
            return Fail(Status::Error, error,
                        "CommandAllocator::Reset(slot " + std::to_string(slot.index) +
                            ") " + d3d12::detail::HrString(allocatorResult));
        }
        const HRESULT listResult = slot.list->Reset(slot.allocator.Get(), nullptr);
        if (FAILED(listResult)) {
            return Fail(Status::Error, error,
                        "GraphicsCommandList::Reset(slot " + std::to_string(slot.index) +
                            ") " + d3d12::detail::HrString(listResult));
        }
        std::memset(slot.mapped, 0, kConstantBytes);
        slot.inUse = true;
        slot.serial = 0;
        if (completedAtAcquire != nullptr) {
            *completedAtAcquire = completed;
        }
        if (outSlot != nullptr) {
            *outSlot = &slot;
        }
        return Status::Ok;
    }
    return Fail(Status::NotReady, error,
                "all " + std::to_string(kFrameSlotCount) +
                    " frame slots are in flight; read back and release a submission "
                    "before submitting again");
}

void d3d12::detail::Q1bState::FinishSlot(FrameSlot& slot, uint64_t serial) {
    slot.serial = serial;
}

bool d3d12::detail::Q1bState::WaitForSerial(uint64_t serial, DWORD timeoutMilliseconds) {
    return host->WaitSerial(serial, timeoutMilliseconds);
}

bool d3d12::detail::Q1bState::HasOutstanding() const {
    for (const FrameSlot& slot : slots) {
        if (slot.inUse && !host->CompletionReached(slot.serial)) {
            return true;
        }
    }
    return false;
}

bool d3d12::detail::Q1bState::DestroyAll() {
    bool safe = true;
    // Wait for every slot before releasing a surface or its chain. A frame
    // that is still in flight may reference the chain buffers.
    for (const FrameSlot& slot : slots) {
        if (slot.inUse && !WaitForSerial(slot.serial, 30000)) {
            safe = false;
        }
    }
    if (!safe) {
        return false;
    }
    std::vector<uint32_t> surfaceIds;
    surfaceIds.reserve(surfaces.size());
    for (const auto& entry : surfaces) {
        surfaceIds.push_back(entry.first);
    }
    for (uint32_t id : surfaceIds) {
        Surface surface;
        surface.id = id;
        if (DestroySurface(surface, nullptr) != Status::Ok) {
            safe = false;
        }
    }
    if (!safe) {
        return false;
    }
    DestroySlots();
    surfaces.clear();
    pipelines.clear();
    shaders.clear();
    return true;
}

Status d3d12::detail::Q1bState::ReadbackAttachment(const Submission& submission,
                                                  uint32_t attachmentIndex,
                                                  ReadbackResult* result,
                                                  std::string* error) {
    using Record = d3d12::detail::SubmissionRecord;
    if (!result) {
        return Fail(Status::InvalidArgument, error, "readback result is null");
    }
    if (host->DeviceRemoved(error)) {
        return Status::DeviceLost;
    }
    Record* record = host->FindHostRecord(submission);
    if (record == nullptr) {
        return Fail(Status::InvalidArgument, error, "unknown or released submission");
    }
    if (!record->signalled) {
        return Fail(Status::Error, error, "submission was not signalled");
    }
    if (!host->CompletionReached(record->serial)) {
        return Fail(Status::NotReady, error,
                    "readback requested before the completion token was reached");
    }
    if (record->attachmentCount == 0 && attachmentIndex == 0) {
        // Q1a clear submissions carry their single image in the original pair.
        return host->ReadbackClear(submission, result, error);
    }
    if (record->attachmentCount == 0 || attachmentIndex >= record->attachmentCount) {
        return Fail(Status::InvalidArgument, error,
                    "attachment index " + std::to_string(attachmentIndex) +
                        " is outside the " + std::to_string(record->attachmentCount) +
                        " attachment(s) of this submission");
    }
    const ComPtr<ID3D12Resource>& readback = record->colorReadbacks[attachmentIndex];
    if (!readback) {
        return Fail(Status::Error, error,
                    "attachment " + std::to_string(attachmentIndex) +
                        " has no readable storage");
    }
    void* mapped = nullptr;
    const HRESULT mapResult = readback->Map(0, nullptr, &mapped);
    if (FAILED(mapResult) || mapped == nullptr) {
        return Fail(Status::Error, error,
                    "ID3D12Resource::Map(attachment " + std::to_string(attachmentIndex) +
                        ") " + d3d12::detail::HrString(mapResult));
    }
    result->width = record->width;
    result->height = record->height;
    result->rowStride = record->colorStrides[attachmentIndex];
    result->adapter = host->Adapter();
    try {
        result->pixels.resize(
            static_cast<size_t>(record->colorStrides[attachmentIndex]) * record->height);
        for (uint32_t y = 0; y < record->height; ++y) {
            std::memcpy(
                result->pixels.data() +
                    static_cast<size_t>(y) * record->colorStrides[attachmentIndex],
                static_cast<const uint8_t*>(mapped) +
                    static_cast<size_t>(y) * record->colorPitches[attachmentIndex],
                record->colorStrides[attachmentIndex]);
        }
    } catch (const std::bad_alloc&) {
        readback->Unmap(0, nullptr);
        *result = ReadbackResult{};
        return Fail(Status::OutOfMemory, error,
                    "caller-owned readback bytes could not be allocated");
    }
    readback->Unmap(0, nullptr);
    if (error) {
        error->clear();
    }
    return Status::Ok;
}

Status d3d12::detail::Q1bState::ReadbackBuffer(const Submission& submission,
                                              BufferResult* result,
                                              std::string* error) {
    using Record = d3d12::detail::SubmissionRecord;
    if (!result) {
        return Fail(Status::InvalidArgument, error, "buffer readback result is null");
    }
    if (host->DeviceRemoved(error)) {
        return Status::DeviceLost;
    }
    Record* record = host->FindHostRecord(submission);
    if (record == nullptr) {
        return Fail(Status::InvalidArgument, error, "unknown or released submission");
    }
    if (!record->bufferReadback || record->bufferElementCount == 0) {
        return Fail(Status::InvalidArgument, error,
                    "submission does not carry a compute result buffer");
    }
    if (!host->CompletionReached(record->serial)) {
        return Fail(Status::NotReady, error,
                    "readback requested before the completion token was reached");
    }
    void* mapped = nullptr;
    const HRESULT mapResult = record->bufferReadback->Map(0, nullptr, &mapped);
    if (FAILED(mapResult) || mapped == nullptr) {
        return Fail(Status::Error, error,
                    "ID3D12Resource::Map(result buffer) " +
                        d3d12::detail::HrString(mapResult));
    }
    result->elementCount = record->bufferElementCount;
    result->elementSize = record->bufferElementSize;
    result->adapter = host->Adapter();
    try {
        const auto* bytes = static_cast<const uint8_t*>(mapped);
        result->bytes.assign(bytes, bytes + record->bufferReadbackBytes);
    } catch (const std::bad_alloc&) {
        record->bufferReadback->Unmap(0, nullptr);
        *result = BufferResult{};
        return Fail(Status::OutOfMemory, error,
                    "caller-owned buffer bytes could not be allocated");
    }
    record->bufferReadback->Unmap(0, nullptr);
    if (error) {
        error->clear();
    }
    return Status::Ok;
}


d3d12::SlotObservation BackendAccess::Slot(const Device& device,
                                           const Submission& submission) {
    d3d12::SlotObservation observation;
    const d3d12::detail::SubmissionRecord* record =
        device.impl_->FindRecord(submission);
    if (record == nullptr) {
        return observation;
    }
    observation.slotIndex = record->slotIndex;
    observation.completedAtAcquire = record->slotCompletedAtAcquire;
    observation.serial = record->serial;
    if (device.impl_->q1b) {
        for (const auto& slot : device.impl_->q1b->slots) {
            if (slot.index == record->slotIndex) {
                observation.inUse = slot.inUse;
                break;
            }
        }
    }
    return observation;
}

}  // namespace rhi
}  // namespace doom3
