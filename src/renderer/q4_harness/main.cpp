#include "content.h"
#include "harness.h"

#include "d3d12/d3d12_private.h"
#include "rhi/rhi.h"

#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {

namespace fs = std::filesystem;

struct AdapterEntry {
    std::string name;
    uint32_t vendorId = 0;
    uint32_t deviceId = 0;
    uint64_t memory = 0;
    bool software = false;
};

std::string Narrow(const wchar_t* value) {
    if (value == nullptr || *value == L'\0') {
        return {};
    }
    const int length =
        WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) {
        return {};
    }
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, &result[0], length, nullptr,
                        nullptr);
    result.resize(static_cast<size_t>(length - 1));
    return result;
}

std::vector<AdapterEntry> EnumerateAdapters(bool softwareClass) {
    std::vector<AdapterEntry> result;
    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        return result;
    }
    auto add = [&](IDXGIAdapter1* adapter, bool software) {
        DXGI_ADAPTER_DESC1 description{};
        if (adapter == nullptr || FAILED(adapter->GetDesc1(&description))) {
            return;
        }
        AdapterEntry entry;
        entry.name = Narrow(description.Description);
        entry.vendorId = description.VendorId;
        entry.deviceId = description.DeviceId;
        entry.memory = static_cast<uint64_t>(description.DedicatedVideoMemory);
        entry.software = software;
        result.push_back(std::move(entry));
    };

    if (!softwareClass) {
        for (UINT index = 0;; ++index) {
            ComPtr<IDXGIAdapter1> adapter;
            const HRESULT hr = factory->EnumAdapters1(index, &adapter);
            if (hr == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            if (FAILED(hr)) {
                break;
            }
            DXGI_ADAPTER_DESC1 description{};
            if (FAILED(adapter->GetDesc1(&description))) {
                continue;
            }
            if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
                continue;
            }
            add(adapter.Get(), false);
        }
        return result;
    }

    ComPtr<IDXGIFactory4> factory4;
    if (SUCCEEDED(factory.As(&factory4)) && factory4) {
        ComPtr<IDXGIAdapter1> warp;
        if (SUCCEEDED(factory4->EnumWarpAdapter(IID_PPV_ARGS(&warp))) && warp) {
            add(warp.Get(), true);
        }
    }
    for (UINT index = 0;; ++index) {
        ComPtr<IDXGIAdapter1> adapter;
        const HRESULT hr = factory->EnumAdapters1(index, &adapter);
        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (FAILED(hr)) {
            break;
        }
        DXGI_ADAPTER_DESC1 description{};
        if (FAILED(adapter->GetDesc1(&description))) {
            continue;
        }
        if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
            continue;
        }
        bool duplicate = false;
        for (const AdapterEntry& existing : result) {
            if (existing.vendorId == description.VendorId &&
                existing.deviceId == description.DeviceId &&
                existing.memory == static_cast<uint64_t>(
                                       description.DedicatedVideoMemory)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            add(adapter.Get(), true);
        }
    }
    return result;
}

void PrintAdapters(std::vector<AdapterEntry>* hardware,
                   std::vector<AdapterEntry>* software) {
    *hardware = EnumerateAdapters(false);
    *software = EnumerateAdapters(true);
    std::printf("Hardware adapters:\n");
    if (hardware->empty()) {
        std::printf("  (none)\n");
    }
    for (size_t i = 0; i < hardware->size(); ++i) {
        const AdapterEntry& entry = (*hardware)[i];
        std::printf("  [%zu] %s (vendor=0x%04X device=0x%04X memory=%llu)\n", i,
                    entry.name.c_str(), entry.vendorId, entry.deviceId,
                    static_cast<unsigned long long>(entry.memory));
    }
    std::printf("Software adapters:\n");
    if (software->empty()) {
        std::printf("  (none)\n");
    }
    for (size_t i = 0; i < software->size(); ++i) {
        const AdapterEntry& entry = (*software)[i];
        std::printf("  [%zu] %s (vendor=0x%04X device=0x%04X memory=%llu)\n", i,
                    entry.name.c_str(), entry.vendorId, entry.deviceId,
                    static_cast<unsigned long long>(entry.memory));
    }
}

struct SceneSpec {
    const char* id;
    std::vector<const char*> cells;
    bool requireAll;
};

const SceneSpec kScenes[] = {
    {"static_opaque", {"opaque_dielectric", "opaque_metal"}, false},
    {"alpha_emissive", {"alpha_tested", "emissive"}, true},
    {"moving", {"opaque_dielectric", "skinned_character"}, false},
    {"transmissive_environment", {"transmissive_glass", "large_environment"},
     true},
};

