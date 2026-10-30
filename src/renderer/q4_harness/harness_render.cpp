#include "harness_impl.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace q4 {
namespace {

using namespace detail;

XMMATRIX MakeTransform(float scale, float rotationYRadians, float x, float y,
                       float z) {
    return XMMatrixMultiply(
        XMMatrixMultiply(XMMatrixScaling(scale, scale, scale),
                         XMMatrixRotationY(rotationYRadians)),
        XMMatrixTranslation(x, y, z));
}

// IEEE 754 binary16 -> float, for the HDR probe readback.
float HalfToFloat(uint16_t value) {
    const uint32_t sign = (value >> 15) & 1u;
    const uint32_t exponent = (value >> 10) & 0x1Fu;
    const uint32_t mantissa = value & 0x3FFu;
    float result = 0.0f;
    if (exponent == 0) {
        result = mantissa == 0 ? 0.0f
                               : std::ldexp(static_cast<float>(mantissa), -24);
    } else if (exponent == 0x1F) {
        result = mantissa == 0 ? std::numeric_limits<float>::infinity()
                               : std::numeric_limits<float>::quiet_NaN();
    } else {
        result = std::ldexp(1.0f + static_cast<float>(mantissa) / 1024.0f,
                            static_cast<int>(exponent) - 15);
    }
    return sign != 0 ? -result : result;
}

}  // namespace

