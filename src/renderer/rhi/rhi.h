#pragma once

// Q1a portable RHI seam.
//
// This header is the engine-facing core for the first vertical slice. It names
// no native API type, handle or synchronization value. The backend
// implementation lives outside this directory and keeps its own vocabulary.
//
// Q1a is deliberately small: one offscreen synthetic clear, one row-pitched
// readback exposed as tightly packed caller bytes, an opaque completion token,
// and a release operation that refuses while the producing submission is still
// running. It is not a renderer, a frame graph or the full Q1 contract.

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
    NotReady,
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

struct AdapterInfo {
    AdapterClass adapterClass = AdapterClass::Hardware;
    std::string name;
    uint32_t vendorId = 0;
    uint32_t deviceId = 0;
    uint64_t dedicatedVideoMemory = 0;
};

struct DeviceOptions {
    AdapterClass adapterClass = AdapterClass::Hardware;
    // Select the Nth adapter of the requested class. An out-of-range index is
    // an actionable Unsupported result and also exercises failed setup.
    uint32_t adapterIndex = 0;
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
    // Exactly one entry per row. The test supplies literal channel values; the
    // backend must clear each row from this description and must not derive
    // expected bytes from anything it later reads back.
    std::vector<ClearRow> rows;
};

// An opaque, device-scoped completion token. It is not a native synchronization
// value, is not comparable across devices, and must not be inspected to infer
// backend progress. Zero means "no submission".
struct Submission {
    uint32_t id = 0;
    uint64_t completionToken = 0;
};

struct ReadbackResult {
    uint32_t width = 0;
    uint32_t height = 0;
    // Bytes per tightly packed row: width * 4 for RGBA8. Backend padding is
    // internal and never appears here.
    uint32_t rowStride = 0;
    // Caller-owned tightly packed bytes, height * rowStride in size.
    std::vector<uint8_t> pixels;
    AdapterInfo adapter;
};

class Device {
public:
    Device();
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    // Enables the selected adapter and backend. Failures carry an actionable
    // reason. This does not run any GPU work.
    Status Initialize(const DeviceOptions& options, std::string* error);

    // Idempotent. Completes or reports unproven outstanding work, then drops
    // backend objects. It returns Error or DeviceLost instead of claiming a
    // clean teardown when ownership cannot be proved.
    Status Shutdown();

    bool IsInitialized() const;
    const AdapterInfo& adapter() const;

    // Records the synthetic row clears, the copy and the completion signal,
    // then returns without waiting. The caller observes progress only through
    // the completion token.
    Status SubmitClearReadback(const ClearReadbackDesc& desc, Submission* submission,
                               std::string* error);

    // Polls the completion token. An unknown, released or foreign submission
    // returns false.
    bool IsComplete(const Submission& submission) const;

    // Copies tightly packed caller-owned bytes. The producing submission must
    // be complete; NotReady is returned otherwise. Readback never exposes
    // backend row padding or a mapped view.
    Status Readback(const Submission& submission, ReadbackResult* result,
                    std::string* error);

    // Releases the recording and resources only after the completion token has
    // been reached. It must refuse while the submission is still outstanding;
    // it must not wait first.
    ReleaseStatus TryRelease(const Submission& submission, std::string* error);

private:
    friend struct BackendAccess;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

const char* ToString(AdapterClass value);
const char* ToString(Status value);
const char* ToString(ReleaseStatus value);

}  // namespace rhi
}  // namespace doom3
