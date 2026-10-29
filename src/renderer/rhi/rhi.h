#pragma once

// Portable RHI seam: Q1a clear/lifetime plus the Q1b programmable foundation.
//
// This header is the engine-facing core. It names no native API type, handle or
// synchronization value. The backend implementation lives outside this
// directory and keeps its own vocabulary.
//
// Q1a covers one offscreen synthetic clear, one row-pitched readback exposed as
// tightly packed caller bytes, an opaque completion token, and a release
// operation that refuses while the producing submission is still running.
//
// Q1b adds opaque shader modules, graphics and compute pipelines, an ordered
// draw frame with several colour attachments and a sampled depth attachment,
// compute dispatch with a buffer readback, bounded frame slots, and a window
// surface that composes and shows whole frames. Declared uses are described by
// the portable frame graph in frame_graph.h; no native state operation appears
// here.

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

// Result of the core release rule, not a GPU operation. A backend lowers it to
// its completion observation: Released only after the producing submission is
// proved complete, RefusedOutstanding instead of waiting or manufacturing
// success. A target without per-submission completion keeps the bookkeeping and
// never derives this result from a native value.
enum class ReleaseStatus {
    Released,
    RefusedOutstanding,
    InvalidSubmission,
};

// Adapter facts a backend may or may not be able to report. A backend that
// cannot produce a fact leaves its bit clear and the value at zero rather than
// inventing one; a client that requires the fact treats a clear bit as
// "unavailable", never as a real zero.
enum class AdapterFact : uint32_t {
    VendorId = 1u << 0,
    DeviceId = 1u << 1,
    DedicatedMemory = 1u << 2,
};

struct AdapterInfo {
    AdapterClass adapterClass = AdapterClass::Hardware;
    std::string name;
    // Bitmask of AdapterFact describing which fields below are reported. A
    // WebGPU-shaped backend that only exposes a vendor string reports no bits;
    // it must not translate that string into a fabricated numeric id.
    uint32_t reportedFacts = 0;
    uint32_t vendorId = 0;
    uint32_t deviceId = 0;
    uint64_t dedicatedVideoMemory = 0;
};

