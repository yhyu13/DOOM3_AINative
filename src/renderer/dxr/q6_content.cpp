#include "dxr/q6_content.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace doom3 {
namespace dxr {
namespace {

namespace fs = std::filesystem;

struct CellDescriptor {
    Q6ContentCell cell;
    const char* material;
    const char* matrix;
};

const CellDescriptor kCells[] = {
    {Q6ContentCell::Dielectric, "hero_dielectric", "opaque_dielectric"},
    {Q6ContentCell::Metal, "hero_metal", "opaque_metal"},
    {Q6ContentCell::Grating, "hero_grating", "alpha_tested"},
    {Q6ContentCell::EmissiveSign, "hero_emissive_sign", "emissive"},
    {Q6ContentCell::CharacterSkin, "hero_character_skin", "skinned_character"},
    {Q6ContentCell::Glass, "hero_glass", "transmissive_glass"},
    {Q6ContentCell::Environment, "hero_environment", "large_environment"},
};

std::string ReadText(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return std::string();
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

// Returns the substring of the value that follows `"key"` when the value is an
// object or an array, delimited by its matching brace. Empty when the key is
// absent. The sidecars are authored and stable, so a brace scanner is enough;
// it is not a general JSON parser.
std::string BlockFor(const std::string& text, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    size_t position = text.find(needle);
    if (position == std::string::npos) {
        return std::string();
    }
    position = text.find_first_of("{[", position + needle.size());
    if (position == std::string::npos) {
        return std::string();
    }
    const char open = text[position];
    const char close = open == '{' ? '}' : ']';
    int depth = 0;
    for (size_t i = position; i < text.size(); ++i) {
        if (text[i] == open) {
            ++depth;
        } else if (text[i] == close) {
            --depth;
            if (depth == 0) {
                return text.substr(position, i - position + 1);
            }
        }
    }
    return std::string();
}

std::string StringField(const std::string& text, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    size_t position = text.find(needle);
    if (position == std::string::npos) {
        return std::string();
    }
    position = text.find(':', position + needle.size());
    if (position == std::string::npos) {
        return std::string();
    }
    position = text.find('"', position);
    if (position == std::string::npos) {
        return std::string();
    }
    const size_t end = text.find('"', position + 1);
    if (end == std::string::npos) {
        return std::string();
    }
    return text.substr(position + 1, end - position - 1);
}

bool BoolField(const std::string& text, const std::string& key, bool defaultValue) {
    const std::string needle = "\"" + key + "\"";
    size_t position = text.find(needle);
    if (position == std::string::npos) {
        return defaultValue;
    }
    position = text.find(':', position + needle.size());
    if (position == std::string::npos) {
        return defaultValue;
    }
    const size_t value = text.find_first_not_of(" \t\r\n", position + 1);
    if (value == std::string::npos) {
        return defaultValue;
    }
    if (text.compare(value, 4, "true") == 0) {
        return true;
    }
    if (text.compare(value, 5, "false") == 0) {
        return false;
    }
    return defaultValue;
}

bool NumberField(const std::string& text, const std::string& key, double* out) {
    const std::string needle = "\"" + key + "\"";
    size_t position = text.find(needle);
    if (position == std::string::npos) {
        return false;
    }
    position = text.find(':', position + needle.size());
    if (position == std::string::npos) {
        return false;
    }
    const char* begin = text.c_str() + position + 1;
    char* end = nullptr;
    const double value = std::strtod(begin, &end);
    if (end == begin) {
        return false;
    }
    *out = value;
    return true;
}

uint64_t FileSize(const std::string& path) {
    std::error_code error;
    const auto size = fs::file_size(path, error);
    return error ? 0u : static_cast<uint64_t>(size);
}

Q6Channel ParseChannel(const std::string& channels, const std::string& name) {
    Q6Channel channel;
    const std::string block = BlockFor(channels, name);
    if (block.empty()) {
        channel.state = Q6ChannelState::Absent;
        return channel;
    }
    const std::string state = StringField(block, "state");
    if (state == "authored") {
        channel.state = Q6ChannelState::Authored;
    } else if (state == "unavailable") {
        channel.state = Q6ChannelState::Unavailable;
    } else {
        channel.state = Q6ChannelState::Absent;
    }
    channel.path = StringField(block, "path");
    channel.format = StringField(block, "format");
    channel.srgb = BoolField(block, "srgb", false);
    channel.reason = StringField(block, "why");
    return channel;
}

void CheckChannel(Q6ContentCellRecord* record, const Q6Channel& channel, const char* label,
                  const std::string& repoRoot) {
    if (channel.state != Q6ChannelState::Authored) {
        return;
    }
    const std::string path = repoRoot + "/" + channel.path;
    const uint64_t bytes = FileSize(path);
    if (bytes == 0) {
        if (!record->error.empty()) {
            record->error += "; ";
        }
        record->error += std::string(label) + " declared authored but missing or empty: " +
                         channel.path;
        return;
    }
    record->authoredBytes += bytes;
}

std::vector<std::string> ParseTiles(const std::string& text) {
    std::vector<std::string> tiles;
    const std::string block = BlockFor(text, "tiles");
    if (block.empty()) {
        return tiles;
    }
    size_t position = 0;
    while (true) {
        const size_t path = block.find("\"path\"", position);
        if (path == std::string::npos) {
            break;
        }
        const size_t value = block.find('"', block.find(':', path) + 1);
        if (value == std::string::npos) {
            break;
        }
        const size_t end = block.find('"', value + 1);
        if (end == std::string::npos) {
            break;
        }
        tiles.push_back(block.substr(value + 1, end - value - 1));
        position = end + 1;
    }
    return tiles;
}

}  // namespace

const char* ToString(Q6ContentCell cell) {
    switch (cell) {
        case Q6ContentCell::Dielectric: return "dielectric";
        case Q6ContentCell::Metal: return "metal";
        case Q6ContentCell::Grating: return "grating";
        case Q6ContentCell::EmissiveSign: return "emissive_sign";
        case Q6ContentCell::CharacterSkin: return "character_skin";
        case Q6ContentCell::Glass: return "glass";
        case Q6ContentCell::Environment: return "environment";
        default: return "unknown";
    }
}

const char* CellMaterialName(Q6ContentCell cell) {
    for (const CellDescriptor& descriptor : kCells) {
        if (descriptor.cell == cell) {
            return descriptor.material;
        }
    }
    return "unknown";
}

const char* CellMatrixName(Q6ContentCell cell) {
    for (const CellDescriptor& descriptor : kCells) {
        if (descriptor.cell == cell) {
            return descriptor.matrix;
        }
    }
    return "unknown";
}

const char* ToString(Q6ChannelState state) {
    switch (state) {
        case Q6ChannelState::Authored: return "authored";
        case Q6ChannelState::Unavailable: return "unavailable";
        default: return "absent";
    }
}

const Q6ContentCellRecord* Q6ContentRegistry::Find(Q6ContentCell cell) const {
    for (const Q6ContentCellRecord& record : cells) {
        if (record.cell == cell) {
            return &record;
        }
    }
    return nullptr;
}

uint32_t Q6ContentRegistry::CompleteCount() const {
    uint32_t count = 0;
    for (const Q6ContentCellRecord& record : cells) {
        if (record.complete) {
            ++count;
        }
    }
    return count;
}

std::string Q6ContentRegistry::Identity(Q6ContentCell cell) const {
    const Q6ContentCellRecord* record = Find(cell);
    if (record == nullptr) {
        return std::string("unknown");
    }
    std::ostringstream out;
    out << record->material << " [" << CellMatrixName(cell) << "]"
        << " licence=" << (record->licencePresent ? record->licence : "MISSING")
        << " authored=" << (record->complete ? "yes" : "no");
    return out.str();
}

bool LoadQ6Content(const std::string& repoRoot, Q6ContentRegistry* registry,
                   std::string* error) {
    if (registry == nullptr) {
        if (error != nullptr) {
            *error = "content registry needs an output";
        }
        return false;
    }
    registry->cells.clear();
    bool allComplete = true;
    for (const CellDescriptor& descriptor : kCells) {
        Q6ContentCellRecord record;
        record.cell = descriptor.cell;
        record.material = descriptor.material;
        record.mtr = std::string("content/materials/") + descriptor.material + ".mtr";
        record.sidecar = std::string("content/materials/") + descriptor.material + ".mat.json";
        record.licence = std::string("content/hero/") +
                         std::string(descriptor.material).substr(5) + "/LICENSE";

        const std::string sidecarText = ReadText(repoRoot + "/" + record.sidecar);
        if (sidecarText.empty()) {
            record.error = "sidecar missing: " + record.sidecar;
            allComplete = false;
            registry->cells.push_back(std::move(record));
            continue;
        }
        const std::string channels = BlockFor(sidecarText, "channels");
        record.baseColor = ParseChannel(channels, "baseColor");
        record.normal = ParseChannel(channels, "normal");
        record.roughness = ParseChannel(channels, "roughness");
        record.metalness = ParseChannel(channels, "metalness");
        record.emissive = ParseChannel(channels, "emissive");

        double emissiveScale = 1.0;
        if (NumberField(sidecarText, "emissiveScale", &emissiveScale)) {
            record.emissiveScale = static_cast<float>(emissiveScale);
        }

        // The environment cell declares its baseColor as four tiles rather than
        // one path. The baseColor object embeds those tile paths, so a plain
        // `path` scan would pick the first tile; the tile list is authoritative
        // when it is present.
        record.environmentTiles = ParseTiles(sidecarText);
        if (!record.environmentTiles.empty()) {
            record.baseColor.path.clear();
            for (const std::string& tile : record.environmentTiles) {
                const uint64_t bytes = FileSize(repoRoot + "/" + tile);
                if (bytes == 0) {
                    if (!record.error.empty()) {
                        record.error += "; ";
                    }
                    record.error += "environment tile missing or empty: " + tile;
                } else {
                    record.authoredBytes += bytes;
                }
            }
        }

        // A tiled baseColor is checked tile by tile above, not through a path
        // the sidecar does not carry.
        if (record.environmentTiles.empty()) {
            CheckChannel(&record, record.baseColor, "baseColor", repoRoot);
        }
        CheckChannel(&record, record.normal, "normal", repoRoot);
        CheckChannel(&record, record.roughness, "roughness", repoRoot);
        CheckChannel(&record, record.metalness, "metalness", repoRoot);
        CheckChannel(&record, record.emissive, "emissive", repoRoot);

        const uint64_t licenceBytes = FileSize(repoRoot + "/" + record.licence);
        record.licencePresent = licenceBytes > 0;
        if (!record.licencePresent) {
            if (!record.error.empty()) {
                record.error += "; ";
            }
            record.error += "licence missing or empty: " + record.licence;
        }

        record.complete = record.licencePresent && record.error.empty();
        if (!record.complete) {
            allComplete = false;
        }
        registry->cells.push_back(std::move(record));
    }
    if (error != nullptr) {
        error->clear();
    }
    if (!allComplete && error != nullptr) {
        *error = "one or more Q4 content cells are incomplete";
    }
    return allComplete;
}

bool CellsForIdentity(const std::string& identity, std::vector<Q6ContentCell>* cells) {
    if (cells == nullptr) {
        return false;
    }
    cells->clear();
    if (identity == "PT-S1") {
        *cells = {Q6ContentCell::Environment, Q6ContentCell::Dielectric, Q6ContentCell::Metal};
        return true;
    }
    if (identity == "PT-S2") {
        *cells = {Q6ContentCell::Environment, Q6ContentCell::EmissiveSign, Q6ContentCell::Grating};
        return true;
    }
    if (identity == "PT-S3") {
        *cells = {Q6ContentCell::Environment, Q6ContentCell::Metal, Q6ContentCell::CharacterSkin};
        return true;
    }
    if (identity == "PT-S4") {
        *cells = {Q6ContentCell::Environment, Q6ContentCell::Glass};
        return true;
    }
    if (identity == "PT-S5") {
        *cells = {Q6ContentCell::Environment, Q6ContentCell::Metal};
        return true;
    }
    return false;
}

}  // namespace dxr
}  // namespace doom3
