#pragma once

// Reference accumulation, history rejection and a spatial denoise for Q6
// (spec 004 T068).
//
// A path-traced reference converges by accumulating independent samples. The
// temporal half reuses the Q2/Q3 correspondence vocabulary (`MotionValidity`)
// instead of inventing a second validity enum: a sample is blended only when
// the correspondence is Valid and no reset/generation/topology flag fired.
// A rejected sample replaces history instead of smearing it.
//
// The spatial denoiser is a documented edge-aware bilateral filter over linear
// radiance, with an optional depth guide. It is not a learned or vendor
// denoiser (Q5 is gated) and makes no interactive-performance claim.

#include "rhi/linear_color.h"
#include "rhi/motion_history.h"

#include <cstdint>
#include <string>
#include <vector>

namespace doom3 {
namespace rhi {

enum class HistoryDecision {
    Accept = 0,
    Reject,
};

const char* ToString(HistoryDecision value);

// Independent inputs; a caller cannot omit a check and still get Accept.
struct HistoryDecisionInput {
    MotionValidity validity = MotionValidity::NoHistory;
    bool generationMismatch = false;
    bool topologyChanged = false;
    bool reset = false;
    bool resized = false;
    bool providerSwitched = false;
    bool newPixel = false;
};

// Accept only for a Valid correspondence with every flag clear. `reason`
// receives a stable label when the decision is Reject.
HistoryDecision DecideHistory(const HistoryDecisionInput& input, std::string* reason);

struct ReferenceAccumulator {
    uint32_t width = 0;
    uint32_t height = 0;
    // Tightly packed linear RGB, 3 floats per pixel, and one count per pixel.
    std::vector<float> sumRgb;
    std::vector<uint32_t> samples;
};

bool ResetAccumulator(ReferenceAccumulator* accumulator, uint32_t width, uint32_t height,
                      std::string* error);

// Adds one RGB sample per pixel. A non-finite sample is refused and leaves the
// accumulator untouched.
bool AccumulateSample(ReferenceAccumulator* accumulator, const float* rgb, std::string* error);

// Mean of every pixel with at least one sample; a pixel with no sample resolves
// to zero and is reported by its count, not by a fabricated value.
bool ResolveAccumulator(const ReferenceAccumulator& accumulator, float* mean,
                        std::string* error);

uint64_t TotalSamples(const ReferenceAccumulator& accumulator);

// Measurement helpers used by the convergence and denoise fixtures. They are
// independent of the filter and are not used by the integrator.
float MeanLuminance(const float* rgb, uint32_t width, uint32_t height);
float VarianceLuminance(const float* rgb, uint32_t width, uint32_t height);

struct DenoiseDesc {
    uint32_t width = 0;
    uint32_t height = 0;
    // Window radius in pixels, 1..4 (3x3 .. 9x9).
    int radius = 2;
    // Linear-luminance tolerance. A larger value smooths more.
    float sigmaColor = 0.25f;
    // Depth tolerance in world units. Ignored when `depth` is null.
    float sigmaDepth = 0.05f;
};

// Edge-aware bilateral filter. `depth` may be null; when supplied it is one
// float per pixel and a depth discontinuity rejects the neighbour. `output`
// must not alias `rgb`. A non-finite input is refused.
bool DenoiseBilateral(const float* rgb, const float* depth, const DenoiseDesc& desc,
                      float* output, std::string* error);

// Accumulator with the history-rejection rule of `DecideHistory` applied per
// sample. Every pixel receives one sample per call, so the counts are uniform.
class TemporalAccumulator {
public:
    bool Reset(uint32_t width, uint32_t height, std::string* error);
    // Reject clears every pixel and starts again at the current sample.
    bool AddSample(const float* rgb, HistoryDecision decision, std::string* error);
    bool Resolve(float* mean, std::string* error) const;
    uint32_t width() const { return accumulator_.width; }
    uint32_t height() const { return accumulator_.height; }
    // Minimum per-pixel count; zero when no sample was added.
    uint32_t sampleCount() const;

private:
    ReferenceAccumulator accumulator_;
};

}  // namespace rhi
}  // namespace doom3