bool D3D12Harness::Impl::BuildDraws(const ContentCatalog& catalog,
                                    const std::string& scene,
                                    uint32_t frameIndex,
                                    std::vector<detail::DrawItem>* draws,
                                    std::string* reason) const {
    (void)catalog;
    draws->clear();
    auto add = [&](MeshKind kind, const XMMATRIX& world, const char* cell,
                   bool alphaTest, bool emissive, bool transmission) {
        if (!HasMaterialImpl(cell)) {
            return;
        }
        detail::DrawItem item;
        item.mesh = kind;
        item.world = world;
        item.cell = cell;
        item.alphaTest = alphaTest;
        item.emissive = emissive;
        item.transmission = transmission;
        item.skinned = kind == MeshKind::SkinnedCharacter;
        draws->push_back(item);
    };

    const XMMATRIX ground = XMMatrixScaling(24.0f, 1.0f, 24.0f);
    const XMMATRIX cube = MakeTransform(1.8f, XMConvertToRadians(20.0f), 0.0f,
                                        0.9f, 0.0f);
    const XMMATRIX sphere = MakeTransform(1.7f, 0.0f, 2.3f, 0.85f, -0.6f);
    const XMMATRIX metalSphere = MakeTransform(1.7f, 0.0f, -2.3f, 0.85f, 0.3f);

    if (scene == "static_opaque") {
        add(MeshKind::Plane, ground, "opaque_dielectric", false, false, false);
        add(MeshKind::Cube, cube, "opaque_dielectric", false, false, false);
        add(MeshKind::Sphere, sphere, "opaque_dielectric", false, false, false);
        add(MeshKind::Sphere, metalSphere, "opaque_metal", false, false, false);
    } else if (scene == "moving") {
        const float angle =
            XMConvertToRadians(20.0f + static_cast<float>(frameIndex) * 25.0f);
        const XMMATRIX movingCube =
            MakeTransform(1.8f, angle, 0.0f, 0.9f, 0.0f);
        add(MeshKind::Plane, ground, "opaque_dielectric", false, false, false);
        add(MeshKind::Cube, movingCube, "opaque_dielectric", false, false, false);
        add(MeshKind::Sphere, sphere, "opaque_dielectric", false, false, false);
        // Real skinned draw: a procedural 12-bone character with linear blend
        // skinning. Its pose advances with the frame index, so the moving
        // category covers skinned content instead of rigid props only.
        if (HasMaterialImpl("skinned_character")) {
            const XMMATRIX character = XMMatrixMultiply(
                XMMatrixMultiply(XMMatrixScaling(1.15f, 1.15f, 1.15f),
                                 XMMatrixRotationY(XMConvertToRadians(-18.0f))),
                XMMatrixTranslation(-2.0f, 0.0f, 1.4f));
            add(MeshKind::SkinnedCharacter, character, "skinned_character",
                false, false, false);
        }
    } else if (scene == "alpha_emissive") {
        if (!HasMaterialImpl("alpha_tested") || !HasMaterialImpl("emissive")) {
            *reason = "alpha_tested or emissive has no authored package";
            return false;
        }
        const XMMATRIX left =
            XMMatrixMultiply(XMMatrixScaling(2.0f, 2.0f, 1.0f),
                             XMMatrixTranslation(-1.4f, 1.4f, 0.2f));
        const XMMATRIX right =
            XMMatrixMultiply(XMMatrixScaling(2.0f, 2.0f, 1.0f),
                             XMMatrixTranslation(1.4f, 1.4f, 0.2f));
        add(MeshKind::Quad, left, "alpha_tested", true, false, false);
        add(MeshKind::Quad, right, "emissive", false, true, false);
        add(MeshKind::Plane, ground, "opaque_dielectric", false, false, false);
    } else if (scene == "transmissive_environment") {
        if (!HasMaterialImpl("transmissive_glass") ||
            !HasMaterialImpl("large_environment")) {
            *reason =
                "transmissive_glass or large_environment has no authored package";
            return false;
        }
        const XMMATRIX glass =
            XMMatrixMultiply(XMMatrixScaling(2.4f, 2.4f, 1.0f),
                             XMMatrixTranslation(0.0f, 1.5f, 0.0f));
        // An opaque marker behind the pane so refraction has background
        // structure to displace, not just a flat gradient. It sits at the
        // pane's centre so its silhouette crosses the refracted region.
        const XMMATRIX marker =
            MakeTransform(1.4f, XMConvertToRadians(20.0f), 0.05f, 1.30f, -2.2f);
        add(MeshKind::Plane, ground, "large_environment", false, false, false);
        add(MeshKind::Cube, marker, "opaque_dielectric", false, false, false);
        if (!options.skipTransmission) {
            add(MeshKind::Quad, glass, "transmissive_glass", false, false, true);
        }
    } else {
        *reason = "unknown scene '" + scene + "'";
        return false;
    }

    if (options.hud) {
        // In-world GUI: scene extent, depth-tested, follows the camera like
        // scene content. It is deliberately not the display overlay.
        detail::DrawItem gui;
        gui.mesh = MeshKind::Quad;
        gui.world = XMMatrixMultiply(XMMatrixScaling(1.7f, 1.0f, 1.0f),
                                     XMMatrixTranslation(-0.2f, 2.35f, 0.9f));
        gui.cell = "opaque_dielectric";
        gui.worldGui = true;
        if (HasMaterialImpl(gui.cell)) {
            draws->push_back(gui);
        }
    }

    if (draws->empty()) {
        *reason = "no authored material package is available for " + scene;
        return false;
    }
    return true;
}

