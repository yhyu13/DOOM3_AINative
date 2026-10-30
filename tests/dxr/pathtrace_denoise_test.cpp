// Q6 denoising, history rejection and accumulation (spec 004 T068).
//
// The history rule reuses the Q2/Q3 `MotionValidity`, so a disocclusion or a
// generation change must reject the previous sample instead of blending it.
// The bilateral filter is checked for the two properties that matter here:
// variance falls on a flat noisy region, and a colour or depth edge is not
// smeared. The expected means are computed in the test from the input array.

#include "dxr_test_support.h"

#include "rhi/pathtrace_denoise.h"

#include <cmath>
#include <string>
#include <vector>

using doom3::rhi::AccumulateSample;
using doom3::rhi::DecideHistory;
using doom3::rhi::DenoiseBilateral;
using doom3::rhi::DenoiseDesc;
using doom3::rhi::HistoryDecision;
using doom3::rhi::HistoryDecisionInput;
using doom3::rhi::MeanLuminance;
using doom3::rhi::MotionValidity;
using doom3::rhi::ReferenceAccumulator;
using doom3::rhi::ResetAccumulator;
using doom3::rhi::ResolveAccumulator;
using doom3::rhi::TemporalAccumulator;
using doom3::rhi::TotalSamples;
using doom3::rhi::VarianceLuminance;

namespace {

void Fill(std::vector<float>* image, uint32_t width, uint32_t height, float value) {
    image->assign(static_cast<size_t>(width) * height * 3, value);
}

void FillNoise(std::vector<float>* image, uint32_t width, uint32_t height, float base,
               float amplitude, uint32_t seed) {
    image->resize(static_cast<size_t>(width) * height * 3);
    uint32_t state = seed;
    for (size_t index = 0; index < static_cast<size_t>(width) * height; ++index) {
        state = state * 1664525u + 1013904223u;
        const float unit = static_cast<float>(state >> 8) / 16777216.0f;
        const float value = base + (unit * 2.0f - 1.0f) * amplitude;
        (*image)[index * 3 + 0] = value;
        (*image)[index * 3 + 1] = value;
        (*image)[index * 3 + 2] = value;
    }
}

float PixelLuma(const std::vector<float>& image, uint32_t width, uint32_t x, uint32_t y) {
    const size_t index = (static_cast<size_t>(y) * width + x) * 3;
    return dxr_test::Luma(&image[index]);
}

}  // namespace

