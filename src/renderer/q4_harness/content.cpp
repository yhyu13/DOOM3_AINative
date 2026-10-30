#include "content.h"

#include "shader/shader_json.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace q4 {
namespace {

namespace fs = std::filesystem;
namespace json = d3::shader::json;

bool ReadTextFile(const fs::path& path, std::string* text, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error != nullptr) {
            *error = "cannot open " + path.string();
        }
        return false;
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    *text = stream.str();
    return true;
}

bool ParseFile(const fs::path& path, json::Value* value, std::string* error) {
    std::string text;
    if (!ReadTextFile(path, &text, error)) {
        return false;
    }
    std::string parseError;
    if (!json::Parse(text, *value, parseError)) {
        if (error != nullptr) {
            *error = path.string() + ": " + parseError;
        }
        return false;
    }
    return true;
}

std::string StringOr(const json::Value& object, const char* key, const char* fallback = "") {
    const json::Value* value = object.Find(key);
    return (value != nullptr && value->IsString()) ? value->string : fallback;
}

bool BoolOr(const json::Value& object, const char* key, bool fallback = false) {
    const json::Value* value = object.Find(key);
    return (value != nullptr && value->IsBool()) ? value->boolean : fallback;
}

double NumberOr(const json::Value& object, const char* key, double fallback = 0.0) {
    const json::Value* value = object.Find(key);
    return (value != nullptr && value->IsNumber()) ? value->number : fallback;
}

void ReadChannel(const json::Value& channels, const char* key, ChannelInfo* out) {
    *out = ChannelInfo{};
    const json::Value* channel = channels.Find(key);
    if (channel == nullptr || !channel->IsObject()) {
        out->state = "absent";
        return;
    }
    out->state = StringOr(*channel, "state", "absent");
    out->path = StringOr(*channel, "path");
    out->format = StringOr(*channel, "format");
    out->handedness = StringOr(*channel, "handedness");
    out->srgb = BoolOr(*channel, "srgb", false);
    out->available = out->state == "authored" || out->state.rfind("derived:", 0) == 0;
}

bool HasManifest(const fs::path& root) {
    return fs::exists(root / "content" / "manifest" / "q4-materials.json");
}

}  // namespace

const PackageTexture* MaterialPackage::Find(const std::string& channel) const {
    for (const PackageTexture& texture : textures) {
        if (texture.channel == channel) {
            return &texture;
        }
    }
    return nullptr;
}

std::string ExecutableDirectory() {
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return {};
    }
    return fs::path(buffer).parent_path().string();
}

std::string FindRepoRoot(const std::string& contentDir, std::string* error) {
    if (!contentDir.empty()) {
        fs::path content = fs::absolute(contentDir);
        fs::path root = content.filename() == "content" ? content.parent_path() : content;
        if (HasManifest(root)) {
            return root.string();
        }
        if (error != nullptr) {
            *error = "no content/manifest/q4-materials.json under " + root.string();
        }
        return {};
    }

    const fs::path current = fs::current_path();
    if (HasManifest(current)) {
        return current.string();
    }

    fs::path probe = ExecutableDirectory();
    while (!probe.empty()) {
        if (HasManifest(probe)) {
            return probe.string();
        }
        if (probe == probe.root_path()) {
            break;
        }
        probe = probe.parent_path();
    }

    if (error != nullptr) {
        *error =
            "could not find content/manifest/q4-materials.json from the current "
            "directory or the executable; pass -ContentDir";
    }
    return {};
}

