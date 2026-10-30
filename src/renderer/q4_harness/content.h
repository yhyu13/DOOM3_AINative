#pragma once

// Q4 capture-harness content catalog.
//
// Reads the shipped Q4 material set (content/manifest/q4-materials.json), each
// material sidecar, and the built package manifest (out/content/<material>/
// package.json). Nothing is invented: a cell the manifest does not list, or a
// channel the sidecar marks unavailable, is reported as absent and the caller
// must not render a frame for it.

#include <cstdint>
#include <string>
#include <vector>

namespace q4 {

struct ChannelInfo {
    std::string state;       // authored / derived:<method> / unavailable / absent
    std::string path;        // source path for authored or derived channels
    std::string format;      // declared package format
    std::string handedness;  // declared normal handedness (opengl / directx)
    bool srgb = false;
    bool available = false;  // authored or derived
};

struct MaterialCell {
    std::string material;
    std::string cell;
    std::string mtr;
    std::string sidecar;
    ChannelInfo baseColor;
    ChannelInfo normal;
    ChannelInfo roughness;
    ChannelInfo metalness;
    ChannelInfo ao;
    ChannelInfo emissive;
    bool present = false;   // listed in q4-materials.json
    bool authored = false;  // enough declared channels for the hybrid opaque route
    bool declaredDielectric = false;
    float emissiveScale = 1.0f;   // sidecar emissiveScale, applied at shading
    std::string hdrState;         // sidecar hdr.state, recorded not inferred
    std::string notRunReason;
};

struct PackageTexture {
    std::string channel;
    std::string file;
    std::string format;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mips = 0;
    bool srgb = false;
};

struct MaterialPackage {
    std::string material;
    std::string dir;
    std::string packageSha256;
    std::vector<PackageTexture> textures;
    bool present = false;
    std::string error;

    const PackageTexture* Find(const std::string& channel) const;
};

class ContentCatalog {
public:
    // `repoRoot` is the directory that holds `content/`. Returns false with an
    // actionable error when the manifest cannot be read or parsed.
    bool Load(const std::string& repoRoot, std::string* error);

    const std::string& repoRoot() const { return repoRoot_; }
    const std::vector<MaterialCell>& cells() const { return cells_; }
    const MaterialCell* FindCell(const std::string& cell) const;
    const MaterialCell* FindMaterial(const std::string& material) const;

    // Reads `packageDir/<material>/package.json`. Returns false when the
    // package is absent or malformed; `package->present` stays false.
    bool LoadPackage(const std::string& packageDir, const std::string& material,
                     MaterialPackage* package, std::string* error) const;

private:
    std::string repoRoot_;
    std::vector<MaterialCell> cells_;
};

// Resolves the repository root that owns `content/manifest/q4-materials.json`.
// `contentDir` is an optional explicit `content` directory; otherwise the
// current directory and then the executable's ancestors are searched.
std::string FindRepoRoot(const std::string& contentDir, std::string* error);

// Reads the executable directory (no trailing separator).
std::string ExecutableDirectory();

}  // namespace q4