const SceneSpec* FindScene(const std::string& id) {
    for (const SceneSpec& scene : kScenes) {
        if (id == scene.id) {
            return &scene;
        }
    }
    return nullptr;
}

std::string CellReason(const q4::ContentCatalog& catalog,
                       const q4::D3D12Harness& harness, const char* cell) {
    const q4::MaterialCell* entry = catalog.FindCell(cell);
    if (entry == nullptr) {
        return "not in content/manifest/q4-materials.json";
    }
    if (!entry->baseColor.available) {
        return entry->notRunReason.empty() ? "no authored base colour"
                                           : entry->notRunReason;
    }
    if (!harness.HasMaterial(cell)) {
        return "package not built (run content/tools/build-material.ps1)";
    }
    return {};
}

std::string CapturePathFor(const std::string& base, const std::string& scene) {
    if (scene == "static_opaque") {
        return base;
    }
    const fs::path path(base);
    const fs::path parent = path.parent_path();
    const std::string stem = path.stem().string();
    const std::string extension = path.extension().string();
    const std::string name = stem + "-" + scene + extension;
    return (parent / name).string();
}

void PrintDiagnostics(doom3::rhi::Device& device) {
    const doom3::rhi::d3d12::DiagnosticReport report =
        doom3::rhi::d3d12::ReadDiagnostics(device);
    std::printf("D3D12 debug layer: %s, messages=%u errors=%u warnings=%u\n",
                doom3::rhi::d3d12::ToString(report.debugLayer),
                report.messageCount, report.errorCount, report.warningCount);
    for (const std::string& message : report.messages) {
        std::printf("  %s\n", message.c_str());
    }
}

void Usage() {
    std::printf(
        "Usage: q4_harness -Scene <id> -Capture <png> [options]\n"
        "  -Scene static_opaque|alpha_emissive|moving|transmissive_environment|all\n"
        "  -Capture <path>        PNG output path\n"
        "  -Material <name>       Q4 material identity for the hook log\n"
        "  -Warp                  select the software (WARP) adapter\n"
        "  -AdapterIndex <n>      index within the selected adapter class\n"
        "  -ListAdapters          print adapter indices and exit\n"
        "  -Width/-Height <n>     display extent (default 1280x720)\n"
        "  -RenderScale <f>       render-extent scale (default 1.0)\n"
        "  -Frames <n>            frames to render before capture (default 1)\n"
        "  -Exposure <f>          tone-map exposure (default 1.0)\n"
        "  -ContentDir <path>     directory that holds content/\n"
        "  -PackageDir <path>     directory that holds <material>/package.json\n"
        "  -ShaderDir <path>      directory that holds q4_hybrid.hlsl\n");
}

}  // namespace

