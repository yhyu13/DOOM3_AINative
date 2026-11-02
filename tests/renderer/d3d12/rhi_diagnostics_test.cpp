#include "d3d12/private_test_support.h"
#include "q1b_test_support.h"

#include <string>

using doom3::rhi::AdapterClass;
using doom3::rhi::ClearReadbackDesc;
using doom3::rhi::ClearRow;
using doom3::rhi::Device;
using doom3::rhi::DeviceOptions;
using doom3::rhi::DrawFrameDesc;
using doom3::rhi::DrawPassDesc;
using doom3::rhi::GraphicsPipelineDesc;
using doom3::rhi::Pipeline;
using doom3::rhi::ReadbackResult;
using doom3::rhi::ReleaseStatus;
using doom3::rhi::ShaderModule;
using doom3::rhi::ShaderModuleDesc;
using doom3::rhi::ShaderStage;
using doom3::rhi::Status;
using doom3::rhi::Submission;
using doom3::rhi::d3d12::CheckState;
using doom3::rhi::d3d12::DiagnosticReport;
using doom3::rhi::d3d12::DiagnosticRequest;

namespace {

// Message waivers and the unwaived-message gate live in
// d3d12/private_test_support.h so the surface fixture applies the same policy.

// The depth half of the Q1b clear-value fix. The attachment is created with
// this value and the pass clears to it; the pre-fix creation declared 1.0f,
// which the debug layer reports as ID 821 (mismatching depth clear). Keeping
// the frame value in one named constant lets the assertion below state the
// value it protects instead of relying on the choice staying incidental.
constexpr float kQ1bDepthClear = 0.5f;
static_assert(kQ1bDepthClear != 1.0f,
              "the Q1b depth clear must differ from the pre-fix creation value 1.0f, "
              "otherwise the ID-821 control cannot detect a regression");

// Exercises the Q1b graphics path on the same device as the Q1a clear: colour
// attachment creation + clear, depth attachment creation + non-default clear,
// a pipeline, a draw and a readback. This is what makes the validation gate
// cover the Q1b attachment code, not only the Q1a render target.
Status SubmitValidationDraw(Device& device, std::string* error) {
    ShaderModule vertex;
    ShaderModuleDesc vertexDesc;
    vertexDesc.stage = ShaderStage::Vertex;
    vertexDesc.source = q1b_test::kFullscreenVertex;
    vertexDesc.entryPoint = "VSMain";
    Status status = device.CreateShaderModule(vertexDesc, &vertex, error);
    if (status != Status::Ok) {
        return status;
    }
    ShaderModule pixel;
    ShaderModuleDesc pixelDesc;
    pixelDesc.stage = ShaderStage::Pixel;
    pixelDesc.source = q1b_test::kPatternPixel;
    pixelDesc.entryPoint = "PSMain";
    status = device.CreateShaderModule(pixelDesc, &pixel, error);
    if (status != Status::Ok) {
        return status;
    }
    GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.vertexShader = vertex;
    pipelineDesc.pixelShader = pixel;
    pipelineDesc.colorAttachmentCount = 1;
    pipelineDesc.depthTest = true;
    pipelineDesc.depthWrite = true;
    Pipeline pipeline;
    status = device.CreateGraphicsPipeline(pipelineDesc, &pipeline, error);
    if (status != Status::Ok) {
        return status;
    }

    uint32_t indices[1] = {0};
    ClearRow clears[1];
    // Alpha 255 is deliberately different from the zeroed pre-fix creation
    // value (0, 0, 0, 0), so a reverted optimized clear value mismatches and
    // surfaces as ID 820 inside the Q1b window rather than being absorbed by
    // the Q1a row-clear budget.
    clears[0].red = 0;
    clears[0].green = 0;
    clears[0].blue = 0;
    clears[0].alpha = 255;
    DrawPassDesc pass;
    pass.pipeline = pipeline;
    pass.colorIndices = indices;
    pass.colorCount = 1;
    pass.colorClears = clears;
    DrawFrameDesc frame;
    frame.width = q1b_test::kWidth;
    frame.height = q1b_test::kHeight;
    frame.attachmentCount = 1;
    frame.depthEnabled = true;
    // Deliberately not 1.0f: creation must follow the frame's clear value.
    frame.depthClear = kQ1bDepthClear;
    frame.passCount = 1;
    frame.passes = &pass;

    Submission submission;
    status = device.SubmitDrawFrame(frame, &submission, error);
    if (status != Status::Ok) {
        return status;
    }
    if (!q1a_test::WaitComplete(device, submission)) {
        if (error != nullptr) {
            *error = "validation draw did not complete";
        }
        return Status::Error;
    }
    ReadbackResult readback;
    status = device.ReadbackAttachment(submission, 0, &readback, error);
    const ReleaseStatus released = device.TryRelease(submission, error);
    if (status != Status::Ok) {
        return status;
    }
    if (released != ReleaseStatus::Released) {
        if (error != nullptr) {
            *error = "validation draw submission was not released";
        }
        return Status::Error;
    }
    return Status::Ok;
}

// Counts cases where a requested validation capability actually executed. A
// case can PASS while this stays zero: the pass is for correct opt-in
// reporting, not for validation execution.
int gValidationExecuted = 0;

// The debug layer is process-global: once any case actually enables it, every
// later device in this process runs under it, including a case that requested
// nothing. `evidence-q1.md` records that inheritance for the software `none`
// case, whose queue holds the same four ID-820 warnings. Track the process-wide
// state, but only after a report says the layer RAN. A request, or a request
// that came back SKIPPED, never sets this flag.
bool gLayerEnabledOnce = false;

// Totals used to reproduce the evidence's "28 = 7 cases x 4" relation without
// hard-coding the number of cases or adapters that happen to be available.
int gLayerActiveCases = 0;
int gClearTarget820Total = 0;

int RunCase(AdapterClass adapterClass, bool requestDebug, bool requestGpuValidation,
            const char* combo) {
    Device device;
    DeviceOptions options;
    options.adapterClass = adapterClass;
    DiagnosticRequest request;
    request.enableDebugLayer = requestDebug;
    request.enableGpuBasedValidation = requestGpuValidation;
    std::string error;
    const Status initialize =
        doom3::rhi::d3d12::InitializeWithDiagnostics(device, options, request, &error);
    if (initialize == Status::Unsupported) {
        q1a_test::Log("DIAGNOSTICS class=%s combo=%s SKIPPED reason=%s",
                      doom3::rhi::ToString(adapterClass), combo, error.c_str());
        return q1a_test::kSkipExitCode;
    }
    bool ok = q1a_test::Require(initialize == Status::Ok, "diagnostics Initialize failed");
    if (initialize != Status::Ok) {
        device.Shutdown();
        return 1;
    }

    const DiagnosticReport requested = doom3::rhi::d3d12::ReadDiagnostics(device);
    q1a_test::Log("DIAGNOSTICS class=%s combo=%s debug_requested=%s "
                  "gpu_validation_requested=%s debug=%s gpu_validation=%s "
                  "messages=%u errors=%u warnings=%u",
                  doom3::rhi::ToString(adapterClass), combo,
                  requestDebug ? "true" : "false",
                  requestGpuValidation ? "true" : "false",
                  doom3::rhi::d3d12::ToString(requested.debugLayer),
                  doom3::rhi::d3d12::ToString(requested.gpuBasedValidation),
                  requested.messageCount, requested.errorCount, requested.warningCount);
    q1a_test::Log("DIAGNOSTICS debug_detail=%s", requested.debugLayerDetail.c_str());
    q1a_test::Log("DIAGNOSTICS gpu_validation_detail=%s",
                  requested.gpuBasedValidationDetail.c_str());
    const bool validationExecuted =
        requested.debugLayer == CheckState::Run ||
        requested.gpuBasedValidation == CheckState::Run;
    if (validationExecuted) {
        ++gValidationExecuted;
        // Only a RUN report proves the process-global layer is really on.
        // Requesting it is not enough: on a host without the SDK layer the
        // report is SKIPPED and nothing enforces validation.
        gLayerEnabledOnce = true;
    }
    // A requested layer that came back SKIPPED is neither active nor a pass.
    // Every case that runs under a layer (including one inherited from an
    // earlier case) still gets the exact-count assertions below.
    const bool requestedValidation = requestDebug || requestGpuValidation;
    const bool validationSkipped = requestedValidation && !validationExecuted;
    const bool layerActive = gLayerEnabledOnce;
    q1a_test::Log("DIAGNOSTICS layer_active=%s",
                  layerActive ? "true" : "false");
    q1a_test::Log("DIAGNOSTICS validation_executed=%s",
                  validationExecuted ? "true" : "false");
    q1a_private::LogDiagnosticMessages("requested", requested);
    // No clear has run yet, so no ID-820 may exist at this point.
    ok &= q1a_private::RequireNoUnwaivedMessages("requested", requested);

    if (requestDebug) {
        ok &= q1a_test::Require(
            q1a_private::AllowedWhenRequested(requested.debugLayer),
            "debug layer was requested but is NOT_RUN");
    } else if (requestGpuValidation) {
        ok &= q1a_test::Require(
            requested.debugLayer == CheckState::NotRequested ||
                requested.debugLayer == CheckState::Run ||
                requested.debugLayer == CheckState::Skipped,
            "GPU-based validation alone left the debug layer unattempted");
    } else {
        ok &= q1a_test::Require(requested.debugLayer == CheckState::NotRequested,
                                "debug layer was not requested but is not NOT_REQUESTED");
    }

    if (requestGpuValidation) {
        ok &= q1a_test::Require(
            q1a_private::AllowedWhenRequested(requested.gpuBasedValidation),
            "GPU-based validation was requested but is NOT_RUN");
        ok &= q1a_test::Require(
            requested.gpuBasedValidation != CheckState::Run ||
                requested.debugLayer == CheckState::Run,
            "GPU-based validation reported RUN while the debug layer is not RUN");
    } else {
        ok &= q1a_test::Require(
            requested.gpuBasedValidation == CheckState::NotRequested,
            "GPU-based validation was not requested but is not NOT_REQUESTED");
    }

    const ClearReadbackDesc desc = q1a_test::MakePatternDesc();
    Submission submission;
    Status status = device.SubmitClearReadback(desc, &submission, &error);
    q1a_test::Log("DIAGNOSTICS work_submit=%s reason=%s",
                  doom3::rhi::ToString(status), error.c_str());
    ok &= q1a_test::Require(status == Status::Ok,
                            "diagnostic device could not submit the clear");
    if (status == Status::Ok) {
        ok &= q1a_test::Require(q1a_test::WaitComplete(device, submission),
                                "diagnostic completion token was not reached");
        ReadbackResult readback;
        status = device.Readback(submission, &readback, &error);
        q1a_test::Log("DIAGNOSTICS work_readback=%s reason=%s",
                      doom3::rhi::ToString(status), error.c_str());
        ok &= q1a_test::Require(status == Status::Ok,
                                "diagnostic device could not read the result back");
        if (status == Status::Ok) {
            ok &= q1a_test::CompareRows(readback, q1a_test::kHeight, &error);
        }
        ok &= q1a_test::Require(
            device.TryRelease(submission, &error) == ReleaseStatus::Released,
            "diagnostic submission was not released");
    }

    // Attribute ID-820 to the phase that produced it. The info queue is
    // cumulative (`UpdateDiagnostics` never clears it), so the count here minus
    // the count in `requested` is exactly what the Q1a row clear emitted: one
    // warning per cleared row while the layer is active, zero otherwise.
    const DiagnosticReport afterClear = doom3::rhi::d3d12::ReadDiagnostics(device);
    const int clearDelta820 =
        static_cast<int>(q1a_private::CountMessages(
            afterClear, q1a_private::kClearRenderTargetNoOptimizedValue,
            q1a_private::kSeverityWarning)) -
        static_cast<int>(q1a_private::CountMessages(
            requested, q1a_private::kClearRenderTargetNoOptimizedValue,
            q1a_private::kSeverityWarning));
    const uint32_t expectedClear820 = layerActive ? q1a_test::kHeight : 0u;
    q1a_test::Log("DIAGNOSTICS q1a_clear_820 delta=%d expected=%u layer_active=%s",
                  clearDelta820, expectedClear820, layerActive ? "true" : "false");
    ok &= q1a_test::Require(
        clearDelta820 == static_cast<int>(expectedClear820),
        "Q1a row clear emitted " + std::to_string(clearDelta820) +
            " ID-820 warning(s), expected " + std::to_string(expectedClear820) +
            " (one per cleared row while the debug layer is active)");
    q1a_private::WaiverBudget clearBudget;
    clearBudget.clearTarget820 = expectedClear820;
    ok &= q1a_private::RequireNoUnwaivedMessages("after-clear", afterClear, clearBudget);
    if (layerActive) {
        ++gLayerActiveCases;
        gClearTarget820Total += clearDelta820;
    }

    std::string drawError;
    const Status drawStatus = SubmitValidationDraw(device, &drawError);
    q1a_test::Log("DIAGNOSTICS q1b_draw=%s reason=%s",
                  doom3::rhi::ToString(drawStatus), drawError.c_str());
    ok &= q1a_test::Require(drawStatus == Status::Ok,
                            "Q1b validation draw failed on the diagnostic device");

    // Always read the queue: the debug layer is process-global, so a case that
    // requested nothing can still be running under a layer enabled by an
    // earlier case. Gating unconditionally closes that blind spot.
    const DiagnosticReport after = doom3::rhi::d3d12::ReadDiagnostics(device);
    q1a_test::Log("DIAGNOSTICS after messages=%u errors=%u warnings=%u",
                  after.messageCount, after.errorCount, after.warningCount);
    q1a_private::LogDiagnosticMessages("after", after);
    // The Q1b draw must add no mismatching-clear diagnostic. This is the
    // regression control an unconditional ID-820 waiver could not provide: a
    // reverted colour clear value emits the same ID 820 as the Q1a row clear,
    // so only the phase-aware delta distinguishes them.
    const int drawDelta820 =
        static_cast<int>(q1a_private::CountMessages(
            after, q1a_private::kClearRenderTargetNoOptimizedValue,
            q1a_private::kSeverityWarning)) -
        static_cast<int>(q1a_private::CountMessages(
            afterClear, q1a_private::kClearRenderTargetNoOptimizedValue,
            q1a_private::kSeverityWarning));
    const int drawDelta821 =
        static_cast<int>(q1a_private::CountMessages(
            after, q1a_private::kClearDepthStencilNoOptimizedValue,
            q1a_private::kSeverityWarning)) -
        static_cast<int>(q1a_private::CountMessages(
            afterClear, q1a_private::kClearDepthStencilNoOptimizedValue,
            q1a_private::kSeverityWarning));
    q1a_test::Log("DIAGNOSTICS q1b_draw_clear 820_delta=%d 821_delta=%d depth_clear=%.1f",
                  drawDelta820, drawDelta821, static_cast<double>(kQ1bDepthClear));
    ok &= q1a_test::Require(
        drawDelta820 == 0,
        "Q1b draw added " + std::to_string(drawDelta820) +
            " ID-820 warning(s); the waiver is scoped to the Q1a row clear, so a "
            "colour attachment created without the first pass's clear value is a "
            "failure");
    ok &= q1a_test::Require(
        drawDelta821 == 0,
        "Q1b draw added " + std::to_string(drawDelta821) +
            " ID-821 warning(s); the depth attachment must be created with "
            "DrawFrameDesc::depthClear = " + std::to_string(kQ1bDepthClear) +
            " (the pre-fix creation value was 1.0f)");
    ok &= q1a_test::Require(after.errorCount == 0,
                            "debug layer reported errors during the diagnostic work");
    // Same cumulative allowance as after-clear: the Q1b phase may not add one
    // ID-820, so a report above that budget is unwaived.
    ok &= q1a_private::RequireNoUnwaivedMessages("after", after, clearBudget);
    if (validationSkipped) {
        // Keep the layer's own failure reason on the skipped row. On a host
        // without the SDK layer this is the documented 0x887A002D condition.
        q1a_test::Log("DIAGNOSTICS validation=SKIPPED debug=%s gpu_validation=%s "
                      "debug_detail=%s gpu_validation_detail=%s",
                      doom3::rhi::d3d12::ToString(requested.debugLayer),
                      doom3::rhi::d3d12::ToString(requested.gpuBasedValidation),
                      requested.debugLayerDetail.c_str(),
                      requested.gpuBasedValidationDetail.c_str());
    } else if (!requestedValidation) {
        q1a_test::Log("DIAGNOSTICS validation=NOT_REQUESTED debug=%s gpu_validation=%s",
                      doom3::rhi::d3d12::ToString(requested.debugLayer),
                      doom3::rhi::d3d12::ToString(requested.gpuBasedValidation));
    }

    const Status shutdown = device.Shutdown();
    ok &= q1a_test::Require(shutdown == Status::Ok,
                            "diagnostics device did not shut down cleanly");
    if (!ok) {
        return 1;
    }
    // Correct opt-in reporting is not a validation run. A requested layer that
    // came back SKIPPED must be reported as SKIPPED, not PASS, so the suite
    // cannot claim the layer executed when it did not.
    return validationSkipped ? q1a_test::kSkipExitCode : 0;
}

}  // namespace