int main() {
    dxr_test::Runner runner;

    // --- Reference accumulation --------------------------------------------
    {
        ReferenceAccumulator accumulator;
        std::string error;
        runner.Check(ResetAccumulator(&accumulator, 2, 1, &error), "2x1 accumulator resets");
        const float first[6] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
        const float second[6] = {3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
        runner.Check(AccumulateSample(&accumulator, first, &error), "the first sample accumulates");
        runner.Check(AccumulateSample(&accumulator, second, &error),
                     "the second sample accumulates");
        float mean[6] = {};
        runner.Check(ResolveAccumulator(accumulator, mean, &error), "the mean resolves");
        runner.Near(mean[0], 2.0f, 1.0e-6f, "first pixel red mean");
        runner.Near(mean[1], 3.0f, 1.0e-6f, "first pixel green mean");
        runner.Near(mean[5], 7.0f, 1.0e-6f, "second pixel blue mean");
        runner.Check(TotalSamples(accumulator) == 4, "total samples counts every pixel and sample");

        const float invalid[6] = {1.0f, 1.0f, 1.0f, std::nanf(""), 1.0f, 1.0f};
        runner.Check(!AccumulateSample(&accumulator, invalid, &error),
                     "a non-finite sample is refused");
        runner.Check(TotalSamples(accumulator) == 4,
                     "a refused sample does not change the accumulator");
        runner.Check(!ResetAccumulator(&accumulator, 0, 1, &error),
                     "a zero extent is refused");
    }

    // --- History decision ---------------------------------------------------
    {
        HistoryDecisionInput input;
        input.validity = MotionValidity::Valid;
        std::string reason;
        runner.Check(DecideHistory(input, &reason) == HistoryDecision::Accept,
                     "a valid correspondence accepts history");
        runner.Check(reason.empty(), "an accept has no rejection reason");

        input.validity = MotionValidity::Disocclusion;
        runner.Check(DecideHistory(input, &reason) == HistoryDecision::Reject,
                     "a disocclusion rejects history");
        runner.Check(!reason.empty(), "a rejection explains itself");

        input.validity = MotionValidity::Valid;
        input.generationMismatch = true;
        runner.Check(DecideHistory(input, &reason) == HistoryDecision::Reject,
                     "a generation mismatch rejects history");
        input.generationMismatch = false;
        input.topologyChanged = true;
        runner.Check(DecideHistory(input, &reason) == HistoryDecision::Reject,
                     "a topology change rejects history");
        input.topologyChanged = false;
        input.reset = true;
        runner.Check(DecideHistory(input, &reason) == HistoryDecision::Reject,
                     "a reset rejects history");
        input.reset = false;
        input.resized = true;
        runner.Check(DecideHistory(input, &reason) == HistoryDecision::Reject,
                     "a resize rejects history");
        input.resized = false;
        input.providerSwitched = true;
        runner.Check(DecideHistory(input, &reason) == HistoryDecision::Reject,
                     "a provider switch rejects history");
        input.providerSwitched = false;
        input.newPixel = true;
        runner.Check(DecideHistory(input, &reason) == HistoryDecision::Reject,
                     "a new pixel rejects history");
    }

    // --- Temporal accumulator ----------------------------------------------
    {
        TemporalAccumulator accumulator;
        std::string error;
        runner.Check(accumulator.Reset(2, 1, &error), "temporal accumulator resets");
        const float sample[6] = {0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f};
        for (int index = 0; index < 4; ++index) {
            accumulator.AddSample(sample, HistoryDecision::Accept, &error);
        }
        runner.Check(accumulator.sampleCount() == 4, "four accepted samples are counted");
        float mean[6] = {};
        runner.Check(accumulator.Resolve(mean, &error), "the temporal mean resolves");
        runner.Near(mean[0], 0.5f, 1.0e-6f, "accepted samples average");

        const float replacement[6] = {0.25f, 0.25f, 0.25f, 0.75f, 0.75f, 0.75f};
        accumulator.AddSample(replacement, HistoryDecision::Reject, &error);
        runner.Check(accumulator.sampleCount() == 1,
                     "a rejected sample restarts the count instead of blending");
        runner.Check(accumulator.Resolve(mean, &error), "the rejected history resolves");
        runner.Near(mean[0], 0.25f, 1.0e-6f, "a rejected sample replaces history");
    }

    // --- Bilateral filter: flat region -------------------------------------
    {
        const uint32_t width = 8;
        const uint32_t height = 8;
        std::vector<float> flat;
        Fill(&flat, width, height, 0.5f);
        std::vector<float> output;
        Fill(&output, width, height, 0.0f);
        DenoiseDesc desc;
        desc.width = width;
        desc.height = height;
        desc.radius = 2;
        std::string error;
        runner.Check(DenoiseBilateral(flat.data(), nullptr, desc, output.data(), &error),
                     "a flat image denoises");
        runner.Near(MeanLuminance(output.data(), width, height), 0.5f, 1.0e-6f,
                    "a flat image keeps its value");
        runner.Near(VarianceLuminance(output.data(), width, height), 0.0f, 1.0e-6f,
                    "a flat image stays flat");
    }

    {
        const uint32_t width = 16;
        const uint32_t height = 16;
        std::vector<float> noisy;
        FillNoise(&noisy, width, height, 0.5f, 0.05f, 7u);
        std::vector<float> output;
        Fill(&output, width, height, 0.0f);
        DenoiseDesc desc;
        desc.width = width;
        desc.height = height;
        desc.radius = 2;
        desc.sigmaColor = 0.25f;
        std::string error;
        runner.Check(DenoiseBilateral(noisy.data(), nullptr, desc, output.data(), &error),
                     "a noisy flat image denoises");
        const float before = VarianceLuminance(noisy.data(), width, height);
        const float after = VarianceLuminance(output.data(), width, height);
        runner.Check(after < before * 0.5f, "the filter reduces variance on a flat region");
        runner.Near(MeanLuminance(output.data(), width, height),
                    MeanLuminance(noisy.data(), width, height), 0.01f,
                    "the filter keeps the flat-region mean");
    }

    // --- Bilateral filter: colour edge -------------------------------------
    {
        const uint32_t width = 8;
        const uint32_t height = 1;
        std::vector<float> edge;
        Fill(&edge, width, height, 0.0f);
        for (uint32_t x = 4; x < width; ++x) {
            for (uint32_t channel = 0; channel < 3; ++channel) {
                edge[x * 3 + channel] = 2.0f;
            }
        }
        std::vector<float> output;
        Fill(&output, width, height, 0.0f);
        DenoiseDesc desc;
        desc.width = width;
        desc.height = height;
        desc.radius = 2;
        desc.sigmaColor = 0.15f;
        std::string error;
        runner.Check(DenoiseBilateral(edge.data(), nullptr, desc, output.data(), &error),
                     "a colour edge denoises");
        runner.Near(PixelLuma(output, width, 2, 0), 0.0f, 1.0e-4f,
                    "the dark side of a colour edge is preserved");
        runner.Near(PixelLuma(output, width, 5, 0), 2.0f, 1.0e-4f,
                    "the bright side of a colour edge is preserved");
    }

    // --- Bilateral filter: depth guide -------------------------------------
    {
        const uint32_t width = 8;
        const uint32_t height = 1;
        std::vector<float> image;
        FillNoise(&image, width, height, 0.3f, 0.05f, 11u);
        for (uint32_t x = 4; x < width; ++x) {
            for (uint32_t channel = 0; channel < 3; ++channel) {
                image[x * 3 + channel] = 0.7f;
            }
        }
        std::vector<float> depth(width, 1.0f);
        for (uint32_t x = 4; x < width; ++x) {
            depth[x] = 5.0f;
        }
        std::vector<float> withoutDepth;
        std::vector<float> withDepth;
        Fill(&withoutDepth, width, height, 0.0f);
        Fill(&withDepth, width, height, 0.0f);
        DenoiseDesc desc;
        desc.width = width;
        desc.height = height;
        desc.radius = 2;
        desc.sigmaColor = 10.0f;
        desc.sigmaDepth = 0.05f;
        std::string error;
        runner.Check(DenoiseBilateral(image.data(), nullptr, desc, withoutDepth.data(), &error),
                     "the depth-free filter runs");
        runner.Check(DenoiseBilateral(image.data(), depth.data(), desc, withDepth.data(), &error),
                     "the depth-guided filter runs");
        runner.Check(PixelLuma(withDepth, width, 3, 0) < 0.35f,
                     "the depth guide keeps the near side from bleeding");
        runner.Check(PixelLuma(withoutDepth, width, 3, 0) > 0.38f,
                     "without a depth guide the sides blend");
        runner.Check(PixelLuma(withDepth, width, 4, 0) > 0.65f,
                     "the depth guide keeps the far side from bleeding");
    }

    // --- Invalid denoise inputs --------------------------------------------
    {
        std::vector<float> image(8 * 3, 0.5f);
        std::vector<float> output(8 * 3, 0.0f);
        DenoiseDesc desc;
        desc.width = 8;
        desc.height = 1;
        std::string error;
        desc.radius = 0;
        runner.Check(!DenoiseBilateral(image.data(), nullptr, desc, output.data(), &error),
                     "a zero radius is refused");
        desc.radius = 2;
        image[2] = std::nanf("");
        runner.Check(!DenoiseBilateral(image.data(), nullptr, desc, output.data(), &error),
                     "a non-finite input is refused");
        image[2] = 0.5f;
        runner.Check(!DenoiseBilateral(image.data(), nullptr, desc, image.data(), &error),
                     "the output may not alias the input");
    }

    return runner.Finish("q6_pathtrace_denoise_test");
}
