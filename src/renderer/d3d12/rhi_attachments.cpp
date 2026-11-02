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
    uint32_t width, uint32_t height, const ClearRow* optimizedClear,
    ComPtr<ID3D12Resource>* texture, uint32_t* tightStride, uint32_t* rowPitch,
    ComPtr<ID3D12Resource>* readback, std::string* error) {
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
    // Supply the first clear this frame will perform as the optimized clear
    // value. A single pass writes each attachment, so the first clear is the
    // only clear RecordDrawPasses issues for it; matching it keeps the debug
    // layer's mismatching-clear diagnostic out of a correct run.
    D3D12_CLEAR_VALUE clearValue{};
    const D3D12_CLEAR_VALUE* optimized = nullptr;
    if (optimizedClear != nullptr) {
        clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        clearValue.Color[0] = optimizedClear->red / 255.0f;
        clearValue.Color[1] = optimizedClear->green / 255.0f;
        clearValue.Color[2] = optimizedClear->blue / 255.0f;
        clearValue.Color[3] = optimizedClear->alpha / 255.0f;
        optimized = &clearValue;
    }
    HRESULT hr = host->Device()->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &textureDescription,
        D3D12_RESOURCE_STATE_RENDER_TARGET, optimized, __uuidof(ID3D12Resource),
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
                                                     float optimizedDepth, bool withStencil,
                                                     ComPtr<ID3D12Resource>* texture,
                                                     std::string* error) {
    const Status dimensions = CheckDimensions(width, height, error);
    if (dimensions != Status::Ok) {
        return dimensions;
    }
    const DXGI_FORMAT depthFormat =
        withStencil ? DXGI_FORMAT_D24_UNORM_S8_UINT : DXGI_FORMAT_D32_FLOAT;
    D3D12_RESOURCE_DESC textureDescription{};
    textureDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDescription.Alignment = 0;
    textureDescription.Width = width;
    textureDescription.Height = height;
    textureDescription.DepthOrArraySize = 1;
    textureDescription.MipLevels = 1;
    textureDescription.Format = depthFormat;
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
    // DrawFrameDesc::depthClear is the value RecordDrawPasses clears with, so
    // declare exactly that value and the clear is an optimized match.
    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = depthFormat;
    clearValue.DepthStencil.Depth = optimizedDepth;
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

namespace {

bool IsBlockFormat(TextureFormat format) {
    return format == TextureFormat::BC4Unorm || format == TextureFormat::BC5Unorm ||
           format == TextureFormat::BC7UnormSrgb || format == TextureFormat::BC7Unorm;
}

uint32_t BytesPerBlock(TextureFormat format) {
    switch (format) {
        case TextureFormat::BC4Unorm:
            return 8;
        case TextureFormat::BC5Unorm:
        case TextureFormat::BC7UnormSrgb:
        case TextureFormat::BC7Unorm:
            return 16;
        default:
            return 0;
    }
}

bool IsSrgb(TextureFormat format) {
    return format == TextureFormat::RGBA8Srgb || format == TextureFormat::BC7UnormSrgb;
}

Status ValidateMip(uint32_t mipIndex, const TextureMip& mip, TextureFormat format,
                   std::string* error) {
    if (mip.width == 0 || mip.height == 0) {
        return Fail(Status::InvalidArgument, error,
                    "texture mip " + std::to_string(mipIndex) + " has a zero dimension");
    }
    const uint32_t rowBytes = d3d12::detail::TextureRowBytes(format, mip.width);
    const uint32_t rows = IsBlockFormat(format) ? (mip.height + 3) / 4 : mip.height;
    const uint64_t expected = static_cast<uint64_t>(rowBytes) * rows;
    if (mip.bytes.size() != expected) {
        return Fail(Status::InvalidArgument, error,
                    "texture mip " + std::to_string(mipIndex) + " holds " +
                        std::to_string(mip.bytes.size()) + " bytes but its " +
                        std::to_string(mip.width) + "x" + std::to_string(mip.height) +
                        " layout needs " + std::to_string(expected));
    }
    return Status::Ok;
}

}  // namespace

DXGI_FORMAT d3d12::detail::NativeTextureFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::RGBA8Unorm:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RGBA8Srgb:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case TextureFormat::R8Unorm:
            return DXGI_FORMAT_R8_UNORM;
        case TextureFormat::BC4Unorm:
            return DXGI_FORMAT_BC4_UNORM;
        case TextureFormat::BC5Unorm:
            return DXGI_FORMAT_BC5_UNORM;
        case TextureFormat::BC7UnormSrgb:
            return DXGI_FORMAT_BC7_UNORM_SRGB;
        case TextureFormat::BC7Unorm:
            return DXGI_FORMAT_BC7_UNORM;
    }
    return DXGI_FORMAT_UNKNOWN;
}

