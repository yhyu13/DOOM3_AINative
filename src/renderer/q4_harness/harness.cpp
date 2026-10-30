#include "harness_impl.h"

#include "image_io.h"

#include "rhi/hybrid_opaque.h"
#include "rhi/indirect_lighting.h"
#include "rhi/lighting.h"
#include "rhi/materials.h"
#include "rhi/reconstruction.h"
#include "rhi/residency.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sstream>

namespace q4 {

namespace rhi = doom3::rhi;

namespace {

using namespace detail;

namespace fs = std::filesystem;

rhi::ChannelState ToChannelState(const ChannelInfo& info) {
    if (info.state == "authored") {
        return rhi::ChannelState::Authored;
    }
    if (info.state.rfind("derived:", 0) == 0) {
        return rhi::ChannelState::Derived;
    }
    if (info.state == "unavailable") {
        return rhi::ChannelState::Unavailable;
    }
    return rhi::ChannelState::Absent;
}

uint64_t DdsBytes(const DdsImage& dds) {
    uint64_t bytes = 0;
    for (const DdsMip& mip : dds.mips) {
        bytes += mip.size;
    }
    return bytes;
}

}  // namespace

namespace detail {

bool Fail(const char* operation, HRESULT hr, std::string* error) {
    if (error != nullptr) {
        char buffer[160] = {};
        std::snprintf(buffer, sizeof(buffer), "%s failed (hr=0x%08lX)", operation,
                      static_cast<unsigned long>(static_cast<uint32_t>(hr)));
        *error = buffer;
    }
    return false;
}

bool FailMessage(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

void Transition(ID3D12GraphicsCommandList* list, ID3D12Resource* resource,
                D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    if (list == nullptr || resource == nullptr || before == after) {
        return;
    }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    list->ResourceBarrier(1, &barrier);
}

D3D12_HEAP_PROPERTIES Heap(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = type;
    heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;
    return heap;
}

D3D12_RESOURCE_DESC BufferDesc(uint64_t bytes) {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = bytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    return desc;
}

D3D12_RESOURCE_DESC TextureDesc(uint32_t width, uint32_t height, uint32_t mips,
                                DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags) {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = static_cast<UINT16>(mips);
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = flags;
    return desc;
}

}  // namespace detail

D3D12Harness::Impl::~Impl() {
    if (constantMapped != nullptr && constantBuffer != nullptr) {
        constantBuffer->Unmap(0, nullptr);
        constantMapped = nullptr;
    }
    if (fenceEvent != nullptr) {
        CloseHandle(fenceEvent);
        fenceEvent = nullptr;
    }
}

bool D3D12Harness::Impl::SignalAndWait(std::string* error) {
    const UINT64 value = ++fenceValue;
    HRESULT hr = queue->Signal(fence.Get(), value);
    if (FAILED(hr)) {
        return detail::Fail("ID3D12CommandQueue::Signal", hr, error);
    }
    if (fence->GetCompletedValue() < value) {
        hr = fence->SetEventOnCompletion(value, fenceEvent);
        if (FAILED(hr)) {
            return detail::Fail("ID3D12Fence::SetEventOnCompletion", hr, error);
        }
        const DWORD wait = WaitForSingleObject(fenceEvent, 60000);
        if (wait != WAIT_OBJECT_0) {
            return detail::FailMessage(
                error, "the GPU submission did not complete within 60 s");
        }
    }
    return true;
}

bool D3D12Harness::Impl::ExecuteAndWait(std::string* error) {
    HRESULT hr = list->Close();
    if (FAILED(hr)) {
        return detail::Fail("ID3D12GraphicsCommandList::Close", hr, error);
    }
    ID3D12CommandList* lists[] = {list.Get()};
    queue->ExecuteCommandLists(1, lists);
    return SignalAndWait(error);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Harness::Impl::CpuHandle(uint32_t index) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        srvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * srvSize;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12Harness::Impl::GpuHandle(uint32_t index) const {
    D3D12_GPU_DESCRIPTOR_HANDLE handle =
        srvHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(index) * srvSize;
    return handle;
}

bool D3D12Harness::Impl::CreateTexture(
    ID3D12GraphicsCommandList* uploadList, const DdsImage& dds,
    DXGI_FORMAT srvFormat, ComPtr<ID3D12Resource>* texture,
    std::vector<ComPtr<ID3D12Resource>>* uploads, std::string* error) {
    (void)srvFormat;
    const D3D12_RESOURCE_DESC textureDesc =
        detail::TextureDesc(dds.width, dds.height, dds.mipCount, dds.format,
                            D3D12_RESOURCE_FLAG_NONE);
    const D3D12_HEAP_PROPERTIES heap = detail::Heap(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(texture->GetAddressOf()));
    if (FAILED(hr)) {
        return detail::Fail("CreateCommittedResource(texture)", hr, error);
    }

    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(dds.mipCount);
    std::vector<UINT> rowCounts(dds.mipCount);
    std::vector<UINT64> rowSizes(dds.mipCount);
    UINT64 totalBytes = 0;
    device->GetCopyableFootprints(&textureDesc, 0, dds.mipCount, 0,
                                  footprints.data(), rowCounts.data(),
                                  rowSizes.data(), &totalBytes);
    ComPtr<ID3D12Resource> upload;
    const D3D12_HEAP_PROPERTIES uploadHeap =
        detail::Heap(D3D12_HEAP_TYPE_UPLOAD);
    const D3D12_RESOURCE_DESC uploadDesc = detail::BufferDesc(totalBytes);
    hr = device->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(upload.GetAddressOf()));
    if (FAILED(hr)) {
        return detail::Fail("CreateCommittedResource(texture upload)", hr, error);
    }
    void* mapped = nullptr;
    hr = upload->Map(0, nullptr, &mapped);
    if (FAILED(hr)) {
        return detail::Fail("ID3D12Resource::Map(texture upload)", hr, error);
    }
    auto* destination = static_cast<uint8_t*>(mapped);
    for (uint32_t mip = 0; mip < dds.mipCount; ++mip) {
        const DdsMip& source = dds.mips[mip];
        const auto* sourceBytes = dds.data.data() + source.offset;
        uint8_t* target = destination + footprints[mip].Offset;
        if (source.rows != rowCounts[mip] || rowSizes[mip] > source.rowPitch) {
            upload->Unmap(0, nullptr);
            return detail::FailMessage(
                error, "DDS mip layout does not match the GPU copy layout");
        }
        for (UINT row = 0; row < rowCounts[mip]; ++row) {
            std::memcpy(target + static_cast<size_t>(row) *
                                     footprints[mip].Footprint.RowPitch,
                        sourceBytes + static_cast<size_t>(row) * source.rowPitch,
                        static_cast<size_t>(rowSizes[mip]));
        }
    }
    upload->Unmap(0, nullptr);

    for (uint32_t mip = 0; mip < dds.mipCount; ++mip) {
        D3D12_TEXTURE_COPY_LOCATION target{};
        target.pResource = texture->Get();
        target.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        target.SubresourceIndex = mip;
        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = upload.Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source.PlacedFootprint = footprints[mip];
        uploadList->CopyTextureRegion(&target, 0, 0, 0, &source, nullptr);
    }
    detail::Transition(uploadList, texture->Get(),
                       D3D12_RESOURCE_STATE_COPY_DEST,
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    uploads->push_back(std::move(upload));
    return true;
}

bool D3D12Harness::Impl::CreateSolidTexture(
    ID3D12GraphicsCommandList* uploadList, uint32_t rgba,
    ComPtr<ID3D12Resource>* texture,
    std::vector<ComPtr<ID3D12Resource>>* uploads, std::string* error) {
    const D3D12_HEAP_PROPERTIES defaultHeap =
        detail::Heap(D3D12_HEAP_TYPE_DEFAULT);
    const D3D12_RESOURCE_DESC textureDesc =
        detail::TextureDesc(1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
                            D3D12_RESOURCE_FLAG_NONE);
    HRESULT hr = device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(texture->GetAddressOf()));
    if (FAILED(hr)) {
        return detail::Fail("CreateCommittedResource(solid texture)", hr, error);
    }
    ComPtr<ID3D12Resource> upload;
    const D3D12_HEAP_PROPERTIES uploadHeap =
        detail::Heap(D3D12_HEAP_TYPE_UPLOAD);
    const D3D12_RESOURCE_DESC uploadDesc =
        detail::BufferDesc(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    hr = device->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(upload.GetAddressOf()));
    if (FAILED(hr)) {
        return detail::Fail("CreateCommittedResource(solid upload)", hr, error);
    }
    void* mapped = nullptr;
    hr = upload->Map(0, nullptr, &mapped);
    if (FAILED(hr)) {
        return detail::Fail("ID3D12Resource::Map(solid upload)", hr, error);
    }
    *static_cast<uint32_t*>(mapped) = rgba;
    upload->Unmap(0, nullptr);

    D3D12_TEXTURE_COPY_LOCATION target{};
    target.pResource = texture->Get();
    target.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    target.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = upload.Get();
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint.Offset = 0;
    source.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    source.PlacedFootprint.Footprint.Width = 1;
    source.PlacedFootprint.Footprint.Height = 1;
    source.PlacedFootprint.Footprint.Depth = 1;
    source.PlacedFootprint.Footprint.RowPitch =
        D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
    uploadList->CopyTextureRegion(&target, 0, 0, 0, &source, nullptr);
    detail::Transition(uploadList, texture->Get(),
                       D3D12_RESOURCE_STATE_COPY_DEST,
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    uploads->push_back(std::move(upload));
    return true;
}

bool D3D12Harness::Impl::LoadMaterial(const ContentCatalog& catalog,
                                     const MaterialCell& cell,
                                     uint32_t materialSlot,
                                     std::string* error) {
    MaterialPackage package;
    std::string packageError;
    if (!catalog.LoadPackage(options.packageDir, cell.material, &package,
                             &packageError)) {
        std::printf("MATERIAL %s: NOT RUN (%s)\n", cell.material.c_str(),
                    packageError.c_str());
        return true;
    }

    detail::MaterialResources resources;
    resources.cell = cell.cell;
    resources.material = cell.material;
    resources.descriptorBase =
        materialSlot * detail::kMaterialChannelCount;
    resources.metalnessDefault = 0.0f;
    resources.aoDefault = 1.0f;
    resources.normalOpenGL = cell.normal.handedness == "opengl";
    resources.packageSha = package.packageSha256;

    ComPtr<ID3D12CommandAllocator> uploadAllocator;
    ComPtr<ID3D12GraphicsCommandList> uploadList;
    HRESULT hr = device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAllocator));
    if (FAILED(hr)) {
        return detail::Fail("CreateCommandAllocator(upload)", hr, error);
    }
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                   uploadAllocator.Get(), nullptr,
                                   IID_PPV_ARGS(&uploadList));
    if (FAILED(hr)) {
        return detail::Fail("CreateCommandList(upload)", hr, error);
    }

