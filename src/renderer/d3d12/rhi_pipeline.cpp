// Programmable graphics slice: shader modules, graphics pipelines, ordered
// draw passes, colour attachments, sampled depth and readbacks.
//
// The frame's declared accesses are validated by the portable frame graph
// before any GPU work is recorded. The backend then records exactly those
// passes in the declared order and keeps every object alive until the
// submission's completion token is reached.

#include "d3d12/d3d12_impl.h"

#include "rhi/frame_graph.h"

#include <d3dcompiler.h>

namespace doom3 {
namespace rhi {

using d3d12::detail::ChangeUse;
using d3d12::detail::Fail;

namespace {

const char* TargetForStage(ShaderStage stage) {
    switch (stage) {
    case ShaderStage::Vertex:
        return "vs_5_0";
    case ShaderStage::Pixel:
        return "ps_5_0";
    case ShaderStage::Compute:
        return "cs_5_0";
    }
    return nullptr;
}

std::string BlobText(ID3DBlob* blob) {
    if (blob == nullptr || blob->GetBufferPointer() == nullptr) {
        return {};
    }
    const auto* text = static_cast<const char*>(blob->GetBufferPointer());
    std::string result(text, blob->GetBufferSize());
    while (!result.empty() && (result.back() == '\0' || result.back() == '\n' ||
                               result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

D3D12_CULL_MODE NativeCull(CullMode mode) {
    switch (mode) {
        case CullMode::None:
            return D3D12_CULL_MODE_NONE;
        case CullMode::Front:
            return D3D12_CULL_MODE_FRONT;
        case CullMode::Back:
            return D3D12_CULL_MODE_BACK;
    }
    return D3D12_CULL_MODE_NONE;
}

D3D12_COMPARISON_FUNC NativeCompare(CompareFunc value) {
    switch (value) {
        case CompareFunc::Never:
            return D3D12_COMPARISON_FUNC_NEVER;
        case CompareFunc::Less:
            return D3D12_COMPARISON_FUNC_LESS;
        case CompareFunc::Equal:
            return D3D12_COMPARISON_FUNC_EQUAL;
        case CompareFunc::LessEqual:
            return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case CompareFunc::Greater:
            return D3D12_COMPARISON_FUNC_GREATER;
        case CompareFunc::NotEqual:
            return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case CompareFunc::GreaterEqual:
            return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case CompareFunc::Always:
            return D3D12_COMPARISON_FUNC_ALWAYS;
    }
    return D3D12_COMPARISON_FUNC_ALWAYS;
}

D3D12_STENCIL_OP NativeStencilOp(StencilOp value) {
    switch (value) {
        case StencilOp::Keep:
            return D3D12_STENCIL_OP_KEEP;
        case StencilOp::Zero:
            return D3D12_STENCIL_OP_ZERO;
        case StencilOp::Replace:
            return D3D12_STENCIL_OP_REPLACE;
        case StencilOp::IncrementSaturate:
            return D3D12_STENCIL_OP_INCR_SAT;
        case StencilOp::DecrementSaturate:
            return D3D12_STENCIL_OP_DECR_SAT;
        case StencilOp::Invert:
            return D3D12_STENCIL_OP_INVERT;
        case StencilOp::IncrementWrap:
            return D3D12_STENCIL_OP_INCR;
        case StencilOp::DecrementWrap:
            return D3D12_STENCIL_OP_DECR;
    }
    return D3D12_STENCIL_OP_KEEP;
}

void FillStencilFace(const StencilFaceState& source, D3D12_DEPTH_STENCILOP_DESC* target) {
    target->StencilFailOp = NativeStencilOp(source.failStencil);
    target->StencilDepthFailOp = NativeStencilOp(source.failDepth);
    target->StencilPassOp = NativeStencilOp(source.pass);
    target->StencilFunc = NativeCompare(source.func);
}

ComPtr<ID3D12RootSignature> CreateGraphicsLayout(ID3D12Device* device,
                                                 std::string* error) {
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0;
    range.RegisterSpace = 0;
    range.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER parameters[3]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[0].Descriptor.ShaderRegister = 0;
    parameters[0].Descriptor.RegisterSpace = 0;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable.NumDescriptorRanges = 1;
    parameters[1].DescriptorTable.pDescriptorRanges = &range;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    // Per-draw-item transform + tint. The classic ordered stream submits one
    // item per world surface, so the data cannot live in the per-pass b0 slot.
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[2].Descriptor.ShaderRegister = 1;
    parameters[2].Descriptor.RegisterSpace = 0;
    parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MinLOD = 0;
    sampler.MaxLOD = 0;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC description{};
    description.NumParameters = 3;
    description.pParameters = parameters;
    description.NumStaticSamplers = 1;
    description.pStaticSamplers = &sampler;
    description.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;
    const HRESULT hr = D3D12SerializeRootSignature(
        &description, D3D_ROOT_SIGNATURE_VERSION_1, serialized.GetAddressOf(),
        errors.GetAddressOf());
    if (FAILED(hr)) {
        ComPtr<ID3D12RootSignature> empty;
        Fail(Status::Error, error,
             "D3D12SerializeRootSignature(graphics) " + d3d12::detail::HrString(hr) +
                 (errors ? "; " + BlobText(errors.Get()) : ""));
        return empty;
    }
    ComPtr<ID3D12RootSignature> layout;
    const HRESULT create = device->CreateRootSignature(
        0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
        __uuidof(ID3D12RootSignature), reinterpret_cast<void**>(layout.GetAddressOf()));
    if (FAILED(create)) {
        Fail(Status::Error, error,
             "CreateRootSignature(graphics) " + d3d12::detail::HrString(create));
        return ComPtr<ID3D12RootSignature>();
    }
    return layout;
}

}  // namespace

Status d3d12::detail::Q1bState::CompileShader(const ShaderModuleDesc& desc,
                                             ShaderRecord* record,
                                             std::string* error) {
    if (record == nullptr) {
        return Fail(Status::InvalidArgument, error, "shader record is null");
    }
    if (desc.source.empty()) {
        return Fail(Status::InvalidArgument, error, "shader source is empty");
    }
    if (desc.entryPoint.empty()) {
        return Fail(Status::InvalidArgument, error, "shader entry point is empty");
    }
    const char* target = TargetForStage(desc.stage);
    if (target == nullptr) {
        return Fail(Status::InvalidArgument, error, "unknown shader stage");
    }
    ComPtr<ID3DBlob> code;
    ComPtr<ID3DBlob> errors;
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
    const HRESULT hr = D3DCompile(
        desc.source.data(), desc.source.size(), "q1b-fixture", nullptr, nullptr,
        desc.entryPoint.c_str(), target, flags, 0, code.GetAddressOf(),
        errors.GetAddressOf());
    if (FAILED(hr) || !code) {
        return Fail(Status::Error, error,
                    std::string("shader compilation failed for stage ") +
                        ToString(desc.stage) + " entry '" + desc.entryPoint + "' " +
                        d3d12::detail::HrString(hr) +
                        (errors ? "; " + BlobText(errors.Get()) : ""));
    }
    record->stage = desc.stage;
    const auto* bytes = static_cast<const uint8_t*>(code->GetBufferPointer());
    try {
        record->bytecode.assign(bytes, bytes + code->GetBufferSize());
    } catch (const std::bad_alloc&) {
        return Fail(Status::OutOfMemory, error, "shader bytecode could not be retained");
    }
    return Status::Ok;
}

Status d3d12::detail::Q1bState::CreateShaderModule(const ShaderModuleDesc& desc,
                                                  ShaderModule* module,
                                                  std::string* error) {
    ShaderRecord record;
    const Status compiled = CompileShader(desc, &record, error);
    if (compiled != Status::Ok) {
        return compiled;
    }
    record.id = nextShaderId++;
    module->id = record.id;
    shaders.emplace(record.id, std::move(record));
    if (error) {
        error->clear();
    }
    return Status::Ok;
}

Status d3d12::detail::Q1bState::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc,
                                                      Pipeline* pipeline,
                                                      std::string* error) {
    if (desc.colorAttachmentCount > 4) {
        return Fail(Status::InvalidArgument, error,
                    "graphics pipeline colour attachment count must be 0..4");
    }
    if (desc.stencil.enabled && !(desc.depthTest || desc.depthWrite)) {
        return Fail(Status::InvalidArgument, error,
                    "a stencil pipeline needs the depth-stencil attachment enabled");
    }
    const auto vertex = shaders.find(desc.vertexShader.id);
    const auto pixel = shaders.find(desc.pixelShader.id);
    if (vertex == shaders.end() || vertex->second.stage != ShaderStage::Vertex) {
        return Fail(Status::InvalidArgument, error,
                    "vertex shader handle does not name a vertex module");
    }
    if (pixel == shaders.end() || pixel->second.stage != ShaderStage::Pixel) {
        return Fail(Status::InvalidArgument, error,
                    "pixel shader handle does not name a pixel module");
    }
    ComPtr<ID3D12RootSignature> layout = CreateGraphicsLayout(host->Device(), error);
    if (!layout) {
        return Status::Error;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};
    state.pRootSignature = layout.Get();
    state.VS.pShaderBytecode = vertex->second.bytecode.data();
    state.VS.BytecodeLength = vertex->second.bytecode.size();
    state.PS.pShaderBytecode = pixel->second.bytecode.data();
    state.PS.BytecodeLength = pixel->second.bytecode.size();
    for (uint32_t i = 0; i < desc.colorAttachmentCount; ++i) {
        state.BlendState.RenderTarget[i].BlendEnable = FALSE;
        state.BlendState.RenderTarget[i].LogicOpEnable = FALSE;
        state.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
        state.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
        state.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
        state.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
        state.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
        state.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        state.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
        state.BlendState.RenderTarget[i].RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_ALL;
        state.RTVFormats[i] = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    state.SampleMask = UINT_MAX;
    state.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    state.RasterizerState.CullMode = NativeCull(desc.cullMode);
    state.RasterizerState.FrontCounterClockwise = FALSE;
    state.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    state.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    state.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    // D3D12 has no portable depth-clamp disable: clipping is always on. A
    // shadow volume must therefore be closed so the z-fail count is bounded by
    // the back-face cap instead of a clamped far plane. See lighting.cpp.
    state.RasterizerState.DepthClipEnable = TRUE;
    state.RasterizerState.MultisampleEnable = FALSE;
    state.RasterizerState.AntialiasedLineEnable = FALSE;
    state.RasterizerState.ForcedSampleCount = 0;
    state.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    state.DepthStencilState.DepthEnable = desc.depthTest || desc.depthWrite;
    state.DepthStencilState.DepthWriteMask =
        desc.depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    state.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    state.DepthStencilState.StencilEnable = desc.stencil.enabled ? TRUE : FALSE;
    state.DepthStencilState.StencilReadMask = desc.stencil.readMask;
    state.DepthStencilState.StencilWriteMask = desc.stencil.writeMask;
    FillStencilFace(desc.stencil.front, &state.DepthStencilState.FrontFace);
    FillStencilFace(desc.stencil.back, &state.DepthStencilState.BackFace);
    // A stencil pipeline must use the depth-stencil format; every pipeline in
    // a stencil frame does, so all their PSOs agree on the attachment format.
    const bool usesDepth =
        desc.depthTest || desc.depthWrite || desc.sampleDepth || desc.stencil.enabled;
    state.DSVFormat = usesDepth
                          ? (desc.stencil.enabled ? DXGI_FORMAT_D24_UNORM_S8_UINT
                                                  : DXGI_FORMAT_D32_FLOAT)
                          : DXGI_FORMAT_UNKNOWN;
    if (desc.vertexInput) {
        static const D3D12_INPUT_ELEMENT_DESC kLayout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };
        static_assert(sizeof(Vertex) == 20, "portable vertex stride changed");
        state.InputLayout = {kLayout, 2};
    } else {
        state.InputLayout = {nullptr, 0};
    }
    state.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    state.NumRenderTargets = desc.colorAttachmentCount;
    state.SampleDesc.Count = 1;
    state.SampleDesc.Quality = 0;
    state.NodeMask = 0;
    state.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    PipelineRecord record;
    record.id = nextPipelineId++;
    record.layout = layout;
    record.colorCount = desc.colorAttachmentCount;
    record.depthTest = desc.depthTest;
    record.depthWrite = desc.depthWrite;
    record.sampleDepth = desc.sampleDepth;
    record.vertexInput = desc.vertexInput;
    record.textured = desc.textured;
    record.stencilEnabled = desc.stencil.enabled;
    record.stencilWriteMask = desc.stencil.enabled ? desc.stencil.writeMask : 0;
    const HRESULT hr = host->Device()->CreateGraphicsPipelineState(
        &state, __uuidof(ID3D12PipelineState),
        reinterpret_cast<void**>(record.state.GetAddressOf()));
    if (FAILED(hr)) {
        return Fail(Status::Error, error,
                    "CreateGraphicsPipelineState " + d3d12::detail::HrString(hr));
    }
    pipeline->id = record.id;
    pipelines.emplace(record.id, std::move(record));
    if (error) {
        error->clear();
    }
    return Status::Ok;
}

Status d3d12::detail::Q1bState::RecordDrawPasses(
    ID3D12GraphicsCommandList* list, FrameSlot& slot, const DrawFrameDesc& desc,
    d3d12::detail::SubmissionRecord& record, bool frameStencil, std::string* error) {
    const UINT increment =
        host->Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeap{};
    rtvHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeap.NumDescriptors = desc.attachmentCount;
    rtvHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HRESULT hr = host->Device()->CreateDescriptorHeap(
        &rtvHeap, __uuidof(ID3D12DescriptorHeap),
        reinterpret_cast<void**>(record.colorViews.GetAddressOf()));
    if (FAILED(hr)) {
        return Fail(Status::Error, error,
                    "CreateDescriptorHeap(colour views) " + d3d12::detail::HrString(hr));
    }
    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        record.colorViews->GetCPUDescriptorHandleForHeapStart();
    for (uint32_t i = 0; i < desc.attachmentCount; ++i) {
        host->Device()->CreateRenderTargetView(record.colors[i].Get(), nullptr, rtv);
        rtv.ptr += increment;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE dsv{};
    if (record.depth) {
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeap{};
        dsvHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeap.NumDescriptors = 1;
        dsvHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        hr = host->Device()->CreateDescriptorHeap(
            &dsvHeap, __uuidof(ID3D12DescriptorHeap),
            reinterpret_cast<void**>(record.depthView.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error,
                        "CreateDescriptorHeap(depth view) " +
                            d3d12::detail::HrString(hr));
        }
        dsv = record.depthView->GetCPUDescriptorHandleForHeapStart();
        host->Device()->CreateDepthStencilView(record.depth.Get(), nullptr, dsv);
    }

    D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(desc.width),
                            static_cast<float>(desc.height), 0.0f, 1.0f};
    D3D12_RECT scissor{0, 0, static_cast<LONG>(desc.width),
                       static_cast<LONG>(desc.height)};
    list->RSSetViewports(1, &viewport);
    list->RSSetScissorRects(1, &scissor);

    // Every draw item owns one texture descriptor and one 256-byte constant
    // slice. Both are submission-scoped, so a later frame cannot overwrite data
    // the device is still reading.
    uint32_t totalItems = 0;
    for (uint32_t passIndex = 0; passIndex < desc.passCount; ++passIndex) {
        totalItems += desc.passes[passIndex].itemCount;
    }
    const UINT srvIncrement = host->Device()->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    if (totalItems > 0) {
        D3D12_DESCRIPTOR_HEAP_DESC itemHeap{};
        itemHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        itemHeap.NumDescriptors = totalItems;
        itemHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        itemHeap.NodeMask = 0;
        hr = host->Device()->CreateDescriptorHeap(
            &itemHeap, __uuidof(ID3D12DescriptorHeap),
            reinterpret_cast<void**>(record.itemViews.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error,
                        "CreateDescriptorHeap(item views) " +
                            d3d12::detail::HrString(hr));
        }
        const uint64_t constantBytes = static_cast<uint64_t>(totalItems) * 256ull;
        D3D12_RESOURCE_DESC constants{};
        constants.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        constants.Alignment = 0;
        constants.Width = constantBytes;
        constants.Height = 1;
        constants.DepthOrArraySize = 1;
        constants.MipLevels = 1;
        constants.Format = DXGI_FORMAT_UNKNOWN;
        constants.SampleDesc.Count = 1;
        constants.SampleDesc.Quality = 0;
        constants.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        constants.Flags = D3D12_RESOURCE_FLAG_NONE;
        D3D12_HEAP_PROPERTIES uploadHeap{};
        uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
        uploadHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        uploadHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        uploadHeap.CreationNodeMask = 1;
        uploadHeap.VisibleNodeMask = 1;
        hr = host->Device()->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &constants,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource),
            reinterpret_cast<void**>(record.itemConstants.GetAddressOf()));
        if (FAILED(hr)) {
            return Fail(Status::Error, error,
                        "CreateCommittedResource(item constants) " +
                            d3d12::detail::HrString(hr));
        }
        void* mapped = nullptr;
        hr = record.itemConstants->Map(0, nullptr, &mapped);
        if (FAILED(hr) || mapped == nullptr) {
            return Fail(Status::Error, error, "Map(item constants) failed");
        }
        auto* bytes = static_cast<uint8_t*>(mapped);
        uint32_t flat = 0;
        for (uint32_t passIndex = 0; passIndex < desc.passCount; ++passIndex) {
            const DrawPassDesc& pass = desc.passes[passIndex];
            for (uint32_t itemIndex = 0; itemIndex < pass.itemCount; ++itemIndex) {
                const DrawItem& item = pass.items[itemIndex];
                uint8_t* target = bytes + static_cast<uint64_t>(flat) * 256ull;
                std::memcpy(target, item.transform, sizeof(item.transform));
                std::memcpy(target + sizeof(item.transform), item.tint,
                            sizeof(item.tint));
                D3D12_CPU_DESCRIPTOR_HANDLE handle =
                    record.itemViews->GetCPUDescriptorHandleForHeapStart();
                handle.ptr += static_cast<SIZE_T>(flat) * srvIncrement;
                D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
                srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srv.Texture2D.MipLevels = static_cast<UINT>(-1);
                const auto texture = textures.find(item.texture.id);
                if (item.texture.id != 0 && texture != textures.end()) {
                    srv.Format = texture->second.nativeFormat;
                    host->Device()->CreateShaderResourceView(
                        texture->second.resource.Get(), &srv, handle);
                } else {
                    // A tint-only pipeline still has a descriptor-table root
                    // parameter; bind a null view rather than leaving the
                    // descriptor uninitialised.
                    host->Device()->CreateShaderResourceView(nullptr, &srv, handle);
                }
                ++flat;
            }
        }
        record.itemConstants->Unmap(0, nullptr);
        record.itemConstantCount = totalItems;
    }