uint32_t d3d12::detail::TextureRowBytes(TextureFormat format, uint32_t width) {
    switch (format) {
        case TextureFormat::RGBA8Unorm:
        case TextureFormat::RGBA8Srgb:
            return width * 4;
        case TextureFormat::R8Unorm:
            return width;
        case TextureFormat::BC4Unorm:
        case TextureFormat::BC5Unorm:
        case TextureFormat::BC7UnormSrgb:
        case TextureFormat::BC7Unorm:
            return ((width + 3) / 4) * BytesPerBlock(format);
    }
    return 0;
}

Status d3d12::detail::Q1bState::CreateBuffer(const BufferDesc& desc, Buffer* buffer,
                                             std::string* error) {
    if (buffer == nullptr) {
        return Fail(Status::InvalidArgument, error, "buffer output is null");
    }
    *buffer = Buffer{};
    if (desc.bytes.empty()) {
        return Fail(Status::InvalidArgument, error, "buffer has no bytes");
    }
    if (desc.usage == BufferUsage::Vertex &&
        desc.bytes.size() % sizeof(Vertex) != 0) {
        return Fail(Status::InvalidArgument, error,
                    "vertex buffer size is not a multiple of the portable vertex stride");
    }
    if (desc.usage == BufferUsage::Index && desc.bytes.size() % 2 != 0) {
        return Fail(Status::InvalidArgument, error,
                    "index buffer size is not a multiple of 16-bit indices");
    }

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    uploadHeap.CreationNodeMask = 1;
    uploadHeap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Alignment = 0;
    description.Width = desc.bytes.size();
    description.Height = 1;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.Format = DXGI_FORMAT_UNKNOWN;
    description.SampleDesc.Count = 1;
    description.SampleDesc.Quality = 0;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    description.Flags = D3D12_RESOURCE_FLAG_NONE;

    GeometryBufferRecord record;
    const HRESULT created = host->Device()->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource),
        reinterpret_cast<void**>(record.resource.GetAddressOf()));
    if (FAILED(created)) {
        return Fail(Status::Error, error,
                    "CreateCommittedResource(geometry buffer) " +
                        d3d12::detail::HrString(created));
    }
    void* mapped = nullptr;
    const HRESULT mapResult = record.resource->Map(0, nullptr, &mapped);
    if (FAILED(mapResult) || mapped == nullptr) {
        return Fail(Status::Error, error,
                    "Map(geometry buffer) " +
                        (mapResult == S_OK
                             ? std::string("returned a null pointer")
                             : d3d12::detail::HrString(mapResult)));
    }
    std::memcpy(mapped, desc.bytes.data(), desc.bytes.size());
    record.resource->Unmap(0, nullptr);
    record.id = nextBufferId++;
    record.usage = desc.usage;
    record.byteCount = static_cast<uint32_t>(desc.bytes.size());
    buffer->id = record.id;
    buffers.emplace(record.id, std::move(record));
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

