#pragma once

// Shared Q1a public-fixture support.
//
// Everything here is scanned by scripts/validate-rhi-contract.py as a public
// test unit, so it names no backend type, handle or private vocabulary. The
// literals are independent expectations, never derived from a readback.

#include "rhi/rhi.h"

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace q1a_test {

constexpr uint32_t kWidth = 4;
constexpr uint32_t kHeight = 4;
constexpr uint32_t kRowStride = kWidth * 4;
constexpr int kSkipExitCode = 77;
constexpr int kTolerance = 1;  // one 8-bit UNORM step

// Per-row pattern. Every row is distinct and recoverable from the bytes.
inline constexpr uint8_t kExpected[kHeight][kRowStride] = {
    {0x00, 0x40, 0x80, 0xFF, 0x00, 0x40, 0x80, 0xFF, 0x00, 0x40, 0x80, 0xFF, 0x00, 0x40, 0x80, 0xFF},
    {0xFF, 0xBF, 0x80, 0x40, 0xFF, 0xBF, 0x80, 0x40, 0xFF, 0xBF, 0x80, 0x40, 0xFF, 0xBF, 0x80, 0x40},
    {0x20, 0x60, 0xA0, 0xE0, 0x20, 0x60, 0xA0, 0xE0, 0x20, 0x60, 0xA0, 0xE0, 0x20, 0x60, 0xA0, 0xE0},
    {0x10, 0x80, 0xF0, 0x30, 0x10, 0x80, 0xF0, 0x30, 0x10, 0x80, 0xF0, 0x30, 0x10, 0x80, 0xF0, 0x30},
};

// FNV-1a 64 over each canonical expected row, published before the run. The
// integrity check recomputes the hash from kExpected and compares it here; it
// does not depend on any backend output. See evidence-q1a.md for the derivation.
inline constexpr uint64_t kExpectedDigest[kHeight] = {
    0x06B23876A7A2A8C5ull,
    0xA5581568C728313Dull,
    0x6C69B69DA236AC65ull,
    0xF0FBAA39868107A5ull,
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

// Expected-data integrity. A corrupted literal or a stale published digest
// fails before any GPU work is trusted.
inline bool VerifyExpectedData(std::string* detail) {
    bool ok = true;
    for (uint32_t row = 0; row < kHeight; ++row) {
        const uint64_t digest = Fnv1a64(kExpected[row], kRowStride);
        Log("EXPECTED row=%u digest=0x%016llx published=0x%016llx %s", row,
            static_cast<unsigned long long>(digest),
            static_cast<unsigned long long>(kExpectedDigest[row]),
            digest == kExpectedDigest[row] ? "OK" : "MISMATCH");
        if (digest != kExpectedDigest[row]) {
            ok = false;
        }
    }
    if (detail != nullptr) {
        *detail = ok ? "published expected digests match the canonical rows"
                     : "published expected digest does not match its canonical row";
    }
    return ok;
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

// A deliberately long submission used only to widen the in-flight window for a
// bounded observation. Rows cycle through the same four literal row colours.
inline doom3::rhi::ClearReadbackDesc MakeLargeDesc(uint32_t width, uint32_t height) {
    doom3::rhi::ClearReadbackDesc desc;
    desc.width = width;
    desc.height = height;
    desc.rows.reserve(height);
    for (uint32_t y = 0; y < height; ++y) {
        const uint32_t source = y % kHeight;
        doom3::rhi::ClearRow row;
        row.red = kExpected[source][0];
        row.green = kExpected[source][1];
        row.blue = kExpected[source][2];
        row.alpha = kExpected[source][3];
        desc.rows.push_back(row);
    }
    return desc;
}

inline void PrintAdapter(const doom3::rhi::AdapterInfo& adapter) {
    Log("ADAPTER class=%s name=%s vendor=0x%04x device=0x%04x vram=%llu",
        doom3::rhi::ToString(adapter.adapterClass), adapter.name.c_str(), adapter.vendorId,
        adapter.deviceId, static_cast<unsigned long long>(adapter.dedicatedVideoMemory));
}

inline const uint8_t* ExpectedRow(uint32_t row) {
    return kExpected[row % kHeight];
}

inline bool CompareRows(const doom3::rhi::ReadbackResult& result, uint32_t expectedRows,
                        std::string* detail) {
    if (!Require(result.width == kWidth && result.height == expectedRows,
                 "readback dimensions do not match the submitted description")) {
        return false;
    }
    if (!Require(result.rowStride == kRowStride,
                 "tight row stride is not width * 4")) {
        return false;
    }
    if (!Require(result.pixels.size() ==
                     static_cast<size_t>(expectedRows) * kRowStride,
                 "tight readback has the wrong byte count")) {
        return false;
    }

    bool ok = true;
    for (uint32_t y = 0; y < expectedRows; ++y) {
        const uint8_t* actual =
            result.pixels.data() + static_cast<size_t>(y) * kRowStride;
        const uint8_t* expected = ExpectedRow(y);
        uint32_t mismatches = 0;
        uint32_t max_delta = 0;
        for (uint32_t i = 0; i < kRowStride; ++i) {
            const int delta =
                std::abs(static_cast<int>(actual[i]) - static_cast<int>(expected[i]));
            max_delta = std::max(max_delta, static_cast<uint32_t>(delta));
            if (delta > kTolerance) {
                ++mismatches;
            }
        }
        Log("ROW %u expected_hash=0x%016llx actual_hash=0x%016llx expected=%s actual=%s "
            "mismatches=%u max_delta=%u tolerance=%d %s",
            y, static_cast<unsigned long long>(Fnv1a64(expected, kRowStride)),
            static_cast<unsigned long long>(Fnv1a64(actual, kRowStride)),
            HexBytes(expected, kRowStride).c_str(), HexBytes(actual, kRowStride).c_str(),
            mismatches, max_delta, kTolerance, mismatches == 0 ? "OK" : "FAIL");
        if (mismatches != 0) {
            ok = false;
        }
    }
    if (detail != nullptr) {
        *detail = ok ? "all rows within the UNORM tolerance" : "one or more rows exceed tolerance";
    }
    return ok;
}

inline std::vector<uint8_t> RowAt(const std::vector<uint8_t>& pixels, uint32_t stride,
                                  uint32_t rowIndex) {
    std::vector<uint8_t> out;
    const size_t offset = static_cast<size_t>(rowIndex) * stride;
    if (offset + kRowStride > pixels.size()) {
        return out;
    }
    out.assign(pixels.begin() + static_cast<std::ptrdiff_t>(offset),
               pixels.begin() + static_cast<std::ptrdiff_t>(offset + kRowStride));
    return out;
}

inline bool RowMatchesExpected(const std::vector<uint8_t>& bytes, uint32_t expectedRow) {
    if (bytes.size() != kRowStride) {
        return false;
    }
    const uint8_t* expected = ExpectedRow(expectedRow);
    for (uint32_t i = 0; i < kRowStride; ++i) {
        if (std::abs(static_cast<int>(bytes[i]) - static_cast<int>(expected[i])) > kTolerance) {
            return false;
        }
    }
    return true;
}

// T013 controlled tolerance pair. A one-step difference must be accepted and a
// two-step difference must be rejected by the same byte comparator.
inline bool TolerancePolicyControl(std::string* detail) {
    uint8_t row[kRowStride];
    std::memcpy(row, kExpected[1], kRowStride);
    row[0] = static_cast<uint8_t>(row[0] > 0 ? row[0] - 1 : row[0] + 1);
    std::vector<uint8_t> oneStep(row, row + kRowStride);
    const bool oneAccepted = RowMatchesExpected(oneStep, 1);

    uint8_t two[kRowStride];
    std::memcpy(two, kExpected[1], kRowStride);
    two[0] = static_cast<uint8_t>(two[0] > 1 ? two[0] - 2 : two[0] + 2);
    std::vector<uint8_t> twoStep(two, two + kRowStride);
    const bool twoRejected = !RowMatchesExpected(twoStep, 1);

    Log("TOLERANCE one_step_accepted=%s two_step_rejected=%s",
        oneAccepted ? "true" : "false", twoRejected ? "true" : "false");
    if (detail != nullptr) {
        *detail = (oneAccepted && twoRejected) ? "one-step accepted, two-step rejected"
                                               : "tolerance policy is not discriminating";
    }
    return oneAccepted && twoRejected;
}

struct ControlResult {
    bool applicable = false;
    bool detected = false;
    std::string detail;
};

inline ControlResult OffByOneRowControl(const doom3::rhi::ReadbackResult& result) {
    ControlResult control;
    if (result.height < 2 || result.rowStride != kRowStride) {
        control.detail = "height or packed stride does not permit an off-by-one-row control";
        return control;
    }
    control.applicable = true;
    uint32_t mismatchedRows = 0;
    for (uint32_t y = 0; y < result.height; ++y) {
        const uint32_t sourceRow = (y + 1) % result.height;
        const std::vector<uint8_t> bytes =
            RowAt(result.pixels, result.rowStride, sourceRow);
        if (!RowMatchesExpected(bytes, y)) {
            ++mismatchedRows;
        }
    }
    control.detected = mismatchedRows > 0;
    control.detail = "off-by-one-row source mismatched " + std::to_string(mismatchedRows) +
                     " row(s)";
    return control;
}

inline ControlResult RepeatRowZeroControl(const doom3::rhi::ReadbackResult& result) {
    ControlResult control;
    if (result.height < 2 || result.rowStride != kRowStride) {
        control.detail = "height or packed stride does not permit a repeat-row-0 control";
        return control;
    }
    control.applicable = true;
    uint32_t mismatchedRows = 0;
    for (uint32_t y = 1; y < result.height; ++y) {
        const std::vector<uint8_t> bytes = RowAt(result.pixels, result.rowStride, 0);
        if (!RowMatchesExpected(bytes, y)) {
            ++mismatchedRows;
        }
    }
    control.detected = mismatchedRows > 0;
    control.detail = "repeat-row-0 mismatched " + std::to_string(mismatchedRows) + " row(s)";
    return control;
}

// A test-side row-length misread: treat the packed result as if each row were
// one byte shorter than width * 4. Every row after the first then starts at the
// wrong offset and must be rejected by the independent byte comparison.
inline ControlResult PackedRowLengthControl(const doom3::rhi::ReadbackResult& result) {
    ControlResult control;
    if (result.height < 2 || result.rowStride <= 1) {
        control.detail = "height or packed stride does not permit a row-length misread";
        return control;
    }
    control.applicable = true;
    uint32_t mismatchedRows = 0;
    for (uint32_t y = 0; y < result.height; ++y) {
        const std::vector<uint8_t> bytes =
            RowAt(result.pixels, result.rowStride - 1, y);
        if (!RowMatchesExpected(bytes, y)) {
            ++mismatchedRows;
        }
    }
    control.detected = mismatchedRows > 0;
    control.detail = "short-row misread mismatched " + std::to_string(mismatchedRows) +
                     " row(s)";
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
        result.pixels.size() < static_cast<size_t>(result.height) * result.rowStride) {
        return false;
    }
    std::error_code errorCode;
    const std::filesystem::path outputPath(path);
    if (outputPath.has_parent_path()) {
        std::filesystem::create_directories(outputPath.parent_path(), errorCode);
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }

    const uint32_t outWidth = result.width * scale;
    const uint32_t outHeight = result.height * scale;
    const uint32_t outStride = (outWidth * 3u + 3u) & ~3u;
    const uint32_t imageSize = outStride * outHeight;
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

    std::vector<uint8_t> row(outStride, 0);
    for (uint32_t outY = outHeight; outY > 0; --outY) {
        std::fill(row.begin(), row.end(), uint8_t{0});
        const uint32_t sourceY = (outY - 1) / scale;
        for (uint32_t outX = 0; outX < outWidth; ++outX) {
            const uint32_t sourceX = outX / scale;
            const uint8_t* pixel =
                result.pixels.data() + static_cast<size_t>(sourceY) * result.rowStride +
                static_cast<size_t>(sourceX) * 4;
            row[outX * 3 + 0] = pixel[2];
            row[outX * 3 + 1] = pixel[1];
            row[outX * 3 + 2] = pixel[0];
        }
        out.write(reinterpret_cast<const char*>(row.data()), outStride);
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
    Log("SUBMISSION %s id=%u token=%llu", label, submission.id,
        static_cast<unsigned long long>(submission.completionToken));
}

// Bounded poll of the public completion token. There is no blocking wait in the
// portable seam and this helper does not inspect any backend value.
inline bool WaitComplete(const doom3::rhi::Device& device,
                         const doom3::rhi::Submission& submission,
                         int timeoutMilliseconds = 60000) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeoutMilliseconds);
    while (std::chrono::steady_clock::now() < deadline) {
        if (device.IsComplete(submission)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return device.IsComplete(submission);
}

// Runs a per-adapter callback for both adapter classes that are available.
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
            Log("ADAPTER %s SKIPPED reason=%s", doom3::rhi::ToString(adapterClass),
                error.c_str());
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
