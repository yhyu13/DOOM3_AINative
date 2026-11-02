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

// ---------------------------------------------------------------------------
// Q2 geometry and texture resources (T080/T081)
// ---------------------------------------------------------------------------
//
// A buffer is caller-supplied bytes in one of the two uses the classic ordered
// stream needs. A texture carries a full mip chain; block-compressed bytes are
// stored exactly as encoded (the seam does not define a block encoder). The
// formats are the Q4 content pins (BC7 sRGB base colour/emissive, BC5 linear
// normal XY, BC4 single channel) plus the uncompressed forms a synthetic
// fixture can write without an encoder.

struct Buffer {
    uint32_t id = 0;
};

enum class BufferUsage {
    Vertex,
    Index,
};

struct BufferDesc {
    BufferUsage usage = BufferUsage::Vertex;
    // Tightly packed resource bytes. Vertex buffers are read as `Vertex` when a
    // pipeline declares `vertexInput`; an index buffer is 16-bit unsigned.
    std::vector<uint8_t> bytes;
};

enum class TextureFormat {
    RGBA8Unorm = 0,
    RGBA8Srgb,
    R8Unorm,
    // Q4 pins: BC4_UNORM (roughness/metalness/ao), BC5_UNORM (normal XY),
    // BC7_UNORM_SRGB (base colour/emissive).
    BC4Unorm,
    BC5Unorm,
    BC7UnormSrgb,
    BC7Unorm,
};

struct TextureMip {
    uint32_t width = 0;
    uint32_t height = 0;
    // Tightly packed rows for uncompressed formats; tightly packed encoded
    // blocks (4x4) for block-compressed formats.
    std::vector<uint8_t> bytes;
};

struct TextureDesc {
    TextureFormat format = TextureFormat::RGBA8Unorm;
    // Mip 0 first. Every mip after the first is half the previous size, with a
    // minimum of one texel (4x4 blocks for block-compressed formats).
    std::vector<TextureMip> mips;
};

struct Texture {
    uint32_t id = 0;
};

// The one vertex layout the classic ordered stream submits. A pipeline that
// declares `vertexInput` reads exactly this layout.
struct Vertex {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float texCoord[2] = {0.0f, 0.0f};
};

enum class CullMode {
    None = 0,
    Front,
    Back,
};

enum class CompareFunc {
    Never = 0,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
};

enum class StencilOp {
    Keep = 0,
    Zero,
    Replace,
    IncrementSaturate,
    DecrementSaturate,
    Invert,
    IncrementWrap,
    DecrementWrap,
};

struct StencilFaceState {
    CompareFunc func = CompareFunc::Always;
    StencilOp failStencil = StencilOp::Keep;
    StencilOp failDepth = StencilOp::Keep;
    StencilOp pass = StencilOp::Keep;
};

struct StencilState {
    bool enabled = false;
    uint8_t readMask = 0xFF;
    uint8_t writeMask = 0xFF;
    StencilFaceState front;
    StencilFaceState back;
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
    // Number of colour attachments the pixel program writes, 0..4. Zero is the
    // stencil-only shadow-volume pass, which writes no colour.
    uint32_t colorAttachmentCount = 1;
    bool depthTest = false;
    bool depthWrite = false;
    // The pixel program reads the depth attachment as a sampled input.
    bool sampleDepth = false;
    // Reads position/texCoord from a vertex buffer described by `Vertex`
    // instead of the Q1b procedural fullscreen triangle.
    bool vertexInput = false;
    // The pixel program samples the draw item's texture at register t0 through
    // the static point sampler at s0.
    bool textured = false;
    CullMode cullMode = CullMode::None;
    // A stencil-enabled pipeline uses the depth-stencil attachment format, so
    // every pipeline in that frame must also enable stencil (a pipeline may
    // still keep every op at Keep with a zero write mask).
    StencilState stencil;
};

struct ComputePipelineDesc {
    ShaderModule computeShader;
};

// One ordered draw. A pipeline with `vertexInput` consumes its vertex/index
// buffers; the item transform is applied by the vertex program and `tint` is
// the per-item colour multiplier. `texture` is sampled when the pipeline
// declares `textured`. `stencilRef` is the comparison reference for a
// stencil-enabled pipeline.
struct DrawItem {
    Buffer vertexBuffer;
    uint32_t vertexCount = 0;
    // Optional. When `indexCount` is non-zero the backend issues an indexed
    // draw through this buffer.
    Buffer indexBuffer;
    uint32_t indexCount = 0;
    Texture texture;
    float tint[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    // Column-major model-to-clip transform.
    float transform[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                           0.0f, 1.0f, 0.0f, 0.0f,
                           0.0f, 0.0f, 1.0f, 0.0f,
                           0.0f, 0.0f, 0.0f, 1.0f};
    uint8_t stencilRef = 0;
};

// One ordered draw pass. `colorIndices` selects which frame attachments this
// pass writes; each attachment is written by exactly one pass. `colorClears`
// is parallel to `colorIndices` (entry i belongs to colorIndices[i]) and is
// applied when the pass begins. A stencil-only pass has `colorCount == 0` and
// no colour targets; its pipeline must also declare zero colour attachments.
struct DrawPassDesc {
    Pipeline pipeline;
    const uint32_t* colorIndices = nullptr;
    uint32_t colorCount = 0;
    const ClearRow* colorClears = nullptr;
    // Ordered item list for a `vertexInput` pipeline. Empty keeps the Q1b
    // procedural fullscreen-triangle behaviour and is rejected otherwise.
    const DrawItem* items = nullptr;
    uint32_t itemCount = 0;
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

    // Creates a device-scoped resource and uploads its bytes. A texture carries
    // a full mip chain. Creation records no frame work; a created handle may be
    // named by any later DrawItem.
    Status CreateBuffer(const BufferDesc& desc, Buffer* buffer, std::string* error);
    Status CreateTexture(const TextureDesc& desc, Texture* texture, std::string* error);
    // Drops the caller handle. The backend keeps the object until no recorded
    // submission can still reference it and returns NotReady while one can.
    Status DestroyBuffer(const Buffer& buffer, std::string* error);
    Status DestroyTexture(const Texture& texture, std::string* error);

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
