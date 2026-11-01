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
    frame.depthClear = 0.5f;
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
    }
    q1a_test::Log("DIAGNOSTICS validation_executed=%s",
                  validationExecuted ? "true" : "false");
    q1a_private::LogDiagnosticMessages("requested", requested);
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
    ok &= q1a_test::Require(after.errorCount == 0,
                            "debug layer reported errors during the diagnostic work");
    ok &= q1a_private::RequireNoUnwaivedMessages("after", after);
    if (!validationExecuted) {
        q1a_test::Log("DIAGNOSTICS validation=SKIPPED debug=%s gpu_validation=%s",
                      doom3::rhi::d3d12::ToString(requested.debugLayer),
                      doom3::rhi::d3d12::ToString(requested.gpuBasedValidation));
    }

    const Status shutdown = device.Shutdown();
    ok &= q1a_test::Require(shutdown == Status::Ok,
                            "diagnostics device did not shut down cleanly");
    return ok ? 0 : 1;
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
    q1a_test::Log("DIAGNOSTICS_SUMMARY cases_pass=%d fail=%d skip=%d "
                  "validation_executed=%d (a case pass is opt-in reporting, not a "
                  "validation run)",
                  passes, failures, skips, gValidationExecuted);
    if (failures > 0) {
        return 1;
    }
    if (passes > 0) {
        return 0;
    }
    return q1a_test::kSkipExitCode;
}
