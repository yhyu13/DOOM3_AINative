#include "harness_impl.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace q4 {
namespace {

using namespace detail;

namespace fs = std::filesystem;

std::string ReadFileText(const fs::path& path, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error != nullptr) {
            *error = "cannot open " + path.string();
        }
        return {};
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

bool CompileShader(const fs::path& path, const char* entry, const char* target,
                   ComPtr<ID3DBlob>* shader, std::string* error) {
    std::string source = ReadFileText(path, error);
    if (source.empty()) {
        return false;
    }
    ComPtr<ID3DBlob> errors;
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
    const HRESULT hr = D3DCompile(source.data(), source.size(),
                                  path.string().c_str(), nullptr, nullptr, entry,
                                  target, flags, 0, shader->GetAddressOf(),
                                  errors.GetAddressOf());
    if (FAILED(hr)) {
        std::string detail =
            "shader compile failed (" + path.string() + ":" + entry + ")";
        if (errors) {
            detail += ": ";
            detail += static_cast<const char*>(errors->GetBufferPointer());
        }
        return FailMessage(error, detail);
    }
    return true;
}

}  // namespace

bool D3D12Harness::Impl::CreateRootSignatures(std::string* error) {
    D3D12_DESCRIPTOR_RANGE materialRange{};
    materialRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    materialRange.NumDescriptors = detail::kMaterialChannelCount;
    materialRange.BaseShaderRegister = 0;
    materialRange.RegisterSpace = 0;
    materialRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE shadowRange{};
    shadowRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    shadowRange.NumDescriptors = 1;
    shadowRange.BaseShaderRegister = detail::kMaterialChannelCount;
    shadowRange.RegisterSpace = 0;
    shadowRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER mainParams[3]{};
    mainParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    mainParams[0].Descriptor.ShaderRegister = 0;
    mainParams[0].Descriptor.RegisterSpace = 0;
    mainParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    mainParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    mainParams[1].DescriptorTable.NumDescriptorRanges = 1;
    mainParams[1].DescriptorTable.pDescriptorRanges = &materialRange;
    mainParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    mainParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    mainParams[2].DescriptorTable.NumDescriptorRanges = 1;
    mainParams[2].DescriptorTable.pDescriptorRanges = &shadowRange;
    mainParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC samplers[2]{};
    samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[0].MipLODBias = 0.0f;
    // MaxAnisotropy = 1 (trilinear). Tried 8x during the B2 investigation and
    // reverted: it raises high-frequency energy 3-4x in the environment cell's
    // far field because that content's mip chain does not filter its grid, so
    // it turns over-blur into shimmer. B2 was an authoring defect, not this
    // setting; see specs/004-quality-renderer/evidence-q4.md 5.7.
    samplers[0].MaxAnisotropy = 1;
    samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    samplers[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    samplers[0].MinLOD = 0.0f;
    samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    samplers[0].ShaderRegister = 0;
    samplers[0].RegisterSpace = 0;
    samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    samplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].MipLODBias = 0.0f;
    samplers[1].MaxAnisotropy = 1;
    samplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    samplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplers[1].MinLOD = 0.0f;
    samplers[1].MaxLOD = D3D12_FLOAT32_MAX;
    samplers[1].ShaderRegister = 1;
    samplers[1].RegisterSpace = 0;
    samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC mainDesc{};
    mainDesc.NumParameters = 3;
    mainDesc.pParameters = mainParams;
    mainDesc.NumStaticSamplers = 2;
    mainDesc.pStaticSamplers = samplers;
    mainDesc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> messages;
    HRESULT hr = D3D12SerializeRootSignature(
        &mainDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, &messages);
    if (FAILED(hr)) {
        std::string detail = "D3D12SerializeRootSignature(main) failed";
        if (messages) {
            detail += ": ";
            detail += static_cast<const char*>(messages->GetBufferPointer());
        }
        return detail::FailMessage(error, detail);
    }
    hr = device->CreateRootSignature(
        0, blob->GetBufferPointer(), blob->GetBufferSize(),
        IID_PPV_ARGS(&mainRoot));
    if (FAILED(hr)) {
        return detail::Fail("ID3D12Device::CreateRootSignature(main)", hr,
                            error);
    }

    D3D12_ROOT_PARAMETER shadowParam{};
    shadowParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    shadowParam.Descriptor.ShaderRegister = 0;
    shadowParam.Descriptor.RegisterSpace = 0;
    shadowParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // The shadow pixel shader samples baseColor.alpha for the cutout test, so
    // the shadow root signature carries the same material table and the same
    // linear sampler the visible pass uses.
    D3D12_ROOT_PARAMETER shadowParams[2]{};
    shadowParams[0] = shadowParam;
    shadowParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    shadowParams[1].DescriptorTable.NumDescriptorRanges = 1;
    shadowParams[1].DescriptorTable.pDescriptorRanges = &materialRange;
    shadowParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC shadowSampler = samplers[0];

    D3D12_ROOT_SIGNATURE_DESC shadowDesc{};
    shadowDesc.NumParameters = 2;
    shadowDesc.pParameters = shadowParams;
    shadowDesc.NumStaticSamplers = 1;
    shadowDesc.pStaticSamplers = &shadowSampler;
    shadowDesc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    blob.Reset();
    messages.Reset();
    hr = D3D12SerializeRootSignature(
        &shadowDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, &messages);
    if (FAILED(hr)) {
        std::string detail = "D3D12SerializeRootSignature(shadow) failed";
        if (messages) {
            detail += ": ";
            detail += static_cast<const char*>(messages->GetBufferPointer());
        }
        return detail::FailMessage(error, detail);
    }
    hr = device->CreateRootSignature(
        0, blob->GetBufferPointer(), blob->GetBufferSize(),
        IID_PPV_ARGS(&shadowRoot));
    if (FAILED(hr)) {
        return detail::Fail("ID3D12Device::CreateRootSignature(shadow)", hr,
                            error);
    }

    D3D12_ROOT_PARAMETER tonemapParams[2]{};
    tonemapParams[0].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    tonemapParams[0].Constants.ShaderRegister = 0;
    tonemapParams[0].Constants.RegisterSpace = 0;
    tonemapParams[0].Constants.Num32BitValues = 4;
    tonemapParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_DESCRIPTOR_RANGE tonemapRange{};
    tonemapRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    tonemapRange.NumDescriptors = 1;
    tonemapRange.BaseShaderRegister = 0;
    tonemapRange.RegisterSpace = 0;
    tonemapRange.OffsetInDescriptorsFromTableStart = 0;
    tonemapParams[1].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    tonemapParams[1].DescriptorTable.NumDescriptorRanges = 1;
    tonemapParams[1].DescriptorTable.pDescriptorRanges = &tonemapRange;
    tonemapParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC tonemapSampler{};
    tonemapSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    tonemapSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    tonemapSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    tonemapSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    tonemapSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    tonemapSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    tonemapSampler.MinLOD = 0.0f;
    tonemapSampler.MaxLOD = D3D12_FLOAT32_MAX;
    tonemapSampler.ShaderRegister = 0;
    tonemapSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC tonemapDesc{};
    tonemapDesc.NumParameters = 2;
    tonemapDesc.pParameters = tonemapParams;
    tonemapDesc.NumStaticSamplers = 1;
    tonemapDesc.pStaticSamplers = &tonemapSampler;
    tonemapDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    blob.Reset();
    messages.Reset();
    hr = D3D12SerializeRootSignature(
        &tonemapDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, &messages);
    if (FAILED(hr)) {
        std::string detail = "D3D12SerializeRootSignature(tonemap) failed";
        if (messages) {
            detail += ": ";
            detail += static_cast<const char*>(messages->GetBufferPointer());
        }
        return detail::FailMessage(error, detail);
    }
    hr = device->CreateRootSignature(
        0, blob->GetBufferPointer(), blob->GetBufferSize(),
        IID_PPV_ARGS(&tonemapRoot));
    if (FAILED(hr)) {
        return detail::Fail("ID3D12Device::CreateRootSignature(tonemap)", hr,
                            error);
    }
    return true;
}

