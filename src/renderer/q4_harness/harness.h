#pragma once

// Standalone Q4 D3D12 capture harness.
//
// The public RHI cannot yet express vertex/index buffers or texture binding, so
// this harness renders with native D3D12 on the device the RHI selected. The
// Q4 portable slices are still used for the material route, direct/indirect
// arithmetic, residency accounting and reconstruction planning; their results
// are logged as hooks, and the shader implements the per-pixel form.

#include "content.h"

#include <cstdint>
#include <memory>
#include <string>

struct ID3D12Device;

namespace q4 {

struct HarnessOptions {
    bool warp = false;
    uint32_t adapterIndex = 0;
    uint32_t width = 1280;
    uint32_t height = 720;
    float renderScale = 1.0f;
    float exposure = 1.0f;
    float emissiveScale = -1.0f;  // < 0 = use the sidecar's emissiveScale
    bool hud = false;             // in-world GUI + display-resolution HUD
    bool hdrProbe = false;        // read back the HDR scene and print the peak
    bool skipTransmission = false;  // reference frame without the glass draw
    bool captureExact = false;      // write -Capture verbatim, no scene suffix
    bool vramProbe = false;         // sample IDXGIAdapter3 video memory per frame
    float glassThickness = 0.35f;   // thin-slab refraction thickness, world units
    uint32_t frames = 1;
    int debugPass = 0;  // 0 = all, 1 = tone-map only, 2 = shadow + tone-map
    int debugShadowCount = -1;  // -1 = all shadow draws
    int debugMainCount = -1;    // -1 = all main draws
    std::string debugShaderEntry = "PSMain";
    std::string scene = "static_opaque";
    std::string material = "hero_dielectric";
    std::string capturePath;
    std::string repoRoot;
    std::string packageDir;
    std::string shaderDir;
};

struct RuntimeHooks {
    std::string materialPlan;
    std::string directLight;
    std::string indirect;
    std::string residency;
    std::string reconstruction;
    std::string shadow;
    std::string emissive;
    std::string skinning;
};

class D3D12Harness {
public:
    D3D12Harness();
    ~D3D12Harness();

    D3D12Harness(const D3D12Harness&) = delete;
    D3D12Harness& operator=(const D3D12Harness&) = delete;

    // `device` is the RHI-selected ID3D12Device. The harness creates its own
    // command queue, fence and resources on it.
    bool Initialize(ID3D12Device* device, const HarnessOptions& options,
                    std::string* error);

    // Loads every catalog cell that has a built package. A missing package is
    // recorded, not replaced by a synthetic material.
    bool LoadMaterials(const ContentCatalog& catalog, std::string* error);

    // Renders one frame of `scene` into the HDR target and resolves it to the
    // display target. Returns false with a reason when the scene's authored
    // cells are not available.
    bool RenderScene(const ContentCatalog& catalog, const std::string& scene,
                     uint32_t frameIndex, std::string* error);

    // Reads the display target back and writes it as PNG.
    bool WriteCapture(const std::string& path, std::string* error);

    void Shutdown();

    const RuntimeHooks& hooks() const;
    uint32_t renderWidth() const;
    uint32_t renderHeight() const;
    uint32_t displayWidth() const;
    uint32_t displayHeight() const;
    const std::string& reconstructionMode() const;
    bool HasMaterial(const std::string& cell) const;

    // True only when -VramProbe was requested and the DXGI probe initialized.
    bool vramAvailable() const;
    // Identity, every sample and the LOCAL/NON_LOCAL peaks as one JSON object.
    std::string vramJson() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace q4