int main(int argc, char** argv) {
    q4::HarnessOptions options;
    bool listAdapters = false;
    bool debugLayer = false;
    std::string contentDir;
    std::string packageDir;
    std::string sceneId = "static_opaque";
    std::string capturePath;
    std::string material = "hero_dielectric";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&](const char* name) -> std::string {
            if (i + 1 < argc) {
                return argv[++i];
            }
            std::fprintf(stderr, "ERROR: %s needs a value\n", name);
            return {};
        };
        if (arg == "+set") {
            if (i + 2 < argc) {
                i += 2;
            }
            continue;
        }
        if (arg == "-Capture") {
            capturePath = value("-Capture");
        } else if (arg == "-Scene") {
            sceneId = value("-Scene");
        } else if (arg == "-Material") {
            material = value("-Material");
        } else if (arg == "-Warp") {
            options.warp = true;
        } else if (arg == "-AdapterIndex") {
            options.adapterIndex =
                static_cast<uint32_t>(std::strtoul(value("-AdapterIndex").c_str(),
                                                   nullptr, 10));
        } else if (arg == "-ListAdapters") {
            listAdapters = true;
        } else if (arg == "-Debug") {
            debugLayer = true;
        } else if (arg == "-DebugPass") {
            options.debugPass = std::atoi(value("-DebugPass").c_str());
        } else if (arg == "-DebugShadowCount") {
            options.debugShadowCount =
                std::atoi(value("-DebugShadowCount").c_str());
        } else if (arg == "-DebugMainCount") {
            options.debugMainCount =
                std::atoi(value("-DebugMainCount").c_str());
        } else if (arg == "-DebugShader") {
            options.debugShaderEntry = value("-DebugShader");
        } else if (arg == "-Width") {
            options.width =
                static_cast<uint32_t>(std::strtoul(value("-Width").c_str(),
                                                   nullptr, 10));
        } else if (arg == "-Height") {
            options.height =
                static_cast<uint32_t>(std::strtoul(value("-Height").c_str(),
                                                   nullptr, 10));
        } else if (arg == "-RenderScale") {
            options.renderScale =
                static_cast<float>(std::atof(value("-RenderScale").c_str()));
        } else if (arg == "-Frames") {
            options.frames =
                static_cast<uint32_t>(std::strtoul(value("-Frames").c_str(),
                                                   nullptr, 10));
        } else if (arg == "-Exposure") {
            options.exposure =
                static_cast<float>(std::atof(value("-Exposure").c_str()));
        } else if (arg == "-ContentDir") {
            contentDir = value("-ContentDir");
        } else if (arg == "-PackageDir") {
            packageDir = value("-PackageDir");
        } else if (arg == "-ShaderDir") {
            options.shaderDir = value("-ShaderDir");
        }
    }

    if (listAdapters) {
        std::vector<AdapterEntry> hardware;
        std::vector<AdapterEntry> software;
        PrintAdapters(&hardware, &software);
        return 0;
    }

    if (capturePath.empty()) {
        Usage();
        std::fprintf(stderr, "ERROR: -Capture is required\n");
        return 2;
    }
    if (options.warp) {
        options.adapterIndex = std::max<uint32_t>(0, options.adapterIndex);
    }

    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE) {
        std::fprintf(stderr, "ERROR: CoInitializeEx failed (hr=0x%08lX)\n",
                     static_cast<unsigned long>(static_cast<uint32_t>(com)));
        return 2;
    }

    std::string error;
    options.repoRoot = q4::FindRepoRoot(contentDir, &error);
    if (options.repoRoot.empty()) {
        std::fprintf(stderr, "ERROR: %s\n", error.c_str());
        return 2;
    }
    options.packageDir =
        packageDir.empty()
            ? (fs::path(options.repoRoot) / "out" / "content").string()
            : packageDir;
    options.material = material;
    options.scene = sceneId;
    options.capturePath = capturePath;

    q4::ContentCatalog catalog;
    if (!catalog.Load(options.repoRoot, &error)) {
        std::fprintf(stderr, "ERROR: content catalog: %s\n", error.c_str());
        return 2;
    }

    doom3::rhi::DeviceOptions deviceOptions;
    deviceOptions.adapterClass = options.warp
                                     ? doom3::rhi::AdapterClass::Software
                                     : doom3::rhi::AdapterClass::Hardware;
    deviceOptions.adapterIndex = options.adapterIndex;
    doom3::rhi::Device device;
    doom3::rhi::Status deviceStatus = doom3::rhi::Status::Ok;
    if (debugLayer) {
        doom3::rhi::d3d12::DiagnosticRequest request{true, false};
        deviceStatus = doom3::rhi::d3d12::InitializeWithDiagnostics(
            device, deviceOptions, request, &error);
    } else {
        deviceStatus = device.Initialize(deviceOptions, &error);
    }
    if (deviceStatus != doom3::rhi::Status::Ok) {
        std::fprintf(stderr,
                     "ERROR: RHI initialize failed: %s\n"
                     "       use -ListAdapters, -AdapterIndex or -Warp\n",
                     error.c_str());
        return 4;
    }
    const doom3::rhi::AdapterInfo& adapter = device.adapter();
    std::printf("RHI backend: d3d12 (%s)\n", adapter.name.c_str());
    const bool hasVendor =
        (adapter.reportedFacts &
         static_cast<uint32_t>(doom3::rhi::AdapterFact::VendorId)) != 0;
    const bool hasDevice =
        (adapter.reportedFacts &
         static_cast<uint32_t>(doom3::rhi::AdapterFact::DeviceId)) != 0;
    const bool hasMemory =
        (adapter.reportedFacts &
         static_cast<uint32_t>(doom3::rhi::AdapterFact::DedicatedMemory)) != 0;
    std::printf("RHI adapter: class=%s vendor=%s device=%s memory=%s\n",
                doom3::rhi::ToString(adapter.adapterClass),
                hasVendor ? "reported" : "unreported",
                hasDevice ? "reported" : "unreported",
                hasMemory ? "reported" : "unreported");

    ID3D12Device* nativeDevice =
        doom3::rhi::BackendAccess::NativeDevice(device);
    if (nativeDevice == nullptr) {
        std::fprintf(stderr, "ERROR: the RHI did not expose a D3D12 device\n");
        device.Shutdown();
        return 4;
    }

    q4::D3D12Harness harness;
    if (!harness.Initialize(nativeDevice, options, &error)) {
        std::fprintf(stderr, "ERROR: harness initialize: %s\n", error.c_str());
        device.Shutdown();
        return 4;
    }
    if (!harness.LoadMaterials(catalog, &error)) {
        std::fprintf(stderr, "ERROR: material load: %s\n", error.c_str());
        harness.Shutdown();
        device.Shutdown();
        return 4;
    }

    std::printf("Q4 material: %s\n", harness.hooks().materialPlan.c_str());
    std::printf("Q4 direct reference: %s\n", harness.hooks().directLight.c_str());
    std::printf("Q4 indirect reference: %s\n", harness.hooks().indirect.c_str());
    std::printf("Q4 residency: %s\n", harness.hooks().residency.c_str());
    std::printf("Q4 reconstruction: %s\n", harness.reconstructionMode().c_str());
    std::printf("Q4 shadow: %s\n", harness.hooks().shadow.c_str());

    // Report every rubric category and the cells it needs. A cell without an
    // authored package is NOT RUN; it is never replaced by a stand-in frame.
    for (const SceneSpec& spec : kScenes) {
        bool any = false;
        bool all = true;
        std::string missing;
        for (const char* cell : spec.cells) {
            const std::string reason = CellReason(catalog, harness, cell);
            if (reason.empty()) {
                any = true;
                std::printf("  CELL %s: RUN\n", cell);
            } else {
                all = false;
                std::printf("  CELL %s: NOT RUN (%s)\n", cell, reason.c_str());
                if (!missing.empty()) {
                    missing += ", ";
                }
                missing += cell;
            }
        }
        const bool runs = spec.requireAll ? all : any;
        if (!runs) {
            std::printf("SCENE %s: NOT RUN (missing %s)\n", spec.id,
                        missing.c_str());
        } else if (!all) {
            std::printf("SCENE %s: PARTIAL (missing %s)\n", spec.id,
                        missing.c_str());
        } else {
            std::printf("SCENE %s: RUN\n", spec.id);
        }
    }

    std::vector<std::string> scenesToRun;
    if (sceneId == "all") {
        for (const SceneSpec& spec : kScenes) {
            bool any = false;
            bool all = true;
            for (const char* cell : spec.cells) {
                if (CellReason(catalog, harness, cell).empty()) {
                    any = true;
                } else {
                    all = false;
                }
            }
            if (spec.requireAll ? all : any) {
                scenesToRun.push_back(spec.id);
            }
        }
        if (scenesToRun.empty()) {
            std::fprintf(stderr, "ERROR: no scene category has authored content\n");
            harness.Shutdown();
            device.Shutdown();
            return 3;
        }
    } else {
        const SceneSpec* spec = FindScene(sceneId);
        if (spec == nullptr) {
            std::fprintf(stderr, "ERROR: unknown scene '%s'\n", sceneId.c_str());
            Usage();
            harness.Shutdown();
            device.Shutdown();
            return 2;
        }
        bool any = false;
        bool all = true;
        for (const char* cell : spec->cells) {
            if (CellReason(catalog, harness, cell).empty()) {
                any = true;
            } else {
                all = false;
            }
        }
        if (!(spec->requireAll ? all : any)) {
            std::fprintf(stderr, "NOT RUN: %s has no authored material\n",
                         sceneId.c_str());
            harness.Shutdown();
            device.Shutdown();
            return 3;
        }
        scenesToRun.push_back(sceneId);
    }

    for (const std::string& scene : scenesToRun) {
        if (!harness.RenderScene(catalog, scene, 0, &error)) {
            std::fprintf(stderr, "ERROR: render %s: %s\n", scene.c_str(),
                         error.c_str());
            if (debugLayer) {
                PrintDiagnostics(device);
            }
            harness.Shutdown();
            device.Shutdown();
            return 4;
        }
        if (debugLayer) {
            PrintDiagnostics(device);
        }
        const std::string output = CapturePathFor(capturePath, scene);
        if (!harness.WriteCapture(output, &error)) {
            std::fprintf(stderr, "ERROR: capture %s: %s\n", scene.c_str(),
                         error.c_str());
            if (debugLayer) {
                PrintDiagnostics(device);
            }
            harness.Shutdown();
            device.Shutdown();
            return 5;
        }
    }

    harness.Shutdown();
    device.Shutdown();
    if (com == S_OK) {
        CoUninitialize();
    }
    return 0;
}
