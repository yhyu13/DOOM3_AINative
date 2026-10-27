#pragma once

// Shared Q1a fixture support. Test expectations in this file are literal,
// independent constants; they are never derived from a backend readback.

#include "rhi/rhi.h"

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace q1a_test {

constexpr uint32_t kWidth = 4;
constexpr uint32_t kHeight = 4;
constexpr uint32_t kTightRowPitch = kWidth * 4;
constexpr int kSkipExitCode = 77;
constexpr int kTolerance = 1;  // one 8-bit UNORM step

// Per-row pattern. Every row is distinct and recoverable from the bytes.
// These are hard-coded expectations, not values computed from a renderer.
inline constexpr uint8_t kExpected[kHeight][kTightRowPitch] = {
    {0x00, 0x40, 0x80, 0xFF, 0x00, 0x40, 0x80, 0xFF, 0x00, 0x40, 0x80, 0xFF, 0x00, 0x40, 0x80, 0xFF},
    {0xFF, 0xBF, 0x80, 0x40, 0xFF, 0xBF, 0x80, 0x40, 0xFF, 0xBF, 0x80, 0x40, 0xFF, 0xBF, 0x80, 0x40},
    {0x20, 0x60, 0xA0, 0xE0, 0x20, 0x60, 0xA0, 0xE0, 0x20, 0x60, 0xA0, 0xE0, 0x20, 0x60, 0xA0, 0xE0},
    {0x10, 0x80, 0xF0, 0x30, 0x10, 0x80, 0xF0, 0x30, 0x10, 0x80, 0xF0, 0x30, 0x10, 0x80, 0xF0, 0x30},
};

inline void Log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::vprintf(format, args);
    va_end(args);
    std::printf("\n");
    std::fflush(stdout);
}

inline bool Require(bool condition, const std::string& message) {
    if (!condition) {
        Log("ASSERTION FAILED: %s", message.c_str());
        return false;
    }
    return true;
}

inline doom3::rhi::ClearReadbackDesc MakePatternDesc() {
    doom3::rhi::ClearReadbackDesc desc;
    desc.width = kWidth;
    desc.height = kHeight;
    desc.rows.reserve(kHeight);
    for (uint32_t y = 0; y < kHeight; ++y) {
        doom3::rhi::ClearRow row;
        row.red = kExpected[y][0];
        row.green = kExpected[y][1];
        row.blue = kExpected[y][2];
        row.alpha = kExpected[y][3];
        desc.rows.push_back(row);
    }
    return desc;
}