    std::vector<ComPtr<ID3D12Resource>> uploads;
    ComPtr<ID3D12Resource> textures[detail::kMaterialChannelCount];
    DXGI_FORMAT formats[detail::kMaterialChannelCount] = {
        DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM};
    uint32_t mipCounts[detail::kMaterialChannelCount] = {1, 1, 1, 1, 1, 1};
    uint64_t textureBytes[detail::kMaterialChannelCount] = {0, 0, 0, 0, 0, 0};
    bool present[detail::kMaterialChannelCount] = {false, false, false,
                                                   false, false, false};
    const char* channelNames[detail::kMaterialChannelCount] = {
        "baseColor", "normal", "roughness", "metalness", "ao", "emissive"};

    for (uint32_t channel = 0; channel < detail::kMaterialChannelCount;
         ++channel) {
        const PackageTexture* texture = package.Find(channelNames[channel]);
        const ChannelInfo* info = nullptr;
        switch (channel) {
            case detail::kBaseColor: info = &cell.baseColor; break;
            case detail::kNormal: info = &cell.normal; break;
            case detail::kRoughness: info = &cell.roughness; break;
            case detail::kMetalness: info = &cell.metalness; break;
            case detail::kAo: info = &cell.ao; break;
            case detail::kEmissive: info = &cell.emissive; break;
        }
        if (texture != nullptr && (info == nullptr || info->available)) {
            DdsImage dds;
            const fs::path ddsPath = fs::path(package.dir) / texture->file;
            if (!LoadDds(ddsPath.string(), &dds, error)) {
                uploadList->Close();
                return false;
            }
            if (!CreateTexture(uploadList.Get(), dds, dds.format,
                               &textures[channel], &uploads, error)) {
                uploadList->Close();
                return false;
            }
            formats[channel] = dds.format;
            mipCounts[channel] = dds.mipCount;
            textureBytes[channel] = DdsBytes(dds);
            present[channel] = true;
        }
    }

