#pragma once

// Q1a public RHI seam.
//
// This header is deliberately backend-neutral: it does not include Windows,
// D3D12, DXGI or any native handle. The D3D12 implementation lives in
// src/renderer/d3d12/rhi_d3d12.cpp. Completion values are opaque backend
// serials, not a promise that every backend owns a fence object.
//
// Q1a is intentionally small. It proves one offscreen clear, one transition
// to a copy source, one row-pitched readback and one fence-safe retirement
// path. It is not a renderer, a frame graph or the full Q1 contract.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace doom3 {
namespace rhi {

enum class AdapterClass {
    Hardware,
    Software,
};

enum class Status {
    Ok,
    InvalidArgument,
    Unsupported,
    DeviceLost,
    OutOfMemory,
    Error,
};

enum class ReleaseStatus {
    Released,
    RefusedOutstanding,
    InvalidSubmission,
};

enum class ResourceState {
    Unknown,
    RenderTarget,
    CopySource,
    CopyDest,
    Common,
};

enum class DiagnosticState {
    NotRequested,
    Run,
    Skipped,
    NotRun,
    Failed,
};

struct AdapterInfo {
    AdapterClass adapterClass = AdapterClass::Hardware;
    std::string name;
    uint32_t vendorId = 0;
    uint32_t deviceId = 0;
    uint64_t dedicatedVideoMemory = 0;
    uint64_t luid = 0;
};

struct DeviceOptions {
    AdapterClass adapterClass = AdapterClass::Hardware;
    // Select the Nth adapter of the requested class. Out-of-range is an
    // actionable Unsupported result, which also exercises incomplete-init
    // shutdown.
    uint32_t adapterIndex = 0;
    bool enableDebugLayer = false;
    bool enableGpuBasedValidation = false;
};

struct ClearRow {
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    uint8_t alpha = 0;
};

struct ClearReadbackDesc {
    uint32_t width = 4;
    uint32_t height = 4;
    // One entry per row. The test supplies hard-coded RGB values; the backend
    // clears each row through a D3D12 clear rect and must not synthesize the
    // expected bytes from the readback.
    std::vector<ClearRow> rows;
};

struct SubmissionOptions {
    // Test/diagnostic gate. When true, the backend enqueues a GPU wait before
    // the submission so the CPU can deterministically observe an unsignalled
    // completion while it attempts to release resources.
    bool waitForCpuGate = false;
};

struct Submission {
    uint32_t id = 0;
    // Backend completion serial. On D3D12 this is the queue fence value.
    uint64_t completionSerial = 0;
    bool gated = false;
};

struct ReadbackResult {
    uint32_t width = 0;
    uint32_t height = 0;
    // GPU staging row pitch returned by the backend. It may be larger than
    // width * 4 and must be used to walk the staging bytes.
    uint32_t rowPitch = 0;
    uint32_t tightRowPitch = 0;
    // Caller-owned tightly packed copy, height * tightRowPitch bytes.
    std::vector<uint8_t> pixels;
    // Caller-owned diagnostic copy of the raw row-pitched staging bytes,
    // height * rowPitch bytes. Bytes beyond each row's tightRowPitch are
    // padding and are not guaranteed to have been written. Q1a uses this to
    // make the wrong-stride and off-by-one-row negative controls real rather
    // than simulated.
    std::vector<uint8_t> staging;
    AdapterInfo adapter;
};

struct StateTransition {
    std::string resource;
    ResourceState from = ResourceState::Unknown;
    ResourceState to = ResourceState::Unknown;
    std::string reason;
};

struct Diagnostics {
    DiagnosticState debugLayer = DiagnosticState::NotRequested;
    DiagnosticState gpuBasedValidation = DiagnosticState::NotRequested;
    std::string debugLayerDetail;
    std::string gpuBasedValidationDetail;
    uint32_t messageCount = 0;
    uint32_t errorCount = 0;
    uint32_t warningCount = 0;
    // Bounded message text captured from ID3D12InfoQueue for the retained test
    // log. Empty when the debug layer is unavailable or no message was stored;
    // messageCount still reports the full number of stored messages.
    std::vector<std::string> messages;
};

class Device {
public:
    Device();
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    // Enables the selected adapter and backend. A requested debug layer or
    // GPU-based validation that is unavailable is recorded in diagnostics and
    // does not silently turn into a pass.
    Status Initialize(const DeviceOptions& options, std::string* error);

    // Idempotent. Outstanding gated submissions are unblocked, completed and
    // released before native objects are dropped. Returns Error when an
    // outstanding serial did not complete within the backend timeout; the
    // references are still dropped because this is test-process teardown.
    Status Shutdown();

    bool IsInitialized() const;
    const AdapterInfo& adapter() const;
    const std::vector<StateTransition>& stateJournal() const;
    const Diagnostics& diagnostics() const;

    // Records the row clears, the RENDER_TARGET -> COPY_SOURCE transition,
    // the readback COPY_DEST placement, submits the command list and signals
    // the completion serial. It does not wait or read back.
    Status SubmitClearReadback(const ClearReadbackDesc& desc,
                               const SubmissionOptions& options,
                               Submission* submission,
                               std::string* error);

    // Releases the CPU gate for a gated submission. Calling this for an
    // ungated submission is an InvalidArgument.
    Status ReleaseGate(const Submission& submission, std::string* error);

    // Waits for the completion serial. A gated submission must have its gate
    // released first; otherwise this returns InvalidArgument rather than
    // deadlocking the test process.
    Status Wait(const Submission& submission, std::string* error);

    // Copies the staging buffer into caller-owned bytes. The completion must
    // already have been waited for.
    Status Readback(const Submission& submission, ReadbackResult* result,
                    std::string* error);

    // Attempts to release the command allocator and resources. It must refuse
    // while the completion serial is still outstanding.
    ReleaseStatus TryRelease(const Submission& submission, std::string* error);

    // Backend completion serial already reached, or zero for an invalid
    // submission. Used by the Q1a lifetime test to log both values.
    uint64_t CompletedSerial(const Submission& submission) const;
    bool IsCompleted(const Submission& submission) const;

    // Convenience path used by the functional fixture: submit, wait, read
    // back and release one submission.
    Status ClearAndReadback(const ClearReadbackDesc& desc,
                            ReadbackResult* result,
                            std::string* error);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

const char* ToString(AdapterClass value);
const char* ToString(Status value);
const char* ToString(ReleaseStatus value);
const char* ToString(ResourceState value);
const char* ToString(DiagnosticState value);

}  // namespace rhi
}  // namespace doom3