inline uint64_t Fnv1a64(const uint8_t* bytes, size_t count) {
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0; i < count; ++i) {
        hash ^= static_cast<uint64_t>(bytes[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

inline std::string HexBytes(const uint8_t* bytes, size_t count) {
    static const char* digits = "0123456789abcdef";
    std::string result;
    result.reserve(count * 2);
    for (size_t i = 0; i < count; ++i) {
        result.push_back(digits[(bytes[i] >> 4) & 0xF]);
        result.push_back(digits[bytes[i] & 0xF]);
    }
    return result;
}

inline void PrintAdapter(const doom3::rhi::AdapterInfo& adapter) {
    Log("ADAPTER class=%s name=%s vendor=0x%04x device=0x%04x vram=%llu luid=0x%016llx",
        doom3::rhi::ToString(adapter.adapterClass), adapter.name.c_str(), adapter.vendorId,
        adapter.deviceId, static_cast<unsigned long long>(adapter.dedicatedVideoMemory),
        static_cast<unsigned long long>(adapter.luid));
}

inline const uint8_t* ExpectedRow(uint32_t row) {
    return kExpected[row % kHeight];
}

inline bool CompareTightReadback(const doom3::rhi::ReadbackResult& result,
                                 std::string* detail) {
    if (!Require(result.width == kWidth && result.height == kHeight,
                 "readback dimensions are not 4x4")) {
        return false;
    }
    if (!Require(result.tightRowPitch == kTightRowPitch,
                 "tight row pitch is not width * 4")) {
        return false;
    }
    if (!Require(result.pixels.size() == static_cast<size_t>(kHeight) * kTightRowPitch,
                 "tight readback has the wrong byte count")) {
        return false;
    }

    bool ok = true;
    for (uint32_t y = 0; y < kHeight; ++y) {
        const uint8_t* actual = result.pixels.data() + static_cast<size_t>(y) * kTightRowPitch;
        const uint8_t* expected = ExpectedRow(y);
        uint32_t mismatches = 0;
        uint32_t max_delta = 0;
        for (uint32_t i = 0; i < kTightRowPitch; ++i) {
            const int delta = std::abs(static_cast<int>(actual[i]) - static_cast<int>(expected[i]));
            max_delta = std::max(max_delta, static_cast<uint32_t>(delta));
            if (delta > kTolerance) {
                ++mismatches;
            }
        }
        Log("ROW %u expected_hash=0x%016llx actual_hash=0x%016llx expected=%s actual=%s mismatches=%u max_delta=%u tolerance=%d %s",
            y, static_cast<unsigned long long>(Fnv1a64(expected, kTightRowPitch)),
            static_cast<unsigned long long>(Fnv1a64(actual, kTightRowPitch)),
            HexBytes(expected, kTightRowPitch).c_str(),
            HexBytes(actual, kTightRowPitch).c_str(), mismatches, max_delta, kTolerance,
            mismatches == 0 ? "OK" : "FAIL");
        if (mismatches != 0) {
            ok = false;
        }
    }
    if (detail) {
        *detail = ok ? "all rows within UNORM tolerance" : "one or more rows exceed tolerance";
    }
    return ok;
}

inline std::vector<uint8_t> ExtractRow(const std::vector<uint8_t>& staging, uint32_t rowPitch,
                                       uint32_t rowIndex) {
    std::vector<uint8_t> out(kTightRowPitch, 0);
    const size_t offset = static_cast<size_t>(rowIndex) * rowPitch;
    if (offset + kTightRowPitch > staging.size()) {
        return {};
    }
    std::memcpy(out.data(), staging.data() + offset, kTightRowPitch);
    return out;
}

inline bool RowMatchesExpected(const std::vector<uint8_t>& bytes, uint32_t expectedRow) {
    if (bytes.size() != kTightRowPitch) {
        return false;
    }
    const uint8_t* expected = ExpectedRow(expectedRow);
    for (uint32_t i = 0; i < kTightRowPitch; ++i) {
        if (std::abs(static_cast<int>(bytes[i]) - static_cast<int>(expected[i])) > kTolerance) {
            return false;
        }
    }
    return true;
}

struct ControlResult {
    bool applicable = false;
    bool detected = false;
    std::string detail;
};

inline ControlResult WrongStrideControl(const doom3::rhi::ReadbackResult& result) {
    ControlResult control;
    if (result.rowPitch <= kTightRowPitch) {
        control.detail = "rowPitch equals width * 4; wrong-stride control is not applicable";
        return control;
    }
    control.applicable = true;
    uint32_t mismatchedRows = 0;
    for (uint32_t y = 0; y < kHeight; ++y) {
        const size_t offset = static_cast<size_t>(y) * kTightRowPitch;
        if (offset + kTightRowPitch > result.staging.size()) {
            control.detail = "staging buffer is too short for wrong-stride control";
            return control;
        }
        std::vector<uint8_t> bytes(result.staging.begin() + static_cast<std::ptrdiff_t>(offset),
                                   result.staging.begin() +
                                       static_cast<std::ptrdiff_t>(offset + kTightRowPitch));
        if (!RowMatchesExpected(bytes, y)) {
            ++mismatchedRows;
        }
    }
    control.detected = mismatchedRows > 0;
    control.detail = "wrong stride mismatched " + std::to_string(mismatchedRows) + "/4 rows";
    return control;
}

inline ControlResult OffByOneControl(const doom3::rhi::ReadbackResult& result) {
    ControlResult control;
    if (result.rowPitch < kTightRowPitch || kHeight < 2) {
        control.detail = "height or row pitch does not permit an off-by-one-row control";
        return control;
    }
    control.applicable = true;
    uint32_t mismatchedRows = 0;
    for (uint32_t y = 0; y < kHeight; ++y) {
        const uint32_t sourceRow = (y + 1) % kHeight;
        const std::vector<uint8_t> bytes = ExtractRow(result.staging, result.rowPitch, sourceRow);
        if (!RowMatchesExpected(bytes, y)) {
            ++mismatchedRows;
        }
    }
    control.detected = mismatchedRows > 0;
    control.detail = "off-by-one-row source mismatched " + std::to_string(mismatchedRows) + "/4 rows";
    return control;
}

inline ControlResult RepeatRowZeroControl(const doom3::rhi::ReadbackResult& result) {
    ControlResult control;
    if (result.rowPitch < kTightRowPitch || kHeight < 2) {
        control.detail = "height or row pitch does not permit a repeat-row-0 control";
        return control;
    }
    control.applicable = true;
    uint32_t mismatchedRows = 0;
    for (uint32_t y = 0; y < kHeight; ++y) {
        const std::vector<uint8_t> bytes = ExtractRow(result.staging, result.rowPitch, 0);
        if (!RowMatchesExpected(bytes, y)) {
            ++mismatchedRows;
        }
    }
    control.detected = mismatchedRows > 0;
    control.detail = "repeat-row-0 mismatched " + std::to_string(mismatchedRows) + "/4 rows";
    return control;
}

inline void PrintControl(const char* name, const ControlResult& control) {
    if (!control.applicable) {
        Log("NEGATIVE_CONTROL %s NOT APPLICABLE reason=%s", name, control.detail.c_str());
        return;
    }
    Log("NEGATIVE_CONTROL %s APPLICABLE detected=%s detail=%s", name,
        control.detected ? "YES" : "NO", control.detail.c_str());
}

inline bool WriteBmp(const std::string& path, const doom3::rhi::ReadbackResult& result,
                     uint32_t scale = 16) {
    if (result.width == 0 || result.height == 0 ||
        result.pixels.size() < static_cast<size_t>(result.height) * result.tightRowPitch) {
        return false;
    }
    std::error_code ec;
    const std::filesystem::path outputPath(path);
    if (outputPath.has_parent_path()) {
        std::filesystem::create_directories(outputPath.parent_path(), ec);
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }

    const uint32_t outWidth = result.width * scale;
    const uint32_t outHeight = result.height * scale;
    const uint32_t rowStride = (outWidth * 3u + 3u) & ~3u;
    const uint32_t imageSize = rowStride * outHeight;
    const uint32_t fileSize = 14u + 40u + imageSize;

    auto writeU16 = [&out](uint16_t value) {
        const char bytes[2] = {static_cast<char>(value & 0xFF),
                               static_cast<char>((value >> 8) & 0xFF)};
        out.write(bytes, 2);
    };
    auto writeU32 = [&out](uint32_t value) {
        const char bytes[4] = {static_cast<char>(value & 0xFF),
                               static_cast<char>((value >> 8) & 0xFF),
                               static_cast<char>((value >> 16) & 0xFF),
                               static_cast<char>((value >> 24) & 0xFF)};
        out.write(bytes, 4);
    };

    out.write("BM", 2);
    writeU32(fileSize);
    writeU16(0);
    writeU16(0);
    writeU32(54);
    writeU32(40);
    writeU32(outWidth);
    writeU32(outHeight);
    writeU16(1);
    writeU16(24);
    writeU32(0);
    writeU32(imageSize);
    writeU32(2835);
    writeU32(2835);
    writeU32(0);
    writeU32(0);

    std::vector<uint8_t> row(rowStride, 0);
    for (uint32_t outY = outHeight; outY > 0; --outY) {
        std::fill(row.begin(), row.end(), uint8_t{0});
        const uint32_t sourceY = (outY - 1) / scale;
        for (uint32_t outX = 0; outX < outWidth; ++outX) {
            const uint32_t sourceX = outX / scale;
            const uint8_t* pixel =
                result.pixels.data() + static_cast<size_t>(sourceY) * result.tightRowPitch +
                static_cast<size_t>(sourceX) * 4;
            row[outX * 3 + 0] = pixel[2];
            row[outX * 3 + 1] = pixel[1];
            row[outX * 3 + 2] = pixel[0];
        }
        out.write(reinterpret_cast<const char*>(row.data()), rowStride);
    }
    return out.good();
}

inline std::string EvidencePath(const std::string& fileName) {
    std::filesystem::path path = ".";
    char* value = nullptr;
    size_t length = 0;
    if (_dupenv_s(&value, &length, "RHI_EVIDENCE_DIR") == 0 && value != nullptr) {
        if (*value != '\0') {
            path = value;
        }
        std::free(value);
    }
    path /= fileName;
    return path.string();
}

inline void PrintSubmission(const char* label, const doom3::rhi::Submission& submission) {
    Log("SUBMISSION %s id=%u completion_serial=%llu gated=%s", label, submission.id,
        static_cast<unsigned long long>(submission.completionSerial),
        submission.gated ? "true" : "false");
}

// Runs a per-adapter callback for both adapter classes that are present.
// Return 0 for pass, 77 for an individual skip, any other value for failure.
template <typename Callback>
int ForEachAdapter(Callback callback) {
    int passes = 0;
    int failures = 0;
    int skips = 0;
    for (doom3::rhi::AdapterClass adapterClass :
         {doom3::rhi::AdapterClass::Hardware, doom3::rhi::AdapterClass::Software}) {
        doom3::rhi::Device device;
        doom3::rhi::DeviceOptions options;
        options.adapterClass = adapterClass;
        std::string error;
        const doom3::rhi::Status status = device.Initialize(options, &error);
        if (status == doom3::rhi::Status::Unsupported) {
            Log("ADAPTER %s SKIPPED reason=%s", doom3::rhi::ToString(adapterClass), error.c_str());
            ++skips;
            continue;
        }
        if (status != doom3::rhi::Status::Ok) {
            Log("ADAPTER %s FAILED status=%s reason=%s", doom3::rhi::ToString(adapterClass),
                doom3::rhi::ToString(status), error.c_str());
            Require(false, "adapter initialization failed for " +
                              std::string(doom3::rhi::ToString(adapterClass)));
            ++failures;
            continue;
        }
        PrintAdapter(device.adapter());
        int result = callback(device);
        const doom3::rhi::Status shutdownStatus = device.Shutdown();
        if (shutdownStatus != doom3::rhi::Status::Ok) {
            Log("ADAPTER %s SHUTDOWN_FAILED status=%s", doom3::rhi::ToString(adapterClass),
                doom3::rhi::ToString(shutdownStatus));
            Require(false, "adapter shutdown failed for " +
                              std::string(doom3::rhi::ToString(adapterClass)));
            result = 1;
        }
        if (result == 0) {
            ++passes;
            Log("ADAPTER %s RESULT PASS", doom3::rhi::ToString(adapterClass));
        } else if (result == kSkipExitCode) {
            ++skips;
            Log("ADAPTER %s RESULT SKIPPED", doom3::rhi::ToString(adapterClass));
        } else {
            ++failures;
            Log("ADAPTER %s RESULT FAIL", doom3::rhi::ToString(adapterClass));
        }
    }
    Log("SUMMARY pass=%d fail=%d skip=%d", passes, failures, skips);
    if (failures > 0) {
        return 1;
    }
    if (passes > 0) {
        return 0;
    }
    return kSkipExitCode;
}

}  // namespace q1a_test
