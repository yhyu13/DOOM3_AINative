#pragma once

// Private test support. This directory is excluded from the public-surface
// lexical gate because these tests intentionally name backend-private state,
// the private journal, fence values and staging pitch.

#include "d3d12/d3d12_private.h"
#include "q1a_test_support.h"

#include <algorithm>
#include <string>
#include <vector>

namespace q1a_private {

using doom3::rhi::d3d12::CheckState;
using doom3::rhi::d3d12::DiagnosticReport;
using doom3::rhi::d3d12::JournalEntry;
using doom3::rhi::d3d12::RawReadback;

inline bool AllowedWhenRequested(CheckState state) {
    return state == CheckState::Run || state == CheckState::Skipped;
}

// Numeric values from d3d12sdklayers.h, kept as plain integers so these tests
// need no native D3D12 type.
constexpr uint32_t kSeverityWarning = 2;
constexpr uint32_t kSeverityMessage = 4;
constexpr uint32_t kClearRenderTargetNoOptimizedValue = 820;
constexpr uint32_t kClearDepthStencilNoOptimizedValue = 821;
constexpr uint32_t kDebugLayerStartupOptions = 1016;

// Exact allowance for ID 820 in one diagnostic report.
//
// W-820 is a performance advisory, not a correctness failure: the Q1a
// submission clears a different colour per row, so no single optimized clear
// value can match every ClearRenderTargetView. Only that phase may produce it.
// This is a budget, not a blanket waiver by ID and severity: the Q1a row clear
// must emit exactly one per cleared row, and every phase that follows must add
// zero. A Q1b colour attachment that regresses to a missing or mismatching
// optimized clear value emits the same ID 820, so a phase-agnostic waiver would
// hide it. `rhi_diagnostics_test.cpp` therefore asserts both the exact Q1a count
// and a zero delta across the Q1b draw, and passes the Q1a count here as the
// cumulative allowance.
struct WaiverBudget {
    // Number of ID-820 warnings the report is allowed to contain.
    uint32_t clearTarget820 = 0;
    // Device-creation startup notice emitted when GPU-based validation is on.
    bool allowStartupOptions1016 = true;
};

// Counts messages with one numeric ID and severity. The info queue is
// cumulative: `UpdateDiagnostics` reads every stored message and never clears,
// so callers compare two reads to attribute a message to the work in between.
inline uint32_t CountMessages(const DiagnosticReport& report, uint32_t id,
                              uint32_t severity) {
    const size_t parallel =
        std::min(report.messageIds.size(), report.messageSeverities.size());
    uint32_t count = 0;
    for (size_t index = 0; index < parallel; ++index) {
        if (report.messageIds[index] == id &&
            report.messageSeverities[index] == severity) {
            ++count;
        }
    }
    return count;
}

// A message is waived only when a precise reason exists and is recorded in
// specs/004-quality-renderer/evidence-q1.md. Waiving is not silence: every
// message is logged with its ID before this gate is consulted.
inline bool RequireNoUnwaivedMessages(const char* label,
                                      const DiagnosticReport& report,
                                      const WaiverBudget& budget = WaiverBudget{}) {
    bool ok = q1a_test::Require(
        report.messageCount == report.messageIds.size(),
        std::string(label) + " message capture was truncated");
    ok &= q1a_test::Require(
        report.messages.size() == report.messageIds.size() &&
            report.messageIds.size() == report.messageSeverities.size(),
        std::string(label) + " diagnostic message vectors are not parallel");
    uint32_t waivedClearTarget820 = 0;
    for (size_t index = 0; index < report.messageIds.size(); ++index) {
        const uint32_t id = report.messageIds[index];
        const uint32_t severity = report.messageSeverities[index];
        bool waived = false;
        if (severity == kSeverityWarning && id == kClearRenderTargetNoOptimizedValue) {
            // Budget, not bypass: the caller also asserts the exact count, so a
            // report with fewer or more ID-820 messages than the budget fails.
            if (waivedClearTarget820 < budget.clearTarget820) {
                waived = true;
                ++waivedClearTarget820;
            }
        } else if (severity == kSeverityMessage && id == kDebugLayerStartupOptions) {
            waived = budget.allowStartupOptions1016;
        }
        if (!waived) {
            q1a_test::Log("UNWAIVED %s message[%zu] id=%u severity=%u text=%s", label,
                          index, id, severity, report.messages[index].c_str());
            ok = false;
        }
    }
    return q1a_test::Require(ok, std::string(label) + " left unwaived messages");
}

inline void LogDiagnosticMessages(const char* label, const DiagnosticReport& report) {
    q1a_test::Log("DIAGNOSTICS %s captured=%zu message_count=%u", label,
                  report.messages.size(), report.messageCount);
    for (size_t index = 0; index < report.messages.size(); ++index) {
        const uint32_t id =
            index < report.messageIds.size() ? report.messageIds[index] : 0u;
        const uint32_t severity = index < report.messageSeverities.size()
                                      ? report.messageSeverities[index]
                                      : 0u;
        q1a_test::Log("DIAGNOSTICS %s message[%zu] id=%u severity=%u text=%s", label,
                      index, id, severity, report.messages[index].c_str());
    }
}

inline void LogJournal(const std::vector<JournalEntry>& journal) {
    q1a_test::Log("JOURNAL size=%zu", journal.size());
    for (size_t index = 0; index < journal.size(); ++index) {
        q1a_test::Log("JOURNAL [%zu] resource=%s from=%s to=%s reason=%s", index,
                      journal[index].resource.c_str(), journal[index].from.c_str(),
                      journal[index].to.c_str(), journal[index].reason.c_str());
    }
}

inline bool JournalMatches(const std::vector<JournalEntry>& journal) {
    if (!q1a_test::Require(journal.size() == 3,
                           "journal does not contain exactly three entries")) {
        return false;
    }
    const bool sequence =
        journal[0].resource == "render-target" && journal[0].from == "UNKNOWN" &&
        journal[0].to == "RENDER_TARGET" && journal[1].resource == "render-target" &&
        journal[1].from == "RENDER_TARGET" && journal[1].to == "COPY_SOURCE" &&
        journal[2].resource == "readback-buffer" && journal[2].from == "UNKNOWN" &&
        journal[2].to == "COPY_DEST";
    bool wronglyCopied = false;
    for (const auto& entry : journal) {
        if (entry.resource == "render-target" && entry.to == "COPY_DEST") {
            wronglyCopied = true;
        }
    }
    q1a_test::Log("JOURNAL exact_sequence=%s render_target_as_copy_dest=%s",
                  sequence ? "true" : "false", wronglyCopied ? "true" : "false");
    return q1a_test::Require(sequence,
                             "journal is not UNKNOWN->RENDER_TARGET, "
                             "RENDER_TARGET->COPY_SOURCE, UNKNOWN->COPY_DEST in order") &&
           q1a_test::Require(!wronglyCopied,
                             "journal records the render target as a copy destination");
}

inline bool WalkRawRowsMatch(const RawReadback& raw, std::string* detail) {
    bool ok = q1a_test::Require(raw.width == q1a_test::kWidth &&
                                    raw.height == q1a_test::kHeight,
                                "raw readback dimensions are not 4x4");
    ok &= q1a_test::Require(raw.tightRowStride == q1a_test::kRowStride,
                            "raw readback tight stride is not width * 4");
    ok &= q1a_test::Require(raw.stagingPitch >= raw.tightRowStride &&
                                raw.staging.size() ==
                                    static_cast<size_t>(raw.stagingPitch) * raw.height,
                            "raw staging does not cover stagingPitch * height");
    if (!ok) {
        if (detail != nullptr) {
            *detail = "raw readback shape is wrong";
        }
        return false;
    }
    for (uint32_t row = 0; row < raw.height; ++row) {
        const uint8_t* actual =
            raw.staging.data() + static_cast<size_t>(row) * raw.stagingPitch;
        const uint8_t* expected = q1a_test::ExpectedRow(row);
        uint32_t mismatches = 0;
        for (uint32_t i = 0; i < raw.tightRowStride; ++i) {
            if (std::abs(static_cast<int>(actual[i]) - static_cast<int>(expected[i])) >
                q1a_test::kTolerance) {
                ++mismatches;
            }
        }
        q1a_test::Log("RAW_ROW %u mismatches=%u %s", row, mismatches,
                      mismatches == 0 ? "OK" : "FAIL");
        if (mismatches != 0) {
            ok = false;
        }
    }
    if (detail != nullptr) {
        *detail = ok ? "raw private rows are within tolerance" : "raw private row mismatch";
    }
    return ok;
}

// The staging-pitch control: walk the raw staging bytes with width * 4 as the
// stride while the real stride is larger. Every row after the first starts at
// the wrong offset and the independent comparison must reject it.
inline q1a_test::ControlResult StagingPitchControl(const RawReadback& raw) {
    q1a_test::ControlResult control;
    if (raw.stagingPitch <= raw.tightRowStride || raw.height < 2) {
        control.detail =
            "staging pitch equals the tight stride; the control is not applicable";
        return control;
    }
    control.applicable = true;
    uint32_t mismatchedRows = 0;
    for (uint32_t row = 0; row < raw.height; ++row) {
        const size_t offset = static_cast<size_t>(row) * raw.tightRowStride;
        if (offset + raw.tightRowStride > raw.staging.size()) {
            control.detail = "staging bytes are too short for the control";
            control.applicable = false;
            return control;
        }
        const std::vector<uint8_t> bytes(
            raw.staging.begin() + static_cast<std::ptrdiff_t>(offset),
            raw.staging.begin() +
                static_cast<std::ptrdiff_t>(offset + raw.tightRowStride));
        if (!q1a_test::RowMatchesExpected(bytes, row)) {
            ++mismatchedRows;
        }
    }
    control.detected = mismatchedRows > 0;
    control.detail = "tight-stride walk over the staging bytes mismatched " +
                     std::to_string(mismatchedRows) + " row(s) with pitch " +
                     std::to_string(raw.stagingPitch);
    return control;
}

// Runs a callback for both adapter classes with a private diagnostic request.
// Returns 0 pass, 77 all classes unavailable, 1 failure.
template <typename Callback>
int ForEachAdapterPrivate(const doom3::rhi::d3d12::DiagnosticRequest& request,
                          Callback callback) {
    int passes = 0;
    int skips = 0;
    int failures = 0;
    for (doom3::rhi::AdapterClass adapterClass :
         {doom3::rhi::AdapterClass::Hardware, doom3::rhi::AdapterClass::Software}) {
        doom3::rhi::Device device;
        doom3::rhi::DeviceOptions options;
        options.adapterClass = adapterClass;
        std::string error;
        const doom3::rhi::Status status =
            doom3::rhi::d3d12::InitializeWithDiagnostics(device, options, request, &error);
        if (status == doom3::rhi::Status::Unsupported) {
            q1a_test::Log("PRIVATE_ADAPTER %s SKIPPED reason=%s",
                          doom3::rhi::ToString(adapterClass), error.c_str());
            ++skips;
            continue;
        }
        if (status != doom3::rhi::Status::Ok) {
            q1a_test::Log("PRIVATE_ADAPTER %s FAILED status=%s reason=%s",
                          doom3::rhi::ToString(adapterClass),
                          doom3::rhi::ToString(status), error.c_str());
            ++failures;
            continue;
        }
        q1a_test::PrintAdapter(device.adapter());
        const int result = callback(device);
        const doom3::rhi::Status shutdown = device.Shutdown();
        if (shutdown != doom3::rhi::Status::Ok) {
            q1a_test::Log("PRIVATE_ADAPTER %s SHUTDOWN_FAILED status=%s",
                          doom3::rhi::ToString(adapterClass),
                          doom3::rhi::ToString(shutdown));
            ++failures;
        } else if (result == 0) {
            ++passes;
            q1a_test::Log("PRIVATE_ADAPTER %s RESULT PASS",
                          doom3::rhi::ToString(adapterClass));
        } else if (result == q1a_test::kSkipExitCode) {
            ++skips;
        } else {
            ++failures;
            q1a_test::Log("PRIVATE_ADAPTER %s RESULT FAIL",
                          doom3::rhi::ToString(adapterClass));
        }
    }
    q1a_test::Log("PRIVATE_SUMMARY pass=%d fail=%d skip=%d", passes, failures, skips);
    if (failures > 0) {
        return 1;
    }
    if (passes > 0) {
        return 0;
    }
    return q1a_test::kSkipExitCode;
}

}  // namespace q1a_private