    ID3D12DescriptorHeap* heaps[] = {
        record.itemViews ? record.itemViews.Get() : slot.heap.Get()};
    list->SetDescriptorHeaps(1, heaps);

    const D3D12_CPU_DESCRIPTOR_HANDLE srvCpu =
        slot.heap->GetCPUDescriptorHandleForHeapStart();
    const D3D12_GPU_DESCRIPTOR_HANDLE srvGpu =
        slot.heap->GetGPUDescriptorHandleForHeapStart();
    const D3D12_GPU_DESCRIPTOR_HANDLE itemGpu =
        record.itemViews ? record.itemViews->GetGPUDescriptorHandleForHeapStart()
                         : D3D12_GPU_DESCRIPTOR_HANDLE{};

    std::vector<bool> cleared(desc.attachmentCount, false);
    bool depthCleared = false;
    D3D12_RESOURCE_STATES depthUse = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    for (uint32_t passIndex = 0; passIndex < desc.passCount; ++passIndex) {
        const DrawPassDesc& pass = desc.passes[passIndex];
        const auto pipeline = pipelines.find(pass.pipeline.id);
        if (pipeline == pipelines.end()) {
            return Fail(Status::InvalidArgument, error,
                        "pass " + std::to_string(passIndex) +
                            " names an unknown pipeline");
        }
        const PipelineRecord& program = pipeline->second;
        if (program.compute) {
            return Fail(Status::InvalidArgument, error,
                        "pass " + std::to_string(passIndex) +
                            " uses a compute pipeline");
        }
        if (program.colorCount != pass.colorCount) {
            return Fail(Status::InvalidArgument, error,
                        "pass " + std::to_string(passIndex) + " declares " +
                            std::to_string(pass.colorCount) + " colour(s) but pipeline " +
                            std::to_string(program.id) + " writes " +
                            std::to_string(program.colorCount));
        }
        if (pass.colorCount != 0 &&
            (pass.colorIndices == nullptr || pass.colorClears == nullptr)) {
            return Fail(Status::InvalidArgument, error,
                        "pass " + std::to_string(passIndex) +
                            " has no colour attachment declaration");
        }
        if (program.vertexInput) {
            if (pass.itemCount == 0 || pass.items == nullptr) {
                return Fail(Status::InvalidArgument, error,
                            "pass " + std::to_string(passIndex) +
                                " uses a vertex-input pipeline without draw items");
            }
        } else if (pass.itemCount != 0) {
            return Fail(Status::InvalidArgument, error,
                        "pass " + std::to_string(passIndex) +
                            " declares draw items for a procedural fullscreen pipeline");
        }
        if (program.sampleDepth && totalItems > 0) {
            return Fail(Status::Unsupported, error,
                        "a frame cannot mix depth sampling with per-item texture views: "
                        "one shader-visible descriptor heap is bound per pass");
        }
        if (pass.colorCount == 0 && !program.vertexInput) {
            // Zero colour targets is the stencil-only shadow-volume pass.
            return Fail(Status::InvalidArgument, error,
                        "pass " + std::to_string(passIndex) +
                            " writes no colour and draws no items");
        }
        if ((program.depthTest || program.depthWrite || program.sampleDepth) &&
            !record.depth) {
            return Fail(Status::InvalidArgument, error,
                        "pass " + std::to_string(passIndex) +
                            " uses depth but the frame has no depth attachment");
        }
        if (program.depthWrite && depthUse != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
            return Fail(Status::InvalidArgument, error,
                        "pass " + std::to_string(passIndex) +
                            " writes depth after it became a sampled input");
        }

        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> targets;
        targets.reserve(pass.colorCount);
        for (uint32_t i = 0; i < pass.colorCount; ++i) {
            const uint32_t index = pass.colorIndices[i];
            D3D12_CPU_DESCRIPTOR_HANDLE handle =
                record.colorViews->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += static_cast<SIZE_T>(index) * increment;
            if (index >= desc.attachmentCount) {
                return Fail(Status::InvalidArgument, error,
                            "pass " + std::to_string(passIndex) +
                                " names an out-of-range colour attachment");
            }
            if (!cleared[index]) {
                const ClearRow& clear = pass.colorClears[i];
                const FLOAT colour[4] = {clear.red / 255.0f, clear.green / 255.0f,
                                         clear.blue / 255.0f, clear.alpha / 255.0f};
                list->ClearRenderTargetView(handle, colour, 0, nullptr);
                cleared[index] = true;
            }
            targets.push_back(handle);
        }
        if (record.depth && !depthCleared) {
            const D3D12_CLEAR_FLAGS flags =
                frameStencil ? (D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL)
                             : D3D12_CLEAR_FLAG_DEPTH;
            list->ClearDepthStencilView(dsv, flags, desc.depthClear, 0, 0, nullptr);
            depthCleared = true;
        }
        if (program.sampleDepth) {
            ChangeUse(list, record.depth.Get(),
                      depthUse, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            depthUse = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }
        std::memcpy(slot.mapped, pass.tint, sizeof(pass.tint));

        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Format = DXGI_FORMAT_R32_FLOAT;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Texture2D.MipLevels = 1;
        host->Device()->CreateShaderResourceView(
            program.sampleDepth ? record.depth.Get() : nullptr, &srv, srvCpu);

        list->SetGraphicsRootSignature(program.layout.Get());
        list->SetPipelineState(program.state.Get());
        list->SetGraphicsRootConstantBufferView(
            0, slot.constants->GetGPUVirtualAddress());
        list->SetGraphicsRootDescriptorTable(1, srvGpu);
        list->OMSetRenderTargets(pass.colorCount, targets.data(), FALSE,
                                 record.depth ? &dsv : nullptr);
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        if (program.vertexInput) {
            uint32_t flat = 0;
            for (uint32_t earlier = 0; earlier < passIndex; ++earlier) {
                flat += desc.passes[earlier].itemCount;
            }
            for (uint32_t itemIndex = 0; itemIndex < pass.itemCount; ++itemIndex) {
                const DrawItem& item = pass.items[itemIndex];
                const auto vertex = buffers.find(item.vertexBuffer.id);
                if (vertex == buffers.end()) {
                    return Fail(Status::InvalidArgument, error,
                                "draw item names an unknown vertex buffer");
                }
                D3D12_VERTEX_BUFFER_VIEW vertexView{};
                vertexView.BufferLocation = vertex->second.resource->GetGPUVirtualAddress();
                vertexView.SizeInBytes = vertex->second.byteCount;
                vertexView.StrideInBytes = static_cast<UINT>(sizeof(Vertex));
                list->IASetVertexBuffers(0, 1, &vertexView);
                D3D12_INDEX_BUFFER_VIEW indexView{};
                bool indexed = false;
                if (item.indexCount != 0) {
                    const auto index = buffers.find(item.indexBuffer.id);
                    if (index == buffers.end() ||
                        index->second.usage != BufferUsage::Index) {
                        return Fail(Status::InvalidArgument, error,
                                    "draw item names an unknown index buffer");
                    }
                    indexView.BufferLocation =
                        index->second.resource->GetGPUVirtualAddress();
                    indexView.SizeInBytes = index->second.byteCount;
                    indexView.Format = DXGI_FORMAT_R16_UINT;
                    list->IASetIndexBuffer(&indexView);
                    indexed = true;
                }
                if (program.stencilEnabled) {
                    list->OMSetStencilRef(item.stencilRef);
                }
                D3D12_GPU_DESCRIPTOR_HANDLE table = itemGpu;
                table.ptr += static_cast<UINT64>(flat + itemIndex) * srvIncrement;
                list->SetGraphicsRootDescriptorTable(1, table);
                list->SetGraphicsRootConstantBufferView(
                    2, record.itemConstants->GetGPUVirtualAddress() +
                           static_cast<UINT64>(flat + itemIndex) * 256ull);
                if (indexed) {
                    list->DrawIndexedInstanced(item.indexCount, 1, 0, 0, 0);
                } else {
                    list->DrawInstanced(item.vertexCount, 1, 0, 0);
                }
            }
        } else {
            list->SetGraphicsRootConstantBufferView(
                2, slot.constants->GetGPUVirtualAddress());
            list->SetGraphicsRootDescriptorTable(1, srvGpu);
            if (program.stencilEnabled) {
                // The procedural shade pass has no item to carry a reference.
                list->OMSetStencilRef(0);
            }
            list->DrawInstanced(3, 1, 0, 0);
        }

        record.journal.push_back(d3d12::JournalEntry{
            "pass " + std::to_string(passIndex), "declared",
            program.sampleDepth ? "depth read + " + std::to_string(pass.colorCount) +
                                      " colour write"
                                : std::to_string(pass.colorCount) + " colour write",
            "pipeline " + std::to_string(program.id)});
    }

    for (uint32_t i = 0; i < desc.attachmentCount; ++i) {
        ChangeUse(list, record.colors[i].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                  D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION destination{};
        destination.pResource = record.colorReadbacks[i].Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint.Offset = 0;
        destination.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        destination.PlacedFootprint.Footprint.Width = desc.width;
        destination.PlacedFootprint.Footprint.Height = desc.height;
        destination.PlacedFootprint.Footprint.Depth = 1;
        destination.PlacedFootprint.Footprint.RowPitch = record.colorPitches[i];
        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = record.colors[i].Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        source.SubresourceIndex = 0;
        list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        record.journal.push_back(d3d12::JournalEntry{
            "attachment " + std::to_string(i), "RENDER_TARGET", "COPY_SOURCE",
            "readback copy recorded"});
    }
    return Status::Ok;
}

Status d3d12::detail::Q1bState::SubmitDrawFrame(const DrawFrameDesc& desc,
                                               Submission* submission,
                                               std::string* error, bool gated) {
    if (host->DeviceRemoved(error)) {
        return Status::DeviceLost;
    }
    if (desc.width == 0 || desc.height == 0) {
        return Fail(Status::InvalidArgument, error, "draw frame dimensions are zero");
    }
    if (desc.attachmentCount == 0 || desc.attachmentCount > 4) {
        return Fail(Status::InvalidArgument, error,
                    "draw frame colour attachment count must be 1..4");
    }
    if (desc.passCount == 0 || desc.passes == nullptr) {
        return Fail(Status::InvalidArgument, error, "draw frame has no passes");
    }
    std::vector<bool> written(desc.attachmentCount, false);
    for (uint32_t passIndex = 0; passIndex < desc.passCount; ++passIndex) {
        const DrawPassDesc& pass = desc.passes[passIndex];
        if (pass.colorCount != 0 && pass.colorIndices == nullptr) {
            return Fail(Status::InvalidArgument, error,
                        "pass " + std::to_string(passIndex) + " has no colours");
        }
        for (uint32_t i = 0; i < pass.colorCount; ++i) {
            const uint32_t index = pass.colorIndices[i];
            if (index >= desc.attachmentCount) {
                return Fail(Status::InvalidArgument, error,
                            "pass " + std::to_string(passIndex) +
                                " names an out-of-range colour attachment");
            }
            if (written[index]) {
                return Fail(Status::InvalidArgument, error,
                            "colour attachment " + std::to_string(index) +
                                " is written by more than one pass");
            }
            written[index] = true;
        }
    }
    for (uint32_t i = 0; i < desc.attachmentCount; ++i) {
        if (!written[i]) {
            return Fail(Status::InvalidArgument, error,
                        "colour attachment " + std::to_string(i) +
                            " is declared but never written");
        }
    }

    // Q2 geometry validation happens before any GPU work is recorded. A
    // stencil-enabled pipeline fixes the frame's depth-stencil format, so every
    // pipeline in that frame must enable stencil as well.
    bool frameStencil = false;
    for (uint32_t passIndex = 0; passIndex < desc.passCount; ++passIndex) {
        const DrawPassDesc& pass = desc.passes[passIndex];
        const auto pipeline = pipelines.find(pass.pipeline.id);
        if (pipeline == pipelines.end()) {
            return Fail(Status::InvalidArgument, error,
                        "pass " + std::to_string(passIndex) +
                            " names an unknown pipeline");
        }
        const PipelineRecord& program = pipeline->second;
        if (program.stencilEnabled) {
            frameStencil = true;
        }
        if (program.vertexInput) {
            if (pass.itemCount == 0 || pass.items == nullptr) {
                return Fail(Status::InvalidArgument, error,
                            "pass " + std::to_string(passIndex) +
                                " uses a vertex-input pipeline without draw items");
            }
            for (uint32_t itemIndex = 0; itemIndex < pass.itemCount; ++itemIndex) {
                const DrawItem& item = pass.items[itemIndex];
                if (item.vertexBuffer.id == 0 || item.vertexCount == 0 ||
                    buffers.find(item.vertexBuffer.id) == buffers.end()) {
                    return Fail(Status::InvalidArgument, error,
                                "draw item " + std::to_string(itemIndex) + " in pass " +
                                    std::to_string(passIndex) +
                                    " has no usable vertex buffer");
                }
                if (item.indexCount != 0 &&
                    (item.indexBuffer.id == 0 ||
                     buffers.find(item.indexBuffer.id) == buffers.end())) {
                    return Fail(Status::InvalidArgument, error,
                                "draw item " + std::to_string(itemIndex) + " in pass " +
                                    std::to_string(passIndex) +
                                    " has no usable index buffer");
                }
                if (program.textured &&
                    (item.texture.id == 0 ||
                     textures.find(item.texture.id) == textures.end())) {
                    return Fail(Status::InvalidArgument, error,
                                "draw item " + std::to_string(itemIndex) + " in pass " +
                                    std::to_string(passIndex) +
                                    " has no usable texture for a textured pipeline");
                }
            }
        } else if (pass.itemCount != 0) {
            return Fail(Status::InvalidArgument, error,
                        "pass " + std::to_string(passIndex) +
                            " declares draw items for a procedural fullscreen pipeline");
        }
    }
    if (frameStencil) {
        if (!desc.depthEnabled) {
            return Fail(Status::InvalidArgument, error,
                        "a stencil frame needs the depth-stencil attachment");
        }
        for (uint32_t passIndex = 0; passIndex < desc.passCount; ++passIndex) {
            const auto pipeline = pipelines.find(desc.passes[passIndex].pipeline.id);
            if (pipeline != pipelines.end() && !pipeline->second.stencilEnabled) {
                return Fail(Status::InvalidArgument, error,
                            "pass " + std::to_string(passIndex) +
                                " uses a pipeline without stencil in a stencil frame; "
                                "all PSOs must agree on the depth-stencil format");
            }
            if (pipeline != pipelines.end() && pipeline->second.sampleDepth) {
                return Fail(Status::Unsupported, error,
                            "depth sampling is not supported in a stencil frame "
                            "(D24_UNORM_S8_UINT has no R32_FLOAT read view here)");
            }
        }
    }

    std::vector<GraphResource> resources;
    resources.reserve(desc.attachmentCount + 1);
    for (uint32_t i = 0; i < desc.attachmentCount; ++i) {
        resources.push_back(GraphResource{i, false});
    }
    const uint32_t depthResource = desc.attachmentCount;
    if (desc.depthEnabled) {
        resources.push_back(GraphResource{depthResource, true});
    }
    std::vector<PassDeclaration> declarations;
    declarations.reserve(desc.passCount);
    for (uint32_t passIndex = 0; passIndex < desc.passCount; ++passIndex) {
        const DrawPassDesc& pass = desc.passes[passIndex];
        PassDeclaration declaration;
        declaration.name = "pass " + std::to_string(passIndex);
        for (uint32_t i = 0; i < pass.colorCount; ++i) {
            declaration.uses.push_back(PassUse{pass.colorIndices[i], Access::Write});
        }
        const auto pipeline = pipelines.find(pass.pipeline.id);
        if (pipeline == pipelines.end()) {
            return Fail(Status::InvalidArgument, error,
                        "pass " + std::to_string(passIndex) +
                            " names an unknown pipeline");
        }
        if ((pipeline->second.depthTest || pipeline->second.depthWrite ||
             pipeline->second.sampleDepth) &&
            !desc.depthEnabled) {
            return Fail(Status::InvalidArgument, error,
                        "pass " + std::to_string(passIndex) +
                            " uses depth but the frame has no depth attachment");
        }
        if (pipeline->second.depthWrite ||
            (pipeline->second.stencilEnabled &&
             pipeline->second.stencilWriteMask != 0)) {
            // Stencil lives in the depth-stencil resource, so an op that writes
            // it is a write of that resource for ordering purposes.
            declaration.uses.push_back(PassUse{depthResource, Access::Write});
        } else if (pipeline->second.sampleDepth ||
                   pipeline->second.stencilEnabled) {
            declaration.uses.push_back(PassUse{depthResource, Access::Read});
        }
        declarations.push_back(std::move(declaration));
    }
    const Schedule schedule = SchedulePasses(resources, declarations);
    if (schedule.status != Status::Ok) {
        return Fail(Status::InvalidArgument, error,
                    "frame graph rejected the declared accesses: " + schedule.error);
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
    record.width = desc.width;
    record.height = desc.height;
    record.slotIndex = slot->index;
    record.slotCompletedAtAcquire = completedAtAcquire;
    record.attachmentCount = desc.attachmentCount;
    record.colors.resize(desc.attachmentCount);
    record.colorReadbacks.resize(desc.attachmentCount);
    record.colorPitches.resize(desc.attachmentCount);
    record.colorStrides.resize(desc.attachmentCount);
    // The first pass that names an attachment performs its only clear. Hand
    // that value to creation so the debug layer sees a matching optimized clear
    // value instead of reporting a mismatching-clear warning on a correct run.
    std::vector<ClearRow> firstClears(desc.attachmentCount);
    std::vector<bool> firstClearKnown(desc.attachmentCount, false);
    for (uint32_t passIndex = 0; passIndex < desc.passCount; ++passIndex) {
        const DrawPassDesc& pass = desc.passes[passIndex];
        if (pass.colorClears == nullptr) {
            continue;
        }
        for (uint32_t i = 0; i < pass.colorCount; ++i) {
            const uint32_t index = pass.colorIndices[i];
            if (!firstClearKnown[index]) {
                firstClears[index] = pass.colorClears[i];
                firstClearKnown[index] = true;
            }
        }
    }
    for (uint32_t i = 0; i < desc.attachmentCount; ++i) {
        const Status created = CreateColorAttachment(
            desc.width, desc.height, firstClearKnown[i] ? &firstClears[i] : nullptr,
            &record.colors[i], &record.colorStrides[i], &record.colorPitches[i],
            &record.colorReadbacks[i], error);
        if (created != Status::Ok) {
            return created;
        }
    }
    if (desc.depthEnabled) {
        const Status created = CreateDepthAttachment(desc.width, desc.height,
                                                     desc.depthClear, frameStencil,
                                                     &record.depth, error);
        if (created != Status::Ok) {
            return created;
        }
    }

    const Status recorded =
        RecordDrawPasses(slot->list.Get(), *slot, desc, record, frameStencil, error);
    if (recorded != Status::Ok) {
        return recorded;
    }
    const HRESULT close = slot->list->Close();
    if (FAILED(close)) {
        return Fail(Status::Error, error,
                    "ID3D12GraphicsCommandList::Close " +
                        d3d12::detail::HrString(close));
    }
    d3d12::detail::SubmissionRecord* storedPtr =
        host->AddHostRecord(std::move(record), error);
    if (storedPtr == nullptr) {
        return Fail(Status::Error, error, "submission identity collision");
    }
    d3d12::detail::SubmissionRecord& stored = *storedPtr;
    if (gated) {
        stored.gated = true;
        const HRESULT fenceResult = host->Device()->CreateFence(
            0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
            reinterpret_cast<void**>(stored.gateFence.GetAddressOf()));
        if (FAILED(fenceResult)) {
            return Fail(Status::Error, error,
                        "CreateFence(draw gate) " +
                            d3d12::detail::HrString(fenceResult));
        }
        const HRESULT waitResult = host->Queue()->Wait(stored.gateFence.Get(), 1);
        if (FAILED(waitResult)) {
            return Fail(Status::Error, error,
                        "CommandQueue::Wait(draw gate) " +
                            d3d12::detail::HrString(waitResult));
        }
    }
    FinishSlot(*slot, stored.serial);
    ID3D12CommandList* lists[] = {slot->list.Get()};
    host->Queue()->ExecuteCommandLists(1, lists);
    const HRESULT signalled =
        host->Queue()->Signal(host->Completion(), stored.serial);
    if (FAILED(signalled)) {
        stored.signalled = false;
        return Fail(Status::Error, error,
                    "CommandQueue::Signal(draw) " +
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

Status BackendAccess::SubmitDrawGated(Device& device, const DrawFrameDesc& desc,
                                     Submission* submission, std::string* error) {
    if (!device.impl_->q1b) {
        return Fail(Status::Error, error, "device is not initialized");
    }
    return device.impl_->q1b->SubmitDrawFrame(desc, submission, error, true);
}

}  // namespace rhi
}  // namespace doom3