    const uint32_t solidWhite = 0xFFFFFFFFu;
    const uint32_t solidBlack = 0x00000000u;
    const uint32_t solidNormal = 0xFF8080FFu;
    const uint32_t solidMid = 0xFF808080u;
    const uint32_t fallbackValues[detail::kMaterialChannelCount] = {
        solidWhite, solidNormal, solidMid, solidBlack, solidWhite, solidBlack};
    for (uint32_t channel = 0; channel < detail::kMaterialChannelCount;
         ++channel) {
        if (!present[channel]) {
            if (!CreateSolidTexture(uploadList.Get(), fallbackValues[channel],
                                    &textures[channel], &uploads, error)) {
                uploadList->Close();
                return false;
            }
            formats[channel] = DXGI_FORMAT_R8G8B8A8_UNORM;
            mipCounts[channel] = 1;
        }
    }

    hr = uploadList->Close();
    if (FAILED(hr)) {
        return detail::Fail("ID3D12GraphicsCommandList::Close(upload)", hr,
                            error);
    }
    ID3D12CommandList* lists[] = {uploadList.Get()};
    queue->ExecuteCommandLists(1, lists);
    if (!SignalAndWait(error)) {
        return false;
    }

    for (uint32_t channel = 0; channel < detail::kMaterialChannelCount;
         ++channel) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Format = formats[channel];
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Texture2D.MipLevels = mipCounts[channel];
        device->CreateShaderResourceView(
            textures[channel].Get(), &srv,
            CpuHandle(resources.descriptorBase + channel));
        resources.textures[channel] = textures[channel];
        resources.has[channel] = present[channel];
    }
    materials.push_back(std::move(resources));
    materialIndex[cell.cell] = materials.size() - 1;

    // Residency hook: account the authored texture bytes with the same policy
    // T054 tests. The GPU resource is one committed texture per channel; this
    // is bookkeeping, not a claim that the policy owns the allocation.
    rhi::ResidencyPolicy policy;
    std::string policyError;
    uint64_t totalBytes = 0;
    for (uint32_t channel = 0; channel < detail::kMaterialChannelCount;
         ++channel) {
        totalBytes += textureBytes[channel];
    }
    if (policy.Configure(256ull * 1024 * 1024, 64ull * 1024 * 1024,
                         &policyError) == rhi::Status::Ok) {
        uint32_t id = 1;
        for (uint32_t channel = 0; channel < detail::kMaterialChannelCount;
             ++channel) {
            if (present[channel]) {
                policy.RequestUpload(id++, textureBytes[channel]);
            }
        }
        const rhi::ResidencyTick tick = policy.Tick(0);
        char buffer[256] = {};
        std::snprintf(
            buffer, sizeof(buffer),
            "budget=%llu uploadPerFrame=%llu resident=%llu uploaded=%llu "
            "deferred=%u",
            static_cast<unsigned long long>(policy.budgetBytes()),
            static_cast<unsigned long long>(policy.uploadBytesPerFrame()),
            static_cast<unsigned long long>(policy.residentBytes()),
            static_cast<unsigned long long>(tick.uploadedBytes),
            tick.uploadsDeferred);
        hooks.residency = buffer;
        std::printf("MATERIAL %s: RUN (%s, %llu texture bytes)\n",
                    cell.material.c_str(), package.packageSha256.c_str(),
                    static_cast<unsigned long long>(totalBytes));
    }
    return true;
}

