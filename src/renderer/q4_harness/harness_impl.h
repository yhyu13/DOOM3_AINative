#pragma once

// Private implementation vocabulary shared by the harness translation units.
// Not part of the public list; only the q4_harness executable includes it.

#include "dds.h"
#include "harness.h"
#include "mesh.h"

#include <windows.h>

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <DirectXMath.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace q4 {
namespace detail {

using Microsoft::WRL::ComPtr;
using namespace DirectX;

constexpr uint32_t kShadowSize = 2048;
constexpr uint32_t kMaxDraws = 64;
constexpr uint32_t kMaxBones = 12;
constexpr uint32_t kCbSlotBytes = 2048;
constexpr uint32_t kMaxMaterials = 16;
constexpr uint32_t kMaterialChannelCount = 6;
constexpr uint32_t kShadowDescriptorIndex =
    kMaxMaterials * kMaterialChannelCount;
constexpr uint32_t kSceneDescriptorIndex = kShadowDescriptorIndex + 1;
constexpr uint32_t kSceneCopyDescriptorIndex = kSceneDescriptorIndex + 1;
constexpr uint32_t kDescriptorCount = kSceneCopyDescriptorIndex + 1;

constexpr DXGI_FORMAT kHdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
constexpr DXGI_FORMAT kDepthFormat = DXGI_FORMAT_D32_FLOAT;
constexpr DXGI_FORMAT kDisplayFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

enum ChannelIndex : uint32_t {
    kBaseColor = 0,
    kNormal = 1,
    kRoughness = 2,
    kMetalness = 3,
    kAo = 4,
    kEmissive = 5,
};

struct alignas(16) FrameConstants {
    XMFLOAT4X4 viewProj;
    XMFLOAT4X4 lightViewProj;
    XMFLOAT4X4 world;
    XMFLOAT4X4 normalMatrix;
    XMFLOAT4 cameraPos;   // xyz, w = exposure
    XMFLOAT4 lightDir;    // xyz = travel direction, w = intensity
    XMFLOAT4 lightColor;  // rgb, w = 1 / shadow size
    XMFLOAT4 material;    // x = roughness scale, y = metalness, z = ao, w = flags
    XMFLOAT4 sh0;
    XMFLOAT4 sh1;
    XMFLOAT4 sh2;
    XMFLOAT4 envZenith;
    XMFLOAT4 envHorizon;
    XMFLOAT4 envGround;
    XMFLOAT4 fog;       // density, heightFalloff, cameraHeight, fogHeight
    XMFLOAT4 fogColor;  // rgb, w = enabled
    XMFLOAT4 params;    // x = alpha cutoff, y = alpha test, z = transmission
    XMFLOAT4 viewport;  // xy = render extent, zw = display extent
    XMFLOAT4 emissiveParams;    // x = scale, y = apply scale, zw unused
    XMFLOAT4 transmissionParams;  // x = ior, y = thickness, z = frost, w = enabled
    XMFLOAT4X4 bones[kMaxBones];  // linear-blend skinning palette (row vectors)
};

struct Mesh {
    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexView{};
    D3D12_INDEX_BUFFER_VIEW indexView{};
    uint32_t indexCount = 0;
};

struct MaterialResources {
    std::string cell;
    std::string material;
    ComPtr<ID3D12Resource> textures[kMaterialChannelCount];
    bool has[kMaterialChannelCount] = {false, false, false, false, false, false};
    uint32_t descriptorBase = 0;
    float metalnessDefault = 0.0f;
    float aoDefault = 1.0f;
    float emissiveScale = 1.0f;
    bool emissiveHdr = false;
    bool normalOpenGL = false;
    std::string packageSha;
};

struct DrawItem {
    MeshKind mesh = MeshKind::Cube;
    XMMATRIX world = XMMatrixIdentity();
    std::string cell;
    bool alphaTest = false;
    bool emissive = false;
    bool transmission = false;
    bool worldGui = false;
    bool skinned = false;
};

bool Fail(const char* operation, HRESULT hr, std::string* error);
bool FailMessage(std::string* error, const std::string& message);
void Transition(ID3D12GraphicsCommandList* list, ID3D12Resource* resource,
                D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
D3D12_HEAP_PROPERTIES Heap(D3D12_HEAP_TYPE type);
D3D12_RESOURCE_DESC BufferDesc(uint64_t bytes);
D3D12_RESOURCE_DESC TextureDesc(uint32_t width, uint32_t height, uint32_t mips,
                                DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags);

}  // namespace detail

using detail::ComPtr;

struct D3D12Harness::Impl {
    HarnessOptions options;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;
    HANDLE fenceEvent = nullptr;
    UINT64 fenceValue = 0;

    ComPtr<ID3D12DescriptorHeap> srvHeap;
    UINT32 srvSize = 0;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    UINT32 rtvSize = 0;
    ComPtr<ID3D12DescriptorHeap> dsvHeap;
    UINT32 dsvSize = 0;

    ComPtr<ID3D12RootSignature> mainRoot;
    ComPtr<ID3D12RootSignature> shadowRoot;
    ComPtr<ID3D12RootSignature> tonemapRoot;
    ComPtr<ID3D12RootSignature> hudRoot;
    ComPtr<ID3D12PipelineState> mainPso;
    ComPtr<ID3D12PipelineState> worldGuiPso;
    ComPtr<ID3D12PipelineState> glassPso;
    ComPtr<ID3D12PipelineState> shadowPso;
    ComPtr<ID3D12PipelineState> tonemapPso;
    ComPtr<ID3D12PipelineState> hudPso;

    ComPtr<ID3D12Resource> sceneColor;
    ComPtr<ID3D12Resource> sceneCopy;
    ComPtr<ID3D12Resource> depth;
    ComPtr<ID3D12Resource> shadowMap;
    ComPtr<ID3D12Resource> display;
    ComPtr<ID3D12Resource> hdrReadback;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT hdrFootprint{};
    UINT hdrRowCount = 0;
    UINT64 hdrTotalBytes = 0;
    ComPtr<ID3D12Resource> readback;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT readbackFootprint{};
    UINT readbackRowCount = 0;

    ComPtr<ID3D12Resource> constantBuffer;
    uint8_t* constantMapped = nullptr;

    D3D12_CPU_DESCRIPTOR_HANDLE sceneRtv{};
    D3D12_CPU_DESCRIPTOR_HANDLE displayRtv{};
    D3D12_CPU_DESCRIPTOR_HANDLE mainDsv{};
    D3D12_CPU_DESCRIPTOR_HANDLE shadowDsv{};
    D3D12_GPU_DESCRIPTOR_HANDLE shadowSrv{};
    D3D12_GPU_DESCRIPTOR_HANDLE sceneSrv{};
    D3D12_GPU_DESCRIPTOR_HANDLE sceneCopySrv{};

    D3D12_RESOURCE_STATES sceneState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    D3D12_RESOURCE_STATES sceneCopyState = D3D12_RESOURCE_STATE_COPY_DEST;
    D3D12_RESOURCE_STATES shadowState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    D3D12_RESOURCE_STATES displayState = D3D12_RESOURCE_STATE_RENDER_TARGET;

    detail::Mesh meshes[5];
    std::vector<detail::MaterialResources> materials;
    std::map<std::string, size_t> materialIndex;
    RuntimeHooks hooks;
    std::string reconstructionMode;
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
    uint32_t displayWidth = 0;
    uint32_t displayHeight = 0;
    float lastFrameMs = 0.0f;

    ~Impl();

    bool SignalAndWait(std::string* error);
    bool ExecuteAndWait(std::string* error);
    bool CreateRootSignatures(std::string* error);
    bool CreatePipelines(std::string* error);
    bool CreateMeshes(std::string* error);
    bool CreateTargets(std::string* error);
    bool CreateDescriptors(std::string* error);
    bool CreateTexture(ID3D12GraphicsCommandList* uploadList, const DdsImage& dds,
                       DXGI_FORMAT srvFormat, ComPtr<ID3D12Resource>* texture,
                       std::vector<ComPtr<ID3D12Resource>>* uploads,
                       std::string* error);
    bool CreateSolidTexture(ID3D12GraphicsCommandList* uploadList, uint32_t rgba,
                            ComPtr<ID3D12Resource>* texture,
                            std::vector<ComPtr<ID3D12Resource>>* uploads,
                            std::string* error);
    bool LoadMaterial(const ContentCatalog& catalog, const MaterialCell& cell,
                      uint32_t materialSlot, std::string* error);
    bool BuildDraws(const ContentCatalog& catalog, const std::string& scene,
                    uint32_t frameIndex, std::vector<detail::DrawItem>* draws,
                    std::string* reason) const;
    bool RenderFrame(const std::vector<detail::DrawItem>& draws,
                     uint32_t frameIndex, std::string* error);
    void BuildConstants(const detail::DrawItem& draw, uint32_t frameIndex,
                        detail::FrameConstants* out) const;
    void BuildHooks(const ContentCatalog& catalog, const MaterialCell& cell);
    bool HasMaterialImpl(const std::string& cell) const;
    D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle(uint32_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle(uint32_t index) const;
};

}  // namespace q4