int main() {
    struct Combination {
        bool debug;
        bool gpuValidation;
        const char* name;
    };
    const Combination combinations[] = {
        {false, false, "none"},
        {true, false, "debug_only"},
        {false, true, "gpu_validation_only"},
        {true, true, "both"},
    };
    int passes = 0;
    int failures = 0;
    int skips = 0;
    for (AdapterClass adapterClass : {AdapterClass::Hardware, AdapterClass::Software}) {
        for (const Combination& combination : combinations) {
            const int result = RunCase(adapterClass, combination.debug,
                                       combination.gpuValidation, combination.name);
            if (result == 0) {
                ++passes;
            } else if (result == q1a_test::kSkipExitCode) {
                ++skips;
            } else {
                ++failures;
            }
            q1a_test::Log("DIAGNOSTICS_CASE class=%s combo=%s result=%s",
                          doom3::rhi::ToString(adapterClass), combination.name,
                          result == 0 ? "PASS"
                                      : (result == q1a_test::kSkipExitCode ? "SKIPPED"
                                                                           : "FAIL"));
        }
    }
    // The evidence records 28 ID-820 warnings as 7 layer-active cases x 4 rows.
    // Reproduce the relation, not the two constants: every case that ran under
    // the layer must have contributed exactly one warning per Q1a row, and no
    // case may contribute any other number. The per-case assertion above already
    // enforces this; the total catches a future case that bypasses it.
    const int expectedTotal820 = static_cast<int>(q1a_test::kHeight) * gLayerActiveCases;
    q1a_test::Log("DIAGNOSTICS_SUMMARY cases_pass=%d fail=%d skip=%d "
                  "validation_executed=%d layer_active_cases=%d "
                  "clear_820_total=%d clear_820_expected=%d "
                  "(pass is a reporting/functional check, not a validation run; "
                  "skip means the layer was requested but did not run)",
                  passes, failures, skips, gValidationExecuted, gLayerActiveCases,
                  gClearTarget820Total, expectedTotal820);
    const bool totalOk = q1a_test::Require(
        gClearTarget820Total == expectedTotal820,
        "ID-820 total across layer-active cases was " +
            std::to_string(gClearTarget820Total) + ", expected " +
            std::to_string(expectedTotal820) + " (one per cleared row per case)");
    if (failures > 0 || !totalOk) {
        return 1;
    }
    if (passes > 0) {
        return 0;
    }
    return q1a_test::kSkipExitCode;
}