void D3D12Harness::Impl::BuildHooks(const ContentCatalog& catalog,
                                    const MaterialCell& cell) {
    (void)catalog;
    rhi::ModernMaterialInput input{};
    input.name = cell.material.c_str();
    input.baseColor = ToChannelState(cell.baseColor);
    input.normal = ToChannelState(cell.normal);
    input.roughness = ToChannelState(cell.roughness);
    input.metalness = ToChannelState(cell.metalness);
    input.ao = ToChannelState(cell.ao);
    input.emissive = ToChannelState(cell.emissive);
    input.baseColorSrgb = cell.baseColor.srgb;
    input.emissiveHdr = false;
    input.normalHandednessDeclared = !cell.normal.handedness.empty();
    input.legacySpecularMap = true;
    input.declaredDielectric = cell.declaredDielectric;
    input.coverage = rhi::CoverageClass::Opaque;

    rhi::ModernMaterialPlan plan;
    std::string planError;
    if (rhi::PlanModernMaterial(input, &plan, &planError) == rhi::Status::Ok) {
        std::ostringstream stream;
        stream << cell.material << ": " << rhi::ToString(plan.route);
        if (plan.route == rhi::MaterialRoute::HybridOpaque) {
            stream << " linear_hdr=" << (plan.linearHdrOpaque ? 1 : 0)
                   << " srgb_decoded=" << (plan.srgbBaseColor ? 1 : 0);
        } else if (plan.forwardReason != nullptr) {
            stream << " (" << plan.forwardReason << ")";
        }
        hooks.materialPlan = stream.str();
    } else {
        hooks.materialPlan = cell.material + ": plan failed: " + planError;
    }

    rhi::HybridOpaqueDesc hybrid{};
    hybrid.material = input;
    hybrid.baseColor = {0.42f, 0.38f, 0.34f};
    hybrid.emissive = {0.0f, 0.0f, 0.0f};
    hybrid.roughness = 0.55f;
    hybrid.metalness = 0.0f;
    hybrid.ao = 1.0f;
    rhi::HybridOpaqueInput hybridInput;
    std::string hybridError;
    if (rhi::BuildHybridOpaqueInput(hybrid, &hybridInput, &hybridError) ==
            rhi::Status::Ok &&
        hybridInput.linearHdrOpaque) {
        hooks.materialPlan += " (opaque inputs accepted)";
    }

    rhi::DirectLightInput light{};
    light.radiance = {1.0f, 0.93f, 0.82f};
    light.direction = {0.45f, -1.0f, -0.30f};
    light.range = 0.0f;
    light.castsShadow = true;
    rhi::DirectSurfaceInput surface{};
    surface.position = {0.0f, 0.9f, 0.0f};
    surface.normal = {0.0f, 1.0f, 0.0f};
    surface.view = {0.0f, 0.0f, 1.0f};
    surface.baseColor = {0.42f, 0.38f, 0.34f};
    surface.roughness = 0.55f;
    surface.metalness = 0.0f;
    rhi::ShadowInput shadow{};
    shadow.lightDepth = 0.40f;
    shadow.surfaceDepth = 0.50f;
    shadow.taps = 9;
    shadow.blockers = 0;
    rhi::DirectLightResult direct;
    std::string directError;
    if (rhi::EvaluateDirectLight(light, surface, shadow, &direct,
                                 &directError) == rhi::Status::Ok) {
        char buffer[256] = {};
        std::snprintf(
            buffer, sizeof(buffer),
            "attenuation=%.3f ndotl=%.3f shadow=%.3f radiance=(%.3f, %.3f, %.3f)",
            direct.attenuation, direct.ndotl, direct.shadowFactor,
            direct.radiance.r, direct.radiance.g, direct.radiance.b);
        hooks.directLight = buffer;
    }

    rhi::Sh4 probe{};
    probe.c[0][0] = 0.38f;
    probe.c[0][1] = 0.43f;
    probe.c[0][2] = 0.52f;
    probe.c[1][0] = 0.07f;
    probe.c[1][1] = 0.08f;
    probe.c[1][2] = 0.11f;
    probe.c[2][0] = 0.03f;
    probe.c[2][1] = 0.04f;
    probe.c[2][2] = 0.05f;
    probe.c[3][0] = 0.04f;
    probe.c[3][1] = 0.035f;
    probe.c[3][2] = 0.03f;
    rhi::LinearRgb irradiance;
    bool clamped = false;
    rhi::EvaluateIrradiance(probe, {0.0f, 1.0f, 0.0f}, &irradiance, &clamped);
    rhi::ReflectionInput reflectionInput{};
    reflectionInput.incident = {0.4f, 0.8f, 0.45f};
    reflectionInput.normal = {0.0f, 1.0f, 0.0f};
    reflectionInput.roughness = 0.55f;
    reflectionInput.environment.zenith = {0.25f, 0.35f, 0.55f};
    reflectionInput.environment.horizon = {0.55f, 0.60f, 0.70f};
    reflectionInput.environment.ground = {0.12f, 0.10f, 0.08f};
    rhi::ReflectionResult reflection;
    std::string reflectionError;
    rhi::EvaluateReflection(reflectionInput, &reflection, &reflectionError);
    rhi::AtmosphereInput atmosphereInput{};
    atmosphereInput.distance = 7.0f;
    atmosphereInput.density = 0.012f;
    atmosphereInput.heightFalloff = 0.15f;
    atmosphereInput.cameraHeight = 2.8f;
    atmosphereInput.fogHeight = 0.0f;
    atmosphereInput.fogColor = {0.35f, 0.45f, 0.62f};
    rhi::AtmosphereResult atmosphere;
    std::string atmosphereError;
    rhi::EvaluateAtmosphere(atmosphereInput, &atmosphere, &atmosphereError);
    char buffer[320] = {};
    std::snprintf(
        buffer, sizeof(buffer),
        "irradiance=(%.3f, %.3f, %.3f) reflection=(%.3f, %.3f, %.3f) "
        "horizonFallback=%d transmittance=%.3f",
        irradiance.r, irradiance.g, irradiance.b, reflection.reflection.r,
        reflection.reflection.g, reflection.reflection.b,
        reflection.horizonFallback ? 1 : 0, atmosphere.transmittance);
    hooks.indirect = buffer;
    hooks.shadow = "2048x2048 directional, PCF 3x3 comparison sampler";
}