Status d3d12::detail::Q1bState::CreateTexture(const TextureDesc& desc, Texture* texture,
                                              std::string* error) {
    if (texture == nullptr) {
        return Fail(Status::InvalidArgument, error, "texture output is null");
    }
    *texture = Texture{};
    if (desc.mips.empty()) {
        return Fail(Status::InvalidArgument, error, "texture has no mips");
    }
    const DXGI_FORMAT nativeFormat = d3d12::detail::NativeTextureFormat(desc.format);
    if (nativeFormat == DXGI_FORMAT_UNKNOWN) {
        return Fail(Status::InvalidArgument, error, "unknown texture format");
    }
    uint32_t expectedWidth = desc.mips[0].width;
    uint32_t expectedHeight = desc.mips[0].height;
    for (uint32_t mipIndex = 0; mipIndex < desc.mips.size(); ++mipIndex) {
        const TextureMip& mip = desc.mips[mipIndex];
        const Status valid = ValidateMip(mipIndex, mip, desc.format, error);
        if (valid != Status::Ok) {
            return valid;
        }
        if (mip.width != expectedWidth || mip.height != expectedHeight) {
            return Fail(Status::InvalidArgument, error,
                        "texture mip " + std::to_string(mipIndex) + " is " +
                            std::to_string(mip.width) + "x" + std::to_string(mip.height) +
                            " but the chain expects " + std::to_string(expectedWidth) + "x" +
                            std::to_string(expectedHeight));
        }
        expectedWidth = std::max(1u, expectedWidth / 2);
        expectedHeight = std::max(1u, expectedHeight / 2);
        if (IsBlockFormat(desc.format)) {
            // A block format mip never goes below one 4x4 block.
            expectedWidth = std::max(4u, expectedWidth);
            expectedHeight = std::max(4u, expectedHeight);
        }
    }
    if (desc.mips[0].width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
        desc.mips[0].height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION) {
        return Fail(Status::InvalidArgument, error,
                    "texture dimensions exceed the texture limit");
    }

    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    description.Alignment = 0;
    description.Width = desc.mips[0].width;
    description.Height = desc.mips[0].height;
    description.DepthOrArraySize = 1;
    description.MipLevels = static_cast<UINT16>(desc.mips.size());
    description.Format = nativeFormat;
    description.SampleDesc.Count = 1;
    description.SampleDesc.Quality = 0;
    description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    description.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    defaultHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    defaultHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    defaultHeap.CreationNodeMask = 1;
    defaultHeap.VisibleNodeMask = 1;

    GeometryTextureRecord record;
    HRESULT hr = host->Device()->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, __uuidof(ID3D12Resource),
        reinterpret_cast<void**>(record.resource.GetAddressOf()));
    if (FAILED(hr)) {
        return Fail(Status::Error, error,
                    "CreateCommittedResource(geometry texture) " +
                        d3d12::detail::HrString(hr));
    }

    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(desc.mips.size());
    std::vector<UINT> rowCounts(desc.mips.size(), 0);
    std::vector<UINT64> rowSizes(desc.mips.size(), 0);
    UINT64 totalBytes = 0;
    host->Device()->GetCopyableFootprints(
        &description, 0, static_cast<UINT>(desc.mips.size()), 0, footprints.data(),
        rowCounts.data(), rowSizes.data(), &totalBytes);
    if (totalBytes == 0) {
        return Fail(Status::Error, error, "texture upload layout is empty");
    }

    D3D12_RESOURCE_DESC uploadDescription{};
    uploadDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDescription.Alignment = 0;
    uploadDescription.Width = totalBytes;
    uploadDescription.Height = 1;
    uploadDescription.DepthOrArraySize = 1;
    uploadDescription.MipLevels = 1;
    uploadDescription.Format = DXGI_FORMAT_UNKNOWN;
    uploadDescription.SampleDesc.Count = 1;
    uploadDescription.SampleDesc.Quality = 0;
    uploadDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    uploadDescription.Flags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    uploadHeap.CreationNodeMask = 1;
    uploadHeap.VisibleNodeMask = 1;
    ComPtr<ID3D12Resource> upload;
    hr = host->Device()->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDescription,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource),
        reinterpret_cast<void**>(upload.GetAddressOf()));
    if (FAILED(hr)) {
        return Fail(Status::Error, error,
                    "CreateCommittedResource(texture upload) " +
                        d3d12::detail::HrString(hr));
    }
    void* mapped = nullptr;
    hr = upload->Map(0, nullptr, &mapped);
    if (FAILED(hr) || mapped == nullptr) {
        return Fail(Status::Error, error, "Map(texture upload) failed");
    }
    auto* base = static_cast<uint8_t*>(mapped);
    for (uint32_t mipIndex = 0; mipIndex < desc.mips.size(); ++mipIndex) {
        const TextureMip& mip = desc.mips[mipIndex];
        const auto* source = mip.bytes.data();
        const uint32_t sourceRow = d3d12::detail::TextureRowBytes(desc.format, mip.width);
        const uint32_t rows = rowCounts[mipIndex];
        for (uint32_t row = 0; row < rows; ++row) {
            std::memcpy(base + footprints[mipIndex].Offset +
                            static_cast<uint64_t>(row) *
                                footprints[mipIndex].Footprint.RowPitch,
                        source + static_cast<uint64_t>(row) * sourceRow, sourceRow);
        }
    }
    upload->Unmap(0, nullptr);

    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    hr = host->Device()->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
        reinterpret_cast<void**>(allocator.GetAddressOf()));
    if (FAILED(hr)) {
        return Fail(Status::Error, error,
                    "CreateCommandAllocator(texture upload) " +
                        d3d12::detail::HrString(hr));
    }
    hr = host->Device()->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
        __uuidof(ID3D12GraphicsCommandList),
        reinterpret_cast<void**>(list.GetAddressOf()));
    if (FAILED(hr)) {
        return Fail(Status::Error, error,
                    "CreateCommandList(texture upload) " + d3d12::detail::HrString(hr));
    }
    for (uint32_t mipIndex = 0; mipIndex < desc.mips.size(); ++mipIndex) {
        D3D12_TEXTURE_COPY_LOCATION destination{};
        destination.pResource = record.resource.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destination.SubresourceIndex = mipIndex;
        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = upload.Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source.PlacedFootprint = footprints[mipIndex];
        list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    }
    d3d12::detail::ChangeUse(list.Get(), record.resource.Get(),
                             D3D12_RESOURCE_STATE_COPY_DEST,
                             D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    hr = list->Close();
    if (FAILED(hr)) {
        return Fail(Status::Error, error,
                    "Close(texture upload) " + d3d12::detail::HrString(hr));
    }
    ID3D12CommandList* lists[] = {list.Get()};
    host->Queue()->ExecuteCommandLists(1, lists);
    const uint64_t serial = host->NextSerial();
    if (serial == 0 || FAILED(host->Queue()->Signal(host->Completion(), serial)) ||
        !host->WaitSerial(serial, d3d12::detail::kShutdownTimeoutMilliseconds)) {
        return Fail(Status::Error, error,
                    "texture upload did not complete; the resource was not published");
    }

    record.id = nextTextureId++;
    record.format = desc.format;
    record.nativeFormat = nativeFormat;
    record.width = desc.mips[0].width;
    record.height = desc.mips[0].height;
    record.mipCount = static_cast<uint32_t>(desc.mips.size());
    texture->id = record.id;
    textures.emplace(record.id, std::move(record));
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

Status d3d12::detail::Q1bState::DestroyBuffer(const Buffer& buffer, std::string* error) {
    if (buffer.id == 0 || buffers.find(buffer.id) == buffers.end()) {
        return Fail(Status::InvalidArgument, error, "unknown buffer handle");
    }
    if (HasOutstanding()) {
        return Fail(Status::NotReady, error,
                    "a recorded submission can still reference this buffer");
    }
    buffers.erase(buffer.id);
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

Status d3d12::detail::Q1bState::DestroyTexture(const Texture& texture, std::string* error) {
    if (texture.id == 0 || textures.find(texture.id) == textures.end()) {
        return Fail(Status::InvalidArgument, error, "unknown texture handle");
    }
    if (HasOutstanding()) {
        return Fail(Status::NotReady, error,
                    "a recorded submission can still reference this texture");
    }
    textures.erase(texture.id);
    if (error != nullptr) {
        error->clear();
    }
    return Status::Ok;
}

}  // namespace rhi
}  // namespace doom3
