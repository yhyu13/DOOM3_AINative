// Colour and depth attachment storage for Q1b.
//
// A colour attachment is an RGBA8 texture created for colour use plus a
// CPU-readable buffer with the backend's own row pitch. Tightly packed bytes
// cross the public boundary; the pitch never does. Depth storage is D32_FLOAT
// with depth use, and is sampled by a later pass through a read view.

#include "d3d12/d3d12_impl.h"

namespace doom3 {
namespace rhi {

using d3d12::detail::Fail;

namespace {

Status CheckDimensions(uint32_t width, uint32_t height, std::string* error) {
    if (width == 0 || height == 0) {
        return Fail(Status::InvalidArgument, error, "attachment dimensions are zero");
    }
    if (width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
        height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION) {
        return Fail(Status::InvalidArgument, error,
                    "attachment dimensions exceed the texture limit");
    }
    if (width > std::numeric_limits<uint32_t>::max() / 4) {
        return Fail(Status::InvalidArgument, error, "row stride overflows uint32");
    }
    return Status::Ok;
}

}  // namespace

Status d3d12::detail::Q1bState::CreateColorAttachment(
    uint32_t width, uint32_t height, ComPtr<ID3D12Resource>* texture,
    uint32_t* tightStride, uint32_t* rowPitch, ComPtr<ID3D12Resource>* readback,
    std::string* error) {
    const Status dimensions = CheckDimensions(width, height, error);
    if (dimensions != Status::Ok) {
        return dimensions;
    }
    *tightStride = width * 4;

    D3D12_RESOURCE_DESC textureDescription{};
    textureDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDescription.Alignment = 0;
    textureDescription.Width = width;
    textureDescription.Height = height;
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
    HRESULT hr = host->Device()->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &textureDescription,
        D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, __uuidof(ID3D12Resource),
        reinterpret_cast<void**>(texture->GetAddressOf()));
    if (FAILED(hr)) {
        return Fail(Status::Error, error,
                    "CreateCommittedResource(colour attachment) " +
                        d3d12::detail::HrString(hr));
    }

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rowCount = 0;
    UINT64 rowSizeInBytes = 0;
    UINT64 totalBytes = 0;
    host->Device()->GetCopyableFootprints(&textureDescription, 0, 1, 0, &footprint,
                                         &rowCount, &rowSizeInBytes, &totalBytes);
    *rowPitch = footprint.Footprint.RowPitch;
    if (*rowPitch < *tightStride || rowCount != height || totalBytes == 0) {
        texture->Reset();
        return Fail(Status::Error, error,
                    "GetCopyableFootprints returned an unexpected colour layout");
    }
    const UINT64 rowSpan =
        static_cast<UINT64>(*rowPitch) * static_cast<UINT64>(height);
    const UINT64 allocationBytes = std::max(totalBytes, rowSpan);

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
    hr = host->Device()->CreateCommittedResource(
        &readbackHeap, D3D12_HEAP_FLAG_NONE, &bufferDescription,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, __uuidof(ID3D12Resource),
        reinterpret_cast<void**>(readback->GetAddressOf()));
    if (FAILED(hr)) {
        texture->Reset();
        return Fail(Status::Error, error,
                    "CreateCommittedResource(colour readback) " +
                        d3d12::detail::HrString(hr));
    }
    return Status::Ok;
}

Status d3d12::detail::Q1bState::CreateDepthAttachment(uint32_t width, uint32_t height,
                                                     ComPtr<ID3D12Resource>* texture,
                                                     std::string* error) {
    const Status dimensions = CheckDimensions(width, height, error);
    if (dimensions != Status::Ok) {
        return dimensions;
    }
    D3D12_RESOURCE_DESC textureDescription{};
    textureDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDescription.Alignment = 0;
    textureDescription.Width = width;
    textureDescription.Height = height;
    textureDescription.DepthOrArraySize = 1;
    textureDescription.MipLevels = 1;
    textureDescription.Format = DXGI_FORMAT_D32_FLOAT;
    textureDescription.SampleDesc.Count = 1;
    textureDescription.SampleDesc.Quality = 0;
    textureDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    defaultHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    defaultHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    defaultHeap.CreationNodeMask = 1;
    defaultHeap.VisibleNodeMask = 1;
    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;
    const HRESULT hr = host->Device()->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &textureDescription,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, __uuidof(ID3D12Resource),
        reinterpret_cast<void**>(texture->GetAddressOf()));
    if (FAILED(hr)) {
        return Fail(Status::Error, error,
                    "CreateCommittedResource(depth attachment) " +
                        d3d12::detail::HrString(hr));
    }
    return Status::Ok;
}

}  // namespace rhi
}  // namespace doom3