bool D3D12Harness::Impl::HasMaterialImpl(const std::string& cell) const {
    auto found = materialIndex.find(cell);
    if (found == materialIndex.end()) {
        return false;
    }
    const detail::MaterialResources& material = materials[found->second];
    return material.has[detail::kBaseColor] &&
           material.has[detail::kRoughness];
}

D3D12Harness::D3D12Harness() = default;
D3D12Harness::~D3D12Harness() = default;

bool D3D12Harness::Initialize(ID3D12Device* device,
                              const HarnessOptions& options,
                              std::string* error) {
    impl_ = std::make_unique<Impl>();
    impl_->options = options;
    impl_->device = device;
    if (!impl_->device) {
        return detail::FailMessage(error, "the RHI did not provide a D3D12 device");
    }

    impl_->displayWidth = std::max<uint32_t>(64, options.width);
    impl_->displayHeight = std::max<uint32_t>(64, options.height);
    const float scale = std::clamp(options.renderScale, 0.25f, 1.0f);
    impl_->renderWidth = std::max<uint32_t>(
        64, static_cast<uint32_t>(std::lround(impl_->displayWidth * scale)));
    impl_->renderHeight = std::max<uint32_t>(
        64, static_cast<uint32_t>(std::lround(impl_->displayHeight * scale)));

    rhi::ReconstructionDesc reconstruction{};
    reconstruction.renderWidth = impl_->renderWidth;
    reconstruction.renderHeight = impl_->renderHeight;
    reconstruction.displayWidth = impl_->displayWidth;
    reconstruction.displayHeight = impl_->displayHeight;
    rhi::ReconstructionOptions reconstructionOptions{};
    rhi::ReconstructionPlan reconstructionPlan;
    std::string reconstructionError;
    if (rhi::PlanReconstruction(reconstruction, reconstructionOptions,
                                &reconstructionPlan,
                                &reconstructionError) != rhi::Status::Ok) {
        return detail::FailMessage(error, reconstructionError);
    }
    impl_->reconstructionMode =
        rhi::ToString(reconstructionPlan.mode);
    char reconstructionBuffer[192] = {};
    std::snprintf(reconstructionBuffer, sizeof(reconstructionBuffer),
                  "%s (%ux%u -> %ux%u)", impl_->reconstructionMode.c_str(),
                  impl_->renderWidth, impl_->renderHeight, impl_->displayWidth,
                  impl_->displayHeight);
    impl_->hooks.reconstruction = reconstructionBuffer;

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;
    HRESULT hr = impl_->device->CreateCommandQueue(
        &queueDesc, IID_PPV_ARGS(&impl_->queue));
    if (FAILED(hr)) {
        return detail::Fail("CreateCommandQueue", hr, error);
    }
    hr = impl_->device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                    IID_PPV_ARGS(&impl_->fence));
    if (FAILED(hr)) {
        return detail::Fail("CreateFence", hr, error);
    }
    impl_->fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (impl_->fenceEvent == nullptr) {
        return detail::FailMessage(error, "CreateEventW failed");
    }
    hr = impl_->device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&impl_->allocator));
    if (FAILED(hr)) {
        return detail::Fail("CreateCommandAllocator", hr, error);
    }
    hr = impl_->device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, impl_->allocator.Get(), nullptr,
        IID_PPV_ARGS(&impl_->list));
    if (FAILED(hr)) {
        return detail::Fail("CreateCommandList", hr, error);
    }
    hr = impl_->list->Close();
    if (FAILED(hr)) {
        return detail::Fail("ID3D12GraphicsCommandList::Close", hr, error);
    }

    if (!impl_->CreateDescriptors(error) ||
        !impl_->CreateRootSignatures(error) ||
        !impl_->CreatePipelines(error) || !impl_->CreateMeshes(error) ||
        !impl_->CreateTargets(error)) {
        return false;
    }
    return true;
}