void D3D12Harness::Impl::BuildConstants(const detail::DrawItem& draw,
                                        uint32_t frameIndex,
                                        detail::FrameConstants* out) const {
    *out = detail::FrameConstants{};
    const XMVECTOR eye = XMVectorSet(4.5f, 2.8f, 6.5f, 1.0f);
    const XMVECTOR target = XMVectorSet(0.0f, 0.8f, 0.0f, 1.0f);
    const XMMATRIX view =
        XMMatrixLookAtLH(eye, target, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const float aspect =
        static_cast<float>(renderWidth) / static_cast<float>(renderHeight);
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(45.0f), aspect, 0.05f, 100.0f);
    const XMMATRIX viewProj = XMMatrixMultiply(view, projection);

    const XMVECTOR lightDirection =
        XMVector3Normalize(XMVectorSet(0.45f, -1.0f, -0.30f, 0.0f));
    const XMVECTOR lightPosition =
        XMVectorSubtract(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
                         XMVectorScale(lightDirection, 15.0f));
    const XMMATRIX lightView =
        XMMatrixLookAtLH(lightPosition, XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
                         XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const XMMATRIX lightProjection =
        XMMatrixOrthographicLH(16.0f, 16.0f, 0.1f, 40.0f);
    const XMMATRIX lightViewProj = XMMatrixMultiply(lightView, lightProjection);

    XMStoreFloat4x4(&out->viewProj, viewProj);
    XMStoreFloat4x4(&out->lightViewProj, lightViewProj);
    XMStoreFloat4x4(&out->world, draw.world);
    XMStoreFloat4x4(&out->normalMatrix, draw.world);
    out->cameraPos = XMFLOAT4(4.5f, 2.8f, 6.5f, options.exposure);
    out->lightDir = XMFLOAT4(0.45f, -1.0f, -0.30f, 2.4f);
    out->lightColor = XMFLOAT4(1.0f, 0.93f, 0.82f, 1.0f / detail::kShadowSize);

    const detail::MaterialResources* material = nullptr;
    auto found = materialIndex.find(draw.cell);
    if (found != materialIndex.end()) {
        material = &materials[found->second];
    }
    float flags = 0.0f;
    if (material != nullptr && material->normalOpenGL) {
        flags += 1.0f;
    }
    if (material != nullptr && material->has[detail::kMetalness]) {
        flags += 2.0f;
    }
    if (material != nullptr && material->has[detail::kAo]) {
        flags += 4.0f;
    }
    if (material != nullptr && material->has[detail::kEmissive]) {
        flags += 8.0f;
    }
    if (draw.transmission) {
        flags += 16.0f;
    }
    out->material =
        XMFLOAT4(1.0f, material != nullptr ? material->metalnessDefault : 0.0f,
                 material != nullptr ? material->aoDefault : 1.0f, flags);
    out->sh0 = XMFLOAT4(0.38f, 0.43f, 0.52f, 0.07f);
    out->sh1 = XMFLOAT4(0.08f, 0.11f, 0.03f, 0.04f);
    out->sh2 = XMFLOAT4(0.05f, 0.04f, 0.035f, 0.03f);
    out->envZenith = XMFLOAT4(0.25f, 0.35f, 0.55f, 0.0f);
    out->envHorizon = XMFLOAT4(0.55f, 0.60f, 0.70f, 0.0f);
    out->envGround = XMFLOAT4(0.12f, 0.10f, 0.08f, 0.0f);
    out->fog = XMFLOAT4(0.012f, 0.15f, 2.8f, 0.0f);
    out->fogColor = XMFLOAT4(0.35f, 0.45f, 0.62f, 1.0f);
    out->params = XMFLOAT4(0.5f, draw.alphaTest ? 1.0f : 0.0f,
                           draw.transmission ? 1.0f : 0.0f,
                           static_cast<float>(frameIndex));
    out->viewport = XMFLOAT4(static_cast<float>(renderWidth),
                             static_cast<float>(renderHeight),
                             static_cast<float>(displayWidth),
                             static_cast<float>(displayHeight));
    float emissiveScale = 1.0f;
    bool emissiveApply = false;
    if (material != nullptr && material->has[detail::kEmissive]) {
        emissiveScale = material->emissiveScale;
        emissiveApply = true;
    }
    out->emissiveParams =
        XMFLOAT4(emissiveScale, emissiveApply ? 1.0f : 0.0f, 0.0f, 0.0f);
    out->transmissionParams =
        XMFLOAT4(1.5f, options.glassThickness,
                 draw.transmission ? 1.0f : 0.0f,
                 draw.transmission ? 1.0f : 0.0f);
    const XMFLOAT4X4 zeroPalette = {};
    for (uint32_t i = 0; i < detail::kMaxBones; ++i) {
        out->bones[i] = zeroPalette;
    }
    if (draw.skinned) {
        BuildCharacterSkinMatrices(static_cast<float>(frameIndex), out->bones,
                                   detail::kMaxBones);
    } else {
        XMStoreFloat4x4(&out->bones[0], XMMatrixIdentity());
    }
}

bool D3D12Harness::Impl::RenderFrame(
    const std::vector<detail::DrawItem>& draws, uint32_t frameIndex,
    std::string* error) {
    if (draws.empty()) {
        return detail::FailMessage(error, "no draws to render");
    }
    if (draws.size() > detail::kMaxDraws) {
        return detail::FailMessage(
            error, "too many draws for the constant-buffer ring");
    }
    const auto start = std::chrono::steady_clock::now();
    HRESULT hr = allocator->Reset();
    if (FAILED(hr)) {
        return detail::Fail("ID3D12CommandAllocator::Reset", hr, error);
    }
    hr = list->Reset(allocator.Get(), nullptr);
    if (FAILED(hr)) {
        return detail::Fail("ID3D12GraphicsCommandList::Reset", hr, error);
    }

    for (size_t i = 0; i < draws.size(); ++i) {
        detail::FrameConstants constants;
        BuildConstants(draws[i], frameIndex, &constants);
        uint8_t* slot = constantMapped + i * detail::kCbSlotBytes;
        std::memset(slot, 0, detail::kCbSlotBytes);
        std::memcpy(slot, &constants, sizeof(constants));
    }

    ID3D12DescriptorHeap* heaps[] = {srvHeap.Get()};
    list->SetDescriptorHeaps(1, heaps);

    D3D12_VIEWPORT shadowViewport{
        0.0f, 0.0f, static_cast<float>(detail::kShadowSize),
        static_cast<float>(detail::kShadowSize), 0.0f, 1.0f};
    D3D12_RECT shadowScissor{0, 0, static_cast<LONG>(detail::kShadowSize),
                             static_cast<LONG>(detail::kShadowSize)};
    D3D12_VIEWPORT sceneViewport{
        0.0f, 0.0f, static_cast<float>(renderWidth),
        static_cast<float>(renderHeight), 0.0f, 1.0f};
    D3D12_RECT sceneScissor{0, 0, static_cast<LONG>(renderWidth),
                            static_cast<LONG>(renderHeight)};
    D3D12_VIEWPORT displayViewport{
        0.0f, 0.0f, static_cast<float>(displayWidth),
        static_cast<float>(displayHeight), 0.0f, 1.0f};
    D3D12_RECT displayScissor{0, 0, static_cast<LONG>(displayWidth),
                              static_cast<LONG>(displayHeight)};

    // Shadow pass. The debug-pass switch isolates the tone-map path while the
    // capture GPU is being brought up.
    if (options.debugPass != 1 && options.debugPass != 6) {
        detail::Transition(list.Get(), shadowMap.Get(), shadowState,
                           D3D12_RESOURCE_STATE_DEPTH_WRITE);
        shadowState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        list->OMSetRenderTargets(0, nullptr, FALSE, &shadowDsv);
        if (options.debugPass != 5) {
            list->ClearDepthStencilView(shadowDsv, D3D12_CLEAR_FLAG_DEPTH,
                                        1.0f, 0, 0, nullptr);
        }
        list->SetPipelineState(shadowPso.Get());
        list->SetGraphicsRootSignature(shadowRoot.Get());
        list->RSSetViewports(1, &shadowViewport);
        list->RSSetScissorRects(1, &shadowScissor);
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        const size_t shadowCount =
            options.debugShadowCount < 0
                ? draws.size()
                : std::min(draws.size(),
                           static_cast<size_t>(options.debugShadowCount));
        for (size_t i = 0; i < shadowCount; ++i) {
            const detail::DrawItem& draw = draws[i];
            // Transmission is composited from the pre-glass scene and does not
            // occlude the key light; it is not an opaque shadow caster. The
            // in-world GUI is a scene overlay, not a physical caster. Both
            // policies are recorded with the captures (review-q4 N7).
            if (draw.transmission || draw.worldGui) {
                continue;
            }
            const detail::Mesh& mesh =
                meshes[static_cast<uint32_t>(draw.mesh)];
            const auto material = materialIndex.find(draw.cell);
            if (material == materialIndex.end()) {
                continue;
            }
            const detail::MaterialResources& resources =
                materials[material->second];
            list->SetGraphicsRootConstantBufferView(
                0, constantBuffer->GetGPUVirtualAddress() +
                       i * detail::kCbSlotBytes);
            // The caster's baseColor.alpha decides the cutout, so every draw
            // binds its own material table in the shadow pass too.
            list->SetGraphicsRootDescriptorTable(
                1, GpuHandle(resources.descriptorBase));
            list->IASetVertexBuffers(0, 1, &mesh.vertexView);
            list->IASetIndexBuffer(&mesh.indexView);
            list->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
        }
        // Unbind the depth view before the resource becomes a shader input.
        list->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
        if (options.debugPass != 7) {
            detail::Transition(list.Get(), shadowMap.Get(),
                               D3D12_RESOURCE_STATE_DEPTH_WRITE,
                               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            shadowState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }
    }

    // Main opaque pass.
    detail::Transition(list.Get(), sceneColor.Get(), sceneState,
                       D3D12_RESOURCE_STATE_RENDER_TARGET);
    sceneState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    const FLOAT hdrClear[4] = {0.012f, 0.014f, 0.020f, 1.0f};
    list->OMSetRenderTargets(1, &sceneRtv, FALSE, &mainDsv);
    list->ClearRenderTargetView(sceneRtv, hdrClear, 0, nullptr);
    list->ClearDepthStencilView(mainDsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0,
                                nullptr);
    list->SetPipelineState(mainPso.Get());
    list->SetGraphicsRootSignature(mainRoot.Get());
    list->SetGraphicsRootDescriptorTable(2, shadowSrv);
    list->RSSetViewports(1, &sceneViewport);
    list->RSSetScissorRects(1, &sceneScissor);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    if (options.debugPass == 0 || options.debugPass == 4) {
        const size_t mainCount =
            options.debugMainCount < 0
                ? draws.size()
                : std::min(draws.size(),
                           static_cast<size_t>(options.debugMainCount));
        for (size_t i = 0; i < mainCount; ++i) {
            const detail::DrawItem& draw = draws[i];
            if (draw.transmission) {
                continue;
            }
            const detail::Mesh& mesh =
                meshes[static_cast<uint32_t>(draw.mesh)];
            const auto material = materialIndex.find(draw.cell);
            if (material == materialIndex.end()) {
                continue;
            }
            const detail::MaterialResources& resources =
                materials[material->second];
            list->SetPipelineState(draw.worldGui ? worldGuiPso.Get()
                                                 : mainPso.Get());
            list->SetGraphicsRootConstantBufferView(
                0, constantBuffer->GetGPUVirtualAddress() +
                       i * detail::kCbSlotBytes);
            list->SetGraphicsRootDescriptorTable(
                1, GpuHandle(resources.descriptorBase));
            list->IASetVertexBuffers(0, 1, &mesh.vertexView);
            list->IASetIndexBuffer(&mesh.indexView);
            list->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
        }
    }
    list->OMSetRenderTargets(0, nullptr, FALSE, nullptr);

    // Copy the pre-transmission scene so the glass pass can refract it. The
    // glass is composited after every opaque draw and before reconstruction,
    // so it never samples the display HUD.
    bool hasTransmission = false;
    for (const detail::DrawItem& draw : draws) {
        hasTransmission = hasTransmission || draw.transmission;
    }
    if (hasTransmission && sceneCopy) {
        detail::Transition(list.Get(), sceneColor.Get(), sceneState,
                           D3D12_RESOURCE_STATE_COPY_SOURCE);
        sceneState = D3D12_RESOURCE_STATE_COPY_SOURCE;
        detail::Transition(list.Get(), sceneCopy.Get(), sceneCopyState,
                           D3D12_RESOURCE_STATE_COPY_DEST);
        sceneCopyState = D3D12_RESOURCE_STATE_COPY_DEST;
        list->CopyResource(sceneCopy.Get(), sceneColor.Get());
        detail::Transition(list.Get(), sceneCopy.Get(),
                           D3D12_RESOURCE_STATE_COPY_DEST,
                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        sceneCopyState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        detail::Transition(list.Get(), sceneColor.Get(),
                           D3D12_RESOURCE_STATE_COPY_SOURCE,
                           D3D12_RESOURCE_STATE_RENDER_TARGET);
        sceneState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    // Transmissive pass: screen-space refraction of the pre-glass scene. It
    // writes an opaque composite (transmission * tint + Fresnel reflection), so
    // no blend state or depth write is needed and no sort order is implied.
    if (hasTransmission && sceneCopy) {
        list->OMSetRenderTargets(1, &sceneRtv, FALSE, &mainDsv);
        list->SetPipelineState(glassPso.Get());
        list->SetGraphicsRootSignature(mainRoot.Get());
        list->SetGraphicsRootDescriptorTable(3, sceneCopySrv);
        list->RSSetViewports(1, &sceneViewport);
        list->RSSetScissorRects(1, &sceneScissor);
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        for (size_t i = 0; i < draws.size(); ++i) {
            const detail::DrawItem& draw = draws[i];
            if (!draw.transmission) {
                continue;
            }
            const detail::Mesh& mesh =
                meshes[static_cast<uint32_t>(draw.mesh)];
            const auto material = materialIndex.find(draw.cell);
            if (material == materialIndex.end()) {
                continue;
            }
            const detail::MaterialResources& resources =
                materials[material->second];
            list->SetGraphicsRootConstantBufferView(
                0, constantBuffer->GetGPUVirtualAddress() +
                       i * detail::kCbSlotBytes);
            list->SetGraphicsRootDescriptorTable(
                1, GpuHandle(resources.descriptorBase));
            list->IASetVertexBuffers(0, 1, &mesh.vertexView);
            list->IASetIndexBuffer(&mesh.indexView);
            list->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
        }
        list->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
    }
    if (options.hdrProbe && hdrReadback) {
        detail::Transition(list.Get(), sceneColor.Get(), sceneState,
                           D3D12_RESOURCE_STATE_COPY_SOURCE);
        sceneState = D3D12_RESOURCE_STATE_COPY_SOURCE;
        D3D12_TEXTURE_COPY_LOCATION destination{};
        destination.pResource = hdrReadback.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint = hdrFootprint;
        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = sceneColor.Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        source.SubresourceIndex = 0;
        list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        detail::Transition(list.Get(), sceneColor.Get(),
                           D3D12_RESOURCE_STATE_COPY_SOURCE,
                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        sceneState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    } else {
        detail::Transition(list.Get(), sceneColor.Get(), sceneState,
                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        sceneState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }

    // Reconstruction + tone map to the display target.
    detail::Transition(list.Get(), display.Get(), displayState,
                       D3D12_RESOURCE_STATE_RENDER_TARGET);
    displayState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    const FLOAT displayClear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    list->OMSetRenderTargets(1, &displayRtv, FALSE, nullptr);
    list->ClearRenderTargetView(displayRtv, displayClear, 0, nullptr);
    list->SetPipelineState(tonemapPso.Get());
    list->SetGraphicsRootSignature(tonemapRoot.Get());
    const float tonemapConstants[4] = {options.exposure, 0.0f, 0.0f, 0.0f};
    list->SetGraphicsRoot32BitConstants(0, 4, tonemapConstants, 0);
    list->SetGraphicsRootDescriptorTable(1, sceneSrv);
    list->RSSetViewports(1, &displayViewport);
    list->RSSetScissorRects(1, &displayScissor);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(3, 1, 0, 0);

    // Display-resolution HUD: composed after reconstruction and tone mapping,
    // at the display extent, in integer pixel space. It does not sample the
    // scene, is not fogged, is not upscaled and is not part of any history.
    if (options.hud && hudPso) {
        list->SetPipelineState(hudPso.Get());
        list->SetGraphicsRootSignature(hudRoot.Get());
        const float hudConstants[4] = {
            static_cast<float>(displayWidth), static_cast<float>(displayHeight),
            static_cast<float>(frameIndex), 1.0f};
        list->SetGraphicsRoot32BitConstants(0, 4, hudConstants, 0);
        list->RSSetViewports(1, &displayViewport);
        list->RSSetScissorRects(1, &displayScissor);
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        list->DrawInstanced(3, 1, 0, 0);
    }
    list->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
    detail::Transition(list.Get(), display.Get(),
                       D3D12_RESOURCE_STATE_RENDER_TARGET,
                       D3D12_RESOURCE_STATE_COPY_SOURCE);
    displayState = D3D12_RESOURCE_STATE_COPY_SOURCE;

    if (!ExecuteAndWait(error)) {
        return false;
    }
    const auto end = std::chrono::steady_clock::now();
    lastFrameMs = std::chrono::duration<float, std::milli>(end - start).count();
    std::printf("FRAME %u: %.2f ms\n", frameIndex, lastFrameMs);

    if (options.hdrProbe && hdrReadback) {
        void* mapped = nullptr;
        if (SUCCEEDED(hdrReadback->Map(0, nullptr, &mapped)) &&
            mapped != nullptr) {
            const uint16_t* base = static_cast<const uint16_t*>(mapped);
            const size_t rowWords = hdrFootprint.Footprint.RowPitch / 2;
            float peak = 0.0f;
            double sum = 0.0;
            uint64_t aboveOne = 0;
            uint64_t aboveEight = 0;
            uint64_t aboveThirtyTwo = 0;
            uint64_t count = 0;
            for (uint32_t y = 0; y < renderHeight; ++y) {
                const uint16_t* row = base + static_cast<size_t>(y) * rowWords;
                for (uint32_t x = 0; x < renderWidth; ++x) {
                    const uint16_t* pixel = row + static_cast<size_t>(x) * 4;
                    const float r = HalfToFloat(pixel[0]);
                    const float g = HalfToFloat(pixel[1]);
                    const float b = HalfToFloat(pixel[2]);
                    const float m = std::max({r, g, b});
                    peak = std::max(peak, m);
                    sum += m;
                    if (r > 1.0f || g > 1.0f || b > 1.0f) {
                        ++aboveOne;
                    }
                    if (r > 8.0f || g > 8.0f || b > 8.0f) {
                        ++aboveEight;
                    }
                    if (r > 32.0f || g > 32.0f || b > 32.0f) {
                        ++aboveThirtyTwo;
                    }
                    ++count;
                }
            }
            hdrReadback->Unmap(0, nullptr);
            std::printf(
                "Q4 hdr probe: peak=%.4f mean=%.4f pixels_gt_1=%llu "
                "pixels_gt_8=%llu pixels_gt_32=%llu of %llu\n",
                static_cast<double>(peak),
                count == 0 ? 0.0 : sum / static_cast<double>(count),
                static_cast<unsigned long long>(aboveOne),
                static_cast<unsigned long long>(aboveEight),
                static_cast<unsigned long long>(aboveThirtyTwo),
                static_cast<unsigned long long>(count));
        }
    }
    return true;
}

}  // namespace q4
