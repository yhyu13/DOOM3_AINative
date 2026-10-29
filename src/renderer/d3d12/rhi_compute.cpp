// Compute slice: compute pipeline, writable structured buffer, dispatch and
// result readback. One dispatch is recorded per submission and the result is
// copied after an explicit declared use change.

#include "d3d12/d3d12_impl.h"

namespace doom3 {
namespace rhi {

using d3d12::detail::ChangeUse;
using d3d12::detail::Fail;

namespace {

ComPtr<ID3D12RootSignature> CreateComputeLayout(ID3D12Device* device,
                                                std::string* error) {
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0;
    range.RegisterSpace = 0;
    range.OffsetInDescriptorsFromTableStart = 0;
    D3D12_ROOT_PARAMETER parameter{};
    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameter.DescriptorTable.NumDescriptorRanges = 1;
    parameter.DescriptorTable.pDescriptorRanges = &range;
    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC description{};
    description.NumParameters = 1;
    description.pParameters = &parameter;
    description.NumStaticSamplers = 0;
    description.pStaticSamplers = nullptr;
    description.Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;
    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;
    const HRESULT hr = D3D12SerializeRootSignature(
        &description, D3D_ROOT_SIGNATURE_VERSION_1, serialized.GetAddressOf(),
        errors.GetAddressOf());
    if (FAILED(hr)) {
        Fail(Status::Error, error,
             "D3D12SerializeRootSignature(compute) " + d3d12::detail::HrString(hr));
        return ComPtr<ID3D12RootSignature>();
    }
    ComPtr<ID3D12RootSignature> layout;
    const HRESULT create = device->CreateRootSignature(
        0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
        __uuidof(ID3D12RootSignature), reinterpret_cast<void**>(layout.GetAddressOf()));
    if (FAILED(create)) {
        Fail(Status::Error, error,
             "CreateRootSignature(compute) " + d3d12::detail::HrString(create));
        return ComPtr<ID3D12RootSignature>();
    }
    return layout;
}

}  // namespace

Status d3d12::detail::Q1bState::CreateComputePipeline(const ComputePipelineDesc& desc,
                                                     Pipeline* pipeline,
                                                     std::string* error) {
    const auto shader = shaders.find(desc.computeShader.id);
    if (shader == shaders.end() || shader->second.stage != ShaderStage::Compute) {
        return Fail(Status::InvalidArgument, error,
                    "compute pipeline handle does not name a compute module");
    }
    ComPtr<ID3D12RootSignature> layout = CreateComputeLayout(host->Device(), error);
    if (!layout) {
        return Status::Error;
    }
    D3D12_COMPUTE_PIPELINE_STATE_DESC state{};
    state.pRootSignature = layout.Get();
    state.CS.pShaderBytecode = shader->second.bytecode.data();
    state.CS.BytecodeLength = shader->second.bytecode.size();
    state.NodeMask = 0;
    state.CachedPSO.pCachedBlob = nullptr;
    state.CachedPSO.CachedBlobSizeInBytes = 0;
    state.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    PipelineRecord record;
    record.id = nextPipelineId++;
    record.layout = layout;
    record.compute = true;
    const HRESULT hr = host->Device()->CreateComputePipelineState(
        &state, __uuidof(ID3D12PipelineState),
        reinterpret_cast<void**>(record.state.GetAddressOf()));
    if (FAILED(hr)) {
        return Fail(Status::Error, error,
                    "CreateComputePipelineState " + d3d12::detail::HrString(hr));
    }
    pipeline->id = record.id;
    pipelines.emplace(record.id, std::move(record));
    if (error) {
        error->clear();
    }
    return Status::Ok;
}

Status d3d12::detail::Q1bState::CreateComputeResources(
    const ComputeDispatchDesc& desc, FrameSlot& /*slot*/, ComPtr<ID3D12Resource>* buffer,
    ComPtr<ID3D12Resource>* readback, uint32_t* readbackBytes, std::string* error) {
    if (desc.elementCount == 0) {
        return Fail(Status::InvalidArgument, error, "compute element count is zero");
    }
    if (desc.elementCount > std::numeric_limits<uint32_t>::max() / 4) {
        return Fail(Status::InvalidArgument, error, "compute buffer size overflows");
    }
    const uint64_t bytes = static_cast<uint64_t>(desc.elementCount) * 4;
    D3D12_RESOURCE_DESC bufferDescription{};
    bufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDescription.Alignment = 0;
    bufferDescription.Width = bytes;
    bufferDescription.Height = 1;
    bufferDescription.DepthOrArraySize = 1;
    bufferDescription.MipLevels = 1;
    bufferDescription.Format = DXGI_FORMAT_UNKNOWN;
    bufferDescription.SampleDesc.Count = 1;
    bufferDescription.SampleDesc.Quality = 0;
    bufferDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    defaultHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    defaultHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    defaultHeap.CreationNodeMask = 1;
    defaultHeap.VisibleNodeMask = 1;
    HRESULT hr = host->Device()->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &bufferDescription,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, __uuidof(ID3D12Resource),
        reinterpret_cast<void**>(buffer->GetAddressOf()));
    if (FAILED(hr)) {
        return Fail(Status::Error, error,
                    "CreateCommittedResource(compute buffer) " +
                        d3d12::detail::HrString(hr));
    }
    D3D12_RESOURCE_DESC readbackDescription = bufferDescription;
    readbackDescription.Flags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_HEAP_PROPERTIES readbackHeap{};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
    readbackHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    readbackHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    readbackHeap.CreationNodeMask = 1;
    readbackHeap.VisibleNodeMask = 1;
    hr = host->Device()->CreateCommittedResource(
        &readbackHeap, D3D12_HEAP_FLAG_NONE, &readbackDescription,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, __uuidof(ID3D12Resource),
        reinterpret_cast<void**>(readback->GetAddressOf()));
    if (FAILED(hr)) {
        return Fail(Status::Error, error,
                    "CreateCommittedResource(compute readback) " +
                        d3d12::detail::HrString(hr));
    }
    *readbackBytes = static_cast<uint32_t>(bytes);
    return Status::Ok;
}

Status d3d12::detail::Q1bState::SubmitCompute(const ComputeDispatchDesc& desc,
                                             Submission* submission,
                                             std::string* error) {
    if (host->DeviceRemoved(error)) {
        return Status::DeviceLost;
    }
    if (desc.groupsX == 0 || desc.groupsY == 0 || desc.groupsZ == 0) {
        return Fail(Status::InvalidArgument, error,
                    "compute dispatch group counts must be non-zero");
    }
    const auto pipeline = pipelines.find(desc.pipeline.id);
    if (pipeline == pipelines.end() || !pipeline->second.compute) {
        return Fail(Status::InvalidArgument, error,
                    "compute dispatch names an unknown compute pipeline");
    }
    if (desc.elementCount > D3D12_REQ_BUFFER_RESOURCE_TEXEL_COUNT_2_TO_EXP) {
        return Fail(Status::InvalidArgument, error,
                    "compute element count exceeds the structured buffer limit");
    }
    uint64_t completedAtAcquire = 0;
    FrameSlot* slot = nullptr;
    const Status acquired = AcquireSlot(&slot, &completedAtAcquire, error);
    if (acquired != Status::Ok) {
        return acquired;
    }

    d3d12::detail::SubmissionRecord record;
    if (!d3d12::detail::AllocateSubmissionId(&record.id) ||
        !d3d12::detail::AllocateCompletionToken(&record.token)) {
        return Fail(Status::Error, error, "submission identity space exhausted");
    }
    record.serial = host->NextSerial();
    if (record.serial == 0) {
        return Fail(Status::Error, error, "completion value space exhausted");
    }
    record.slotIndex = slot->index;
    record.slotCompletedAtAcquire = completedAtAcquire;
    record.bufferElementCount = desc.elementCount;
    record.bufferElementSize = 4;
    uint32_t readbackBytes = 0;
    const Status created = CreateComputeResources(
        desc, *slot, &record.buffer, &record.bufferReadback, &readbackBytes, error);
    if (created != Status::Ok) {
        return created;
    }
    record.bufferReadbackBytes = readbackBytes;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
    uav.Format = DXGI_FORMAT_UNKNOWN;
    uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uav.Buffer.FirstElement = 0;
    uav.Buffer.NumElements = desc.elementCount;
    uav.Buffer.StructureByteStride = 4;
    uav.Buffer.CounterOffsetInBytes = 0;
    uav.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    host->Device()->CreateUnorderedAccessView(
        record.buffer.Get(), nullptr, &uav,
        slot->heap->GetCPUDescriptorHandleForHeapStart());

    ID3D12DescriptorHeap* heaps[] = {slot->heap.Get()};
    slot->list->SetDescriptorHeaps(1, heaps);
    slot->list->SetComputeRootSignature(pipeline->second.layout.Get());
    slot->list->SetPipelineState(pipeline->second.state.Get());
    slot->list->SetComputeRootDescriptorTable(
        0, slot->heap->GetGPUDescriptorHandleForHeapStart());
    slot->list->Dispatch(desc.groupsX, desc.groupsY, desc.groupsZ);
    ChangeUse(slot->list.Get(), record.buffer.Get(),
              D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
              D3D12_RESOURCE_STATE_COPY_SOURCE);
    slot->list->CopyBufferRegion(record.bufferReadback.Get(), 0, record.buffer.Get(), 0,
                                 readbackBytes);
    const HRESULT close = slot->list->Close();
    if (FAILED(close)) {
        return Fail(Status::Error, error,
                    "ID3D12GraphicsCommandList::Close(compute) " +
                        d3d12::detail::HrString(close));
    }
    record.journal.push_back(d3d12::JournalEntry{
        "compute-buffer", "UNORDERED_ACCESS", "COPY_SOURCE", "dispatch then readback"});
    d3d12::detail::SubmissionRecord* storedPtr =
        host->AddHostRecord(std::move(record), error);
    if (storedPtr == nullptr) {
        return Fail(Status::Error, error, "submission identity collision");
    }
    d3d12::detail::SubmissionRecord& stored = *storedPtr;
    FinishSlot(*slot, stored.serial);
    ID3D12CommandList* lists[] = {slot->list.Get()};
    host->Queue()->ExecuteCommandLists(1, lists);
    const HRESULT signalled =
        host->Queue()->Signal(host->Completion(), stored.serial);
    if (FAILED(signalled)) {
        stored.signalled = false;
        return Fail(Status::Error, error,
                    "CommandQueue::Signal(compute) " +
                        d3d12::detail::HrString(signalled));
    }
    stored.signalled = true;
    submission->id = stored.id;
    submission->completionToken = stored.token;
    if (error) {
        error->clear();
    }
    return Status::Ok;
}

}  // namespace rhi
}  // namespace doom3