bool D3D12Harness::LoadMaterials(const ContentCatalog& catalog,
                                 std::string* error) {
    if (!impl_) {
        return detail::FailMessage(error, "harness is not initialized");
    }
    uint32_t slot = 0;
    for (const MaterialCell& cell : catalog.cells()) {
        if (!cell.present) {
            continue;
        }
        if (slot >= detail::kMaxMaterials) {
            return detail::FailMessage(error,
                                       "too many materials for the descriptor heap");
        }
        if (!impl_->LoadMaterial(catalog, cell, slot, error)) {
            return false;
        }
        ++slot;
    }
    const MaterialCell* hookCell = catalog.FindMaterial(impl_->options.material);
    if (hookCell == nullptr && !catalog.cells().empty()) {
        hookCell = &catalog.cells().front();
    }
    if (hookCell != nullptr) {
        impl_->BuildHooks(catalog, *hookCell);
    }
    return true;
}

bool D3D12Harness::RenderScene(const ContentCatalog& catalog,
                               const std::string& scene, uint32_t frameIndex,
                               std::string* error) {
    if (!impl_) {
        return detail::FailMessage(error, "harness is not initialized");
    }
    const uint32_t frames = std::max<uint32_t>(1, impl_->options.frames);
    for (uint32_t frame = 0; frame < frames; ++frame) {
        (void)frameIndex;
        std::vector<detail::DrawItem> draws;
        std::string reason;
        if (!impl_->BuildDraws(catalog, scene, frame, &draws, &reason)) {
            std::printf("SCENE %s: NOT RUN (%s)\n", scene.c_str(),
                        reason.c_str());
            return detail::FailMessage(error, reason);
        }
        if (!impl_->RenderFrame(draws, frame, error)) {
            return false;
        }
    }
    return true;
}