struct DeviceOptions {
    AdapterClass adapterClass = AdapterClass::Hardware;
    // Select the Nth adapter of the requested class. An out-of-range index is
    // an actionable Unsupported result and also exercises failed setup. The
    // portable core only guarantees index 0: a backend that cannot enumerate
    // (a WebGPU adapter request returns exactly one adapter) must return
    // Unsupported for any other index instead of silently using the first.
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

struct BufferResult {
    uint32_t elementCount = 0;
    uint32_t elementSize = 0;
    // Caller-owned tightly packed bytes, elementCount * elementSize in size.
    std::vector<uint8_t> bytes;
    AdapterInfo adapter;
};

// Opaque, device-scoped handles. Zero means "none"; a handle from another
// device or after a new Initialize is rejected rather than reinterpreted.
struct ShaderModule {
    uint32_t id = 0;
};

enum class ShaderStage {
    Vertex,
    Pixel,
    Compute,
};

struct ShaderModuleDesc {
    ShaderStage stage = ShaderStage::Vertex;
    // Opaque program text for the active backend's declared shader
    // capability. The portable seam does not define the language, compiler or
    // file format; a backend that cannot compile it fails with Unsupported.
    std::string source;
    std::string entryPoint = "main";
};

struct Pipeline {
    uint32_t id = 0;
};

struct GraphicsPipelineDesc {
    ShaderModule vertexShader;
    ShaderModule pixelShader;
    // Number of colour attachments the pixel program writes, 1..4.
    uint32_t colorAttachmentCount = 1;
    bool depthTest = false;
    bool depthWrite = false;
    // The pixel program reads the depth attachment as a sampled input.
    bool sampleDepth = false;
};

struct ComputePipelineDesc {
    ShaderModule computeShader;
};

// One ordered draw pass. `colorIndices` selects which frame attachments this
// pass writes; each attachment is written by exactly one pass. `colorClears`
// is parallel to `colorIndices` (entry i belongs to colorIndices[i]) and is
// applied when the pass begins.
struct DrawPassDesc {
    Pipeline pipeline;
    const uint32_t* colorIndices = nullptr;
    uint32_t colorCount = 0;
    const ClearRow* colorClears = nullptr;
    // One uniform colour multiplier uploaded for this pass. It is deliberately
    // per-submission data so frame-slot reuse can be checked for stale bytes.
    float tint[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct DrawFrameDesc {
    uint32_t width = 0;
    uint32_t height = 0;
    // Total distinct colour attachments in the frame, 1..4.
    uint32_t attachmentCount = 0;
    bool depthEnabled = false;
    // Q1b convention: the depth attachment is 32-bit float. A pass that sets
    // sampleDepth reads it at group 0, binding 1, with a point sampler at
    // group 0, binding 0.
    float depthClear = 1.0f;
    uint32_t passCount = 0;
    const DrawPassDesc* passes = nullptr;
};

struct ComputeDispatchDesc {
    Pipeline pipeline;
    // Dispatch grid. `groupsX * groupsY * groupsZ` workgroups are recorded.
    uint32_t groupsX = 0;
    uint32_t groupsY = 0;
    uint32_t groupsZ = 1;
    // Size of the writable result buffer in 32-bit elements. The program writes
    // it through group 0, binding 0; no other stride is defined by Q1b.
    uint32_t elementCount = 0;
};

// Opaque application-visible output surface. The backend owns the window and
// the composed images; a caller never receives a native handle.
struct Surface {
    uint32_t id = 0;
};

struct SurfaceDesc {
    std::string title;
    uint32_t width = 0;
    uint32_t height = 0;
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

    // Compiles and retains one program. Compilation failures are actionable and
    // leave no handle behind.
    Status CreateShaderModule(const ShaderModuleDesc& desc, ShaderModule* module,
                              std::string* error);
    Status CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, Pipeline* pipeline,
                                  std::string* error);
    Status CreateComputePipeline(const ComputePipelineDesc& desc, Pipeline* pipeline,
                                 std::string* error);

    // Records the ordered passes and readbacks, then returns without waiting.
    // Colour attachments are read back by index with ReadbackAttachment.
    Status SubmitDrawFrame(const DrawFrameDesc& desc, Submission* submission,
                           std::string* error);
    // Records one dispatch and a result-buffer readback.
    Status SubmitCompute(const ComputeDispatchDesc& desc, Submission* submission,
                         std::string* error);

    // The producing submission must be complete; NotReady is returned
    // otherwise. Attachment indices are the frame-wide indices declared in
    // DrawFrameDesc, and zero selects the single image of a clear submission.
    Status ReadbackAttachment(const Submission& submission, uint32_t attachmentIndex,
                              ReadbackResult* result, std::string* error);
    Status ReadbackBuffer(const Submission& submission, BufferResult* result,
                          std::string* error);

    // Creates an application-visible window surface at the requested size.
    Status CreateSurface(const SurfaceDesc& desc, Surface* surface, std::string* error);
    // Composes one solid-colour frame and shows it. A minimized surface returns
    // NotReady without recording work. The composed image is available through
    // ReadbackAttachment(submission, 0) after completion.
    Status SubmitSurfaceFrame(const Surface& surface, const ClearRow& color,
                              Submission* submission, std::string* error);
    Status ResizeSurface(const Surface& surface, uint32_t width, uint32_t height,
                         std::string* error);
    Status DestroySurface(const Surface& surface, std::string* error);

private:
    friend struct BackendAccess;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

const char* ToString(AdapterClass value);
const char* ToString(Status value);
const char* ToString(ReleaseStatus value);
const char* ToString(ShaderStage value);

}  // namespace rhi
}  // namespace doom3
