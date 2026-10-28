#pragma once

// D3D12-private diagnostic seam for Q1a.
//
// Nothing in this header is reachable from engine, scene or material code: it
// exists for the backend's own tests. It is the only place where recorded
// resource states, the state journal, fence values, staging row pitch and
// debug-layer/GPU-based-validation reporting are named.

#include "rhi/rhi.h"

#include <cstdint>
#include <string>
#include <vector>

namespace doom3 {
namespace rhi {
namespace d3d12 {

enum class CheckState {
    NotRequested,
    Run,
    Skipped,
    NotRun,
    Failed,
};

const char* ToString(CheckState value);

struct DiagnosticRequest {
    bool enableDebugLayer = false;
    bool enableGpuBasedValidation = false;
};

struct DiagnosticReport {
    CheckState debugLayer = CheckState::NotRequested;
    CheckState gpuBasedValidation = CheckState::NotRequested;
    std::string debugLayerDetail;
    std::string gpuBasedValidationDetail;
    uint32_t messageCount = 0;
    uint32_t errorCount = 0;
    uint32_t warningCount = 0;
    std::vector<std::string> messages;
};

// Recorded state sequence entry. Values are backend state names, not portable
// vocabulary.
struct JournalEntry {
    std::string resource;
    std::string from;
    std::string to;
    std::string reason;
};

struct RawReadback {
    uint32_t width = 0;
    uint32_t height = 0;
    // Backend staging stride. It may be larger than width * 4.
    uint32_t stagingPitch = 0;
    uint32_t tightRowStride = 0;
    std::vector<uint8_t> staging;
};

struct FenceObservation {
    uint64_t completedValue = 0;
    uint64_t signalValue = 0;
    // True when the backend reports its removed-device sentinel. A removed
    // device is not completion evidence and must not be read as progress.
    bool deviceRemoved = false;
};

}  // namespace d3d12

// Friend accessor implemented by the backend. Declared after the private
// vocabulary so its signatures do not need a second declaration site.
struct BackendAccess {
    static Status Initialize(Device& device, const DeviceOptions& options,
                             bool requestDebugLayer, bool requestGpuValidation,
                             std::string* error);
    static Status SubmitGated(Device& device, const ClearReadbackDesc& desc,
                              Submission* submission, std::string* error);
    static Status ReleaseGate(Device& device, const Submission& submission,
                              std::string* error);
    static d3d12::DiagnosticReport Diagnostics(Device& device);
    static std::vector<d3d12::JournalEntry> Journal(const Device& device,
                                                    const Submission& submission);
    static d3d12::RawReadback RawReadback(const Device& device,
                                          const Submission& submission,
                                          std::string* error);
    static d3d12::FenceObservation Observe(const Device& device,
                                           const Submission& submission);
};

namespace d3d12 {

// Initialize with opt-in backend validation requests. The public Initialize
// calls this with both requests false.
Status InitializeWithDiagnostics(Device& device, const DeviceOptions& options,
                                 const DiagnosticRequest& request, std::string* error);

// Submit the same Q1a work, but hold execution on a backend-private gate so a
// test can observe an incomplete completion token deterministically.
Status SubmitWithGate(Device& device, const ClearReadbackDesc& desc,
                      Submission* submission, std::string* error);
Status ReleaseGate(Device& device, const Submission& submission, std::string* error);

DiagnosticReport ReadDiagnostics(const Device& device);
std::vector<JournalEntry> ReadJournal(const Device& device, const Submission& submission);
RawReadback ReadRawReadback(const Device& device, const Submission& submission,
                            std::string* error);
FenceObservation ObserveFence(const Device& device, const Submission& submission);

}  // namespace d3d12
}  // namespace rhi
}  // namespace doom3