bool ContentCatalog::Load(const std::string& repoRoot, std::string* error) {
    repoRoot_ = repoRoot;
    cells_.clear();

    const fs::path manifestPath =
        fs::path(repoRoot) / "content" / "manifest" / "q4-materials.json";
    json::Value manifest;
    if (!ParseFile(manifestPath, &manifest, error)) {
        return false;
    }
    const json::Value* materials = manifest.Find("materials");
    if (materials == nullptr || !materials->IsArray()) {
        if (error != nullptr) {
            *error = manifestPath.string() + ": missing materials array";
        }
        return false;
    }

    for (const json::Value& entry : materials->array) {
        MaterialCell cell;
        cell.material = StringOr(entry, "material");
        cell.cell = StringOr(entry, "cell");
        cell.mtr = StringOr(entry, "mtr");
        if (cell.material.empty() || cell.cell.empty()) {
            continue;
        }
        cell.present = true;
        cell.sidecar = "content/materials/" + cell.material + ".mat.json";

        const fs::path sidecarPath = fs::path(repoRoot) / cell.sidecar;
        json::Value sidecar;
        std::string sidecarError;
        if (!ParseFile(sidecarPath, &sidecar, &sidecarError)) {
            cell.notRunReason = sidecarError;
            cells_.push_back(std::move(cell));
            continue;
        }
        const json::Value* channels = sidecar.Find("channels");
        if (channels == nullptr || !channels->IsObject()) {
            cell.notRunReason = sidecarPath.string() + ": missing channels object";
            cells_.push_back(std::move(cell));
            continue;
        }
        ReadChannel(*channels, "baseColor", &cell.baseColor);
        ReadChannel(*channels, "normal", &cell.normal);
        ReadChannel(*channels, "roughness", &cell.roughness);
        ReadChannel(*channels, "metalness", &cell.metalness);
        ReadChannel(*channels, "ao", &cell.ao);
        ReadChannel(*channels, "emissive", &cell.emissive);

        if (!cell.metalness.available) {
            const json::Value* metalness = channels->Find("metalness");
            const json::Value* why =
                metalness != nullptr ? metalness->Find("why") : nullptr;
            if ((why != nullptr && why->IsString() &&
                 why->string.find("dielectric") != std::string::npos) ||
                cell.cell == "opaque_dielectric") {
                cell.declaredDielectric = true;
            }
        }

        cell.authored = cell.baseColor.available && cell.roughness.available &&
                        cell.normal.available &&
                        (cell.metalness.available || cell.declaredDielectric);
        if (!cell.authored) {
            cell.notRunReason =
                "declared channels are insufficient for the hybrid opaque route";
        }
        cells_.push_back(std::move(cell));
    }

    if (cells_.empty()) {
        if (error != nullptr) {
            *error = manifestPath.string() + ": the Q4 material set is empty";
        }
        return false;
    }
    return true;
}

const MaterialCell* ContentCatalog::FindCell(const std::string& cell) const {
    for (const MaterialCell& entry : cells_) {
        if (entry.cell == cell) {
            return &entry;
        }
    }
    return nullptr;
}

const MaterialCell* ContentCatalog::FindMaterial(const std::string& material) const {
    for (const MaterialCell& entry : cells_) {
        if (entry.material == material) {
            return &entry;
        }
    }
    return nullptr;
}

bool ContentCatalog::LoadPackage(const std::string& packageDir,
                                 const std::string& material,
                                 MaterialPackage* package, std::string* error) const {
    *package = MaterialPackage{};
    package->material = material;
    package->dir = (fs::path(packageDir) / material).string();

    const fs::path manifestPath = fs::path(package->dir) / "package.json";
    json::Value root;
    std::string parseError;
    if (!ParseFile(manifestPath, &root, &parseError)) {
        package->error = parseError;
        if (error != nullptr) {
            *error = parseError;
        }
        return false;
    }

    package->packageSha256 = StringOr(root, "package_sha256");
    const json::Value* textures = root.Find("textures");
    if (textures == nullptr || !textures->IsArray()) {
        package->error = manifestPath.string() + ": missing textures array";
        if (error != nullptr) {
            *error = package->error;
        }
        return false;
    }
    for (const json::Value& entry : textures->array) {
        PackageTexture texture;
        texture.channel = StringOr(entry, "channel");
        texture.file = StringOr(entry, "file");
        texture.format = StringOr(entry, "format");
        texture.width = static_cast<uint32_t>(NumberOr(entry, "width"));
        texture.height = static_cast<uint32_t>(NumberOr(entry, "height"));
        texture.mips = static_cast<uint32_t>(NumberOr(entry, "mips"));
        texture.srgb = BoolOr(entry, "srgb", false);
        if (!texture.channel.empty() && !texture.file.empty()) {
            package->textures.push_back(std::move(texture));
        }
    }
    if (package->textures.empty()) {
        package->error = manifestPath.string() + ": no textures";
        if (error != nullptr) {
            *error = package->error;
        }
        return false;
    }
    package->present = true;
    return true;
}

}  // namespace q4