bool D3D12Harness::Impl::CreatePipelines(std::string* error) {
    fs::path shaderDir = options.shaderDir;
    if (shaderDir.empty()) {
        shaderDir = fs::path(q4::ExecutableDirectory()) / "shaders";
    }
    if (!fs::exists(shaderDir / "q4_hybrid.hlsl")) {
        shaderDir = fs::path(options.repoRoot) / "src" / "renderer" /
                    "q4_harness" / "shaders";
    }
    const fs::path hybridPath = shaderDir / "q4_hybrid.hlsl";
    const fs::path tonemapPath = shaderDir / "q4_tonemap.hlsl";

    ComPtr<ID3DBlob> vs;
    ComPtr<ID3DBlob> ps;
    ComPtr<ID3DBlob> shadowVs;
    ComPtr<ID3DBlob> shadowPs;
    ComPtr<ID3DBlob> tonemapVs;
    ComPtr<ID3DBlob> tonemapPs;
    if (!CompileShader(hybridPath, "VSMain", "vs_5_1", &vs, error) ||
        !CompileShader(hybridPath, options.debugShaderEntry.c_str(), "ps_5_1",
                       &ps, error) ||
        !CompileShader(hybridPath, "VSShadow", "vs_5_1", &shadowVs, error) ||
        !CompileShader(hybridPath, "PSShadowAlpha", "ps_5_1", &shadowPs,
                       error) ||
        !CompileShader(tonemapPath, "VSMain", "vs_5_1", &tonemapVs, error) ||
        !CompileShader(tonemapPath, "PSMain", "ps_5_1", &tonemapPs, error)) {
        return false;
    }

    const D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         offsetof(Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
         0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         offsetof(Vertex, tangent),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, uv),
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_RASTERIZER_DESC rasterizer{};
    rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.CullMode = D3D12_CULL_MODE_NONE;
    rasterizer.FrontCounterClockwise = FALSE;
    rasterizer.DepthBias = 0;
    rasterizer.DepthBiasClamp = 0.0f;
    rasterizer.SlopeScaledDepthBias = 0.0f;
    rasterizer.DepthClipEnable = TRUE;
    rasterizer.MultisampleEnable = FALSE;
    rasterizer.AntialiasedLineEnable = FALSE;
    rasterizer.ForcedSampleCount = 0;
    rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_DEPTH_STENCIL_DESC depthState{};
    depthState.DepthEnable = TRUE;
    depthState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depthState.StencilEnable = FALSE;

    D3D12_BLEND_DESC blend{};
    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
    blend.RenderTarget[0].BlendEnable = FALSE;
    blend.RenderTarget[0].LogicOpEnable = FALSE;
    blend.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    blend.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
    blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC mainDesc{};
    mainDesc.pRootSignature = mainRoot.Get();
    mainDesc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    mainDesc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    mainDesc.BlendState = blend;
    mainDesc.SampleMask = UINT_MAX;
    mainDesc.RasterizerState = rasterizer;
    mainDesc.DepthStencilState = depthState;
    mainDesc.InputLayout = {layout, _countof(layout)};
    mainDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    mainDesc.NumRenderTargets = 1;
    mainDesc.RTVFormats[0] = detail::kHdrFormat;
    mainDesc.DSVFormat = detail::kDepthFormat;
    mainDesc.SampleDesc.Count = 1;
    mainDesc.SampleDesc.Quality = 0;
    HRESULT hr =
        device->CreateGraphicsPipelineState(&mainDesc, IID_PPV_ARGS(&mainPso));
    if (FAILED(hr)) {
        return detail::Fail("CreateGraphicsPipelineState(main)", hr, error);
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowDesc{};
    shadowDesc.pRootSignature = shadowRoot.Get();
    shadowDesc.VS = {shadowVs->GetBufferPointer(), shadowVs->GetBufferSize()};
    // Alpha-tested depth: the caster's cutout drives the pixel shader's clip(),
    // so a perforated surface no longer casts its solid silhouette.
    shadowDesc.PS = {shadowPs->GetBufferPointer(), shadowPs->GetBufferSize()};
    shadowDesc.BlendState = blend;
    shadowDesc.SampleMask = UINT_MAX;
    shadowDesc.RasterizerState = rasterizer;
    shadowDesc.DepthStencilState = depthState;
    shadowDesc.InputLayout = {layout, _countof(layout)};
    shadowDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    shadowDesc.NumRenderTargets = 0;
    shadowDesc.DSVFormat = detail::kDepthFormat;
    shadowDesc.SampleDesc.Count = 1;
    shadowDesc.SampleDesc.Quality = 0;
    hr = device->CreateGraphicsPipelineState(&shadowDesc,
                                             IID_PPV_ARGS(&shadowPso));
    if (FAILED(hr)) {
        return detail::Fail("CreateGraphicsPipelineState(shadow)", hr, error);
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC tonemapDesc{};
    tonemapDesc.pRootSignature = tonemapRoot.Get();
    tonemapDesc.VS = {tonemapVs->GetBufferPointer(), tonemapVs->GetBufferSize()};
    tonemapDesc.PS = {tonemapPs->GetBufferPointer(), tonemapPs->GetBufferSize()};
    tonemapDesc.BlendState = blend;
    tonemapDesc.SampleMask = UINT_MAX;
    tonemapDesc.RasterizerState = rasterizer;
    tonemapDesc.DepthStencilState.DepthEnable = FALSE;
    tonemapDesc.DepthStencilState.DepthWriteMask =
        D3D12_DEPTH_WRITE_MASK_ZERO;
    tonemapDesc.DepthStencilState.DepthFunc =
        D3D12_COMPARISON_FUNC_ALWAYS;
    tonemapDesc.DepthStencilState.StencilEnable = FALSE;
    tonemapDesc.InputLayout = {};
    tonemapDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    tonemapDesc.NumRenderTargets = 1;
    tonemapDesc.RTVFormats[0] = detail::kDisplayFormat;
    tonemapDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    tonemapDesc.SampleDesc.Count = 1;
    tonemapDesc.SampleDesc.Quality = 0;
    hr = device->CreateGraphicsPipelineState(&tonemapDesc,
                                             IID_PPV_ARGS(&tonemapPso));
    if (FAILED(hr)) {
        return detail::Fail("CreateGraphicsPipelineState(tonemap)", hr, error);
    }
    return true;
}

bool D3D12Harness::Impl::CreateMeshes(std::string* error) {
    struct Definition {
        MeshKind kind;
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };
    const Definition definitions[] = {
        {MeshKind::Plane, MakePlaneVertices(1.0f, 6.0f), MakePlaneIndices()},
        {MeshKind::Cube, MakeCubeVertices(), MakeCubeIndices()},
        {MeshKind::Sphere, MakeSphereVertices(32, 16), MakeSphereIndices(32, 16)},
        {MeshKind::Quad, MakeQuadVertices(), MakeQuadIndices()},
    };

    for (const Definition& definition : definitions) {
        detail::Mesh& mesh =
            meshes[static_cast<uint32_t>(definition.kind)];
        const UINT64 vbBytes = definition.vertices.size() * sizeof(Vertex);
        const UINT64 ibBytes = definition.indices.size() * sizeof(uint32_t);
        const D3D12_HEAP_PROPERTIES uploadHeap =
            detail::Heap(D3D12_HEAP_TYPE_UPLOAD);
        const D3D12_RESOURCE_DESC vertexDesc = detail::BufferDesc(vbBytes);
        const D3D12_RESOURCE_DESC indexDesc = detail::BufferDesc(ibBytes);
        HRESULT hr = device->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &vertexDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&mesh.vertexBuffer));
        if (FAILED(hr)) {
            return detail::Fail("CreateCommittedResource(vertex buffer)", hr,
                                error);
        }
        hr = device->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &indexDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&mesh.indexBuffer));
        if (FAILED(hr)) {
            return detail::Fail("CreateCommittedResource(index buffer)", hr,
                                error);
        }
        void* mapped = nullptr;
        hr = mesh.vertexBuffer->Map(0, nullptr, &mapped);
        if (FAILED(hr)) {
            return detail::Fail("ID3D12Resource::Map(vertex buffer)", hr,
                                error);
        }
        std::memcpy(mapped, definition.vertices.data(), vbBytes);
        mesh.vertexBuffer->Unmap(0, nullptr);
        hr = mesh.indexBuffer->Map(0, nullptr, &mapped);
        if (FAILED(hr)) {
            return detail::Fail("ID3D12Resource::Map(index buffer)", hr, error);
        }
        std::memcpy(mapped, definition.indices.data(), ibBytes);
        mesh.indexBuffer->Unmap(0, nullptr);

        mesh.vertexView.BufferLocation = mesh.vertexBuffer->GetGPUVirtualAddress();
        mesh.vertexView.SizeInBytes = static_cast<UINT>(vbBytes);
        mesh.vertexView.StrideInBytes = sizeof(Vertex);
        mesh.indexView.BufferLocation = mesh.indexBuffer->GetGPUVirtualAddress();
        mesh.indexView.SizeInBytes = static_cast<UINT>(ibBytes);
        mesh.indexView.Format = DXGI_FORMAT_R32_UINT;
        mesh.indexCount = static_cast<uint32_t>(definition.indices.size());
    }
    return true;
}