bool D3D12Harness::WriteCapture(const std::string& path, std::string* error) {
    if (!impl_) {
        return detail::FailMessage(error, "harness is not initialized");
    }
    if (!impl_->display) {
        return detail::FailMessage(error, "no display target exists");
    }
    HRESULT hr = impl_->allocator->Reset();
    if (FAILED(hr)) {
        return detail::Fail("ID3D12CommandAllocator::Reset(capture)", hr, error);
    }
    hr = impl_->list->Reset(impl_->allocator.Get(), nullptr);
    if (FAILED(hr)) {
        return detail::Fail("ID3D12GraphicsCommandList::Reset(capture)", hr, error);
    }
    detail::Transition(impl_->list.Get(), impl_->display.Get(),
                       impl_->displayState, D3D12_RESOURCE_STATE_COPY_SOURCE);
    impl_->displayState = D3D12_RESOURCE_STATE_COPY_SOURCE;
    D3D12_TEXTURE_COPY_LOCATION target{};
    target.pResource = impl_->readback.Get();
    target.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    target.PlacedFootprint = impl_->readbackFootprint;
    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = impl_->display.Get();
    source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    source.SubresourceIndex = 0;
    impl_->list->CopyTextureRegion(&target, 0, 0, 0, &source, nullptr);
    if (!impl_->ExecuteAndWait(error)) {
        return false;
    }

    void* mapped = nullptr;
    hr = impl_->readback->Map(0, nullptr, &mapped);
    if (FAILED(hr)) {
        const HRESULT reason = impl_->device->GetDeviceRemovedReason();
        char buffer[192] = {};
        std::snprintf(buffer, sizeof(buffer),
                      "ID3D12Resource::Map(readback) failed (hr=0x%08lX, "
                      "device reason=0x%08lX)",
                      static_cast<unsigned long>(static_cast<uint32_t>(hr)),
                      static_cast<unsigned long>(static_cast<uint32_t>(reason)));
        return detail::FailMessage(error, buffer);
    }
    const fs::path outputPath = path;
    if (outputPath.has_parent_path()) {
        std::error_code createError;
        fs::create_directories(outputPath.parent_path(), createError);
        if (createError) {
            impl_->readback->Unmap(0, nullptr);
            return detail::FailMessage(
                error, "cannot create " + outputPath.parent_path().string() +
                           ": " + createError.message());
        }
    }
    const auto* bytes = static_cast<const uint8_t*>(mapped);
    const bool written = WritePngRgba8(
        path, impl_->displayWidth, impl_->displayHeight, bytes,
        impl_->readbackFootprint.Footprint.RowPitch, error);
    impl_->readback->Unmap(0, nullptr);
    if (!written) {
        return false;
    }
    std::printf("CAPTURE WROTE: %s (%ux%u)\n", path.c_str(),
                impl_->displayWidth, impl_->displayHeight);
    return true;
}

void D3D12Harness::Shutdown() {
    if (!impl_) {
        return;
    }
    if (impl_->queue && impl_->fence && impl_->fenceValue > 0) {
        std::string ignored;
        impl_->SignalAndWait(&ignored);
    }
    impl_.reset();
}

const RuntimeHooks& D3D12Harness::hooks() const {
    static const RuntimeHooks empty;
    return impl_ ? impl_->hooks : empty;
}

uint32_t D3D12Harness::renderWidth() const {
    return impl_ ? impl_->renderWidth : 0;
}

uint32_t D3D12Harness::renderHeight() const {
    return impl_ ? impl_->renderHeight : 0;
}

uint32_t D3D12Harness::displayWidth() const {
    return impl_ ? impl_->displayWidth : 0;
}

uint32_t D3D12Harness::displayHeight() const {
    return impl_ ? impl_->displayHeight : 0;
}

const std::string& D3D12Harness::reconstructionMode() const {
    static const std::string empty;
    return impl_ ? impl_->reconstructionMode : empty;
}

bool D3D12Harness::HasMaterial(const std::string& cell) const {
    return impl_ ? impl_->HasMaterialImpl(cell) : false;
}

}  // namespace q4