bool D3D12Harness::Impl::CreateDescriptors(std::string* error) {
    srvSize = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    rtvSize =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    dsvSize =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    D3D12_DESCRIPTOR_HEAP_DESC srvDesc{};
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.NumDescriptors = detail::kDescriptorCount;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    srvDesc.NodeMask = 0;
    HRESULT hr = device->CreateDescriptorHeap(&srvDesc,
                                              IID_PPV_ARGS(&srvHeap));
    if (FAILED(hr)) {
        return detail::Fail("CreateDescriptorHeap(SRV)", hr, error);
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NumDescriptors = 2;
    rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&rtvHeap));
    if (FAILED(hr)) {
        return detail::Fail("CreateDescriptorHeap(RTV)", hr, error);
    }

    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc{};
    dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvDesc.NumDescriptors = 2;
    dsvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&dsvHeap));
    if (FAILED(hr)) {
        return detail::Fail("CreateDescriptorHeap(DSV)", hr, error);
    }
    return true;
}

bool D3D12Harness::Impl::CreateTargets(std::string* error) {
    const D3D12_HEAP_PROPERTIES defaultHeap =
        detail::Heap(D3D12_HEAP_TYPE_DEFAULT);
    const D3D12_HEAP_PROPERTIES uploadHeap =
        detail::Heap(D3D12_HEAP_TYPE_UPLOAD);
    const D3D12_HEAP_PROPERTIES readbackHeap =
        detail::Heap(D3D12_HEAP_TYPE_READBACK);

    D3D12_CLEAR_VALUE hdrClear{};
    hdrClear.Format = detail::kHdrFormat;
    hdrClear.Color[0] = 0.012f;
    hdrClear.Color[1] = 0.014f;
    hdrClear.Color[2] = 0.020f;
    hdrClear.Color[3] = 1.0f;
    const D3D12_RESOURCE_DESC sceneDesc = detail::TextureDesc(
        renderWidth, renderHeight, 1, detail::kHdrFormat,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    HRESULT hr = device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &sceneDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET, &hdrClear,
        IID_PPV_ARGS(&sceneColor));
    if (FAILED(hr)) {
        return detail::Fail("CreateCommittedResource(scene HDR)", hr, error);
    }

    D3D12_CLEAR_VALUE depthClear{};
    depthClear.Format = detail::kDepthFormat;
    depthClear.DepthStencil.Depth = 1.0f;
    depthClear.DepthStencil.Stencil = 0;
    const D3D12_RESOURCE_DESC depthDesc = detail::TextureDesc(
        renderWidth, renderHeight, 1, detail::kDepthFormat,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    hr = device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClear, IID_PPV_ARGS(&depth));
    if (FAILED(hr)) {
        return detail::Fail("CreateCommittedResource(scene depth)", hr, error);
    }

    D3D12_CLEAR_VALUE shadowClear{};
    shadowClear.Format = detail::kDepthFormat;
    shadowClear.DepthStencil.Depth = 1.0f;
    shadowClear.DepthStencil.Stencil = 0;
    const D3D12_RESOURCE_DESC shadowDesc = detail::TextureDesc(
        detail::kShadowSize, detail::kShadowSize, 1, DXGI_FORMAT_R32_TYPELESS,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    hr = device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &shadowDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &shadowClear,
        IID_PPV_ARGS(&shadowMap));
    if (FAILED(hr)) {
        return detail::Fail("CreateCommittedResource(shadow map)", hr, error);
    }

    D3D12_CLEAR_VALUE displayClear{};
    displayClear.Format = detail::kDisplayFormat;
    displayClear.Color[0] = 0.0f;
    displayClear.Color[1] = 0.0f;
    displayClear.Color[2] = 0.0f;
    displayClear.Color[3] = 1.0f;
    const D3D12_RESOURCE_DESC displayDesc = detail::TextureDesc(
        displayWidth, displayHeight, 1, detail::kDisplayFormat,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    hr = device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &displayDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET, &displayClear,
        IID_PPV_ARGS(&display));
    if (FAILED(hr)) {
        return detail::Fail("CreateCommittedResource(display)", hr, error);
    }

    D3D12_RENDER_TARGET_VIEW_DESC hdrRtv{};
    hdrRtv.Format = detail::kHdrFormat;
    hdrRtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    hdrRtv.Texture2D.MipSlice = 0;
    sceneRtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    device->CreateRenderTargetView(sceneColor.Get(), &hdrRtv, sceneRtv);

    D3D12_RENDER_TARGET_VIEW_DESC displayRtvDesc{};
    displayRtvDesc.Format = detail::kDisplayFormat;
    displayRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    displayRtvDesc.Texture2D.MipSlice = 0;
    displayRtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    displayRtv.ptr += rtvSize;
    device->CreateRenderTargetView(display.Get(), &displayRtvDesc, displayRtv);

    D3D12_DEPTH_STENCIL_VIEW_DESC mainDsvDesc{};
    mainDsvDesc.Format = detail::kDepthFormat;
    mainDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    mainDsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    mainDsvDesc.Texture2D.MipSlice = 0;
    mainDsv = dsvHeap->GetCPUDescriptorHandleForHeapStart();
    device->CreateDepthStencilView(depth.Get(), &mainDsvDesc, mainDsv);

    D3D12_DEPTH_STENCIL_VIEW_DESC shadowDsvDesc{};
    shadowDsvDesc.Format = detail::kDepthFormat;
    shadowDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    shadowDsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    shadowDsvDesc.Texture2D.MipSlice = 0;
    shadowDsv = dsvHeap->GetCPUDescriptorHandleForHeapStart();
    shadowDsv.ptr += dsvSize;
    device->CreateDepthStencilView(shadowMap.Get(), &shadowDsvDesc, shadowDsv);

    D3D12_SHADER_RESOURCE_VIEW_DESC shadowSrvDesc{};
    shadowSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    shadowSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    shadowSrvDesc.Shader4ComponentMapping =
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    shadowSrvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(
        shadowMap.Get(), &shadowSrvDesc, CpuHandle(detail::kShadowDescriptorIndex));
    shadowSrv = GpuHandle(detail::kShadowDescriptorIndex);

    D3D12_SHADER_RESOURCE_VIEW_DESC sceneSrvDesc{};
    sceneSrvDesc.Format = detail::kHdrFormat;
    sceneSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sceneSrvDesc.Shader4ComponentMapping =
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sceneSrvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(
        sceneColor.Get(), &sceneSrvDesc, CpuHandle(detail::kSceneDescriptorIndex));
    sceneSrv = GpuHandle(detail::kSceneDescriptorIndex);

    const D3D12_RESOURCE_DESC copyDesc = display->GetDesc();
    UINT64 totalBytes = 0;
    UINT64 rowSize = 0;
    device->GetCopyableFootprints(&copyDesc, 0, 1, 0, &readbackFootprint,
                                  &readbackRowCount, &rowSize, &totalBytes);
    const D3D12_RESOURCE_DESC readbackDesc = detail::BufferDesc(totalBytes);
    hr = device->CreateCommittedResource(
        &readbackHeap, D3D12_HEAP_FLAG_NONE, &readbackDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));
    if (FAILED(hr)) {
        return detail::Fail("CreateCommittedResource(readback)", hr, error);
    }

    const UINT64 cbBytes =
        static_cast<UINT64>(detail::kCbSlotBytes) * detail::kMaxDraws;
    const D3D12_RESOURCE_DESC cbDesc = detail::BufferDesc(cbBytes);
    hr = device->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &cbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&constantBuffer));
    if (FAILED(hr)) {
        return detail::Fail("CreateCommittedResource(constant buffer)", hr,
                            error);
    }
    hr = constantBuffer->Map(0, nullptr,
                             reinterpret_cast<void**>(&constantMapped));
    if (FAILED(hr)) {
        return detail::Fail("ID3D12Resource::Map(constant buffer)", hr, error);
    }
    return true;
}

}  // namespace q4
