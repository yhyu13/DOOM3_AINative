// Reference accumulation, history rejection and bilateral denoise (spec 004
// T068). Backend-neutral arithmetic; no device, no learned model, no vendor
// denoiser.

#include "rhi/pathtrace_denoise.h"

#include <algorithm>
#include <cmath>

namespace doom3 {
namespace rhi {

namespace {

bool IsFiniteRgb(const float* rgb, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (!std::isfinite(rgb[i])) {
            return false;
        }
    }
    return true;
}

float Luma(const float* rgb) { return 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2]; }

}  // namespace

const char* ToString(HistoryDecision value) {
    switch (value) {
        case HistoryDecision::Accept: return "Accept";
        case HistoryDecision::Reject: return "Reject";
    }
    return "Unknown";
}

HistoryDecision DecideHistory(const HistoryDecisionInput& input, std::string* reason) {
    if (input.validity != MotionValidity::Valid) {
        if (reason != nullptr) {
            *reason = std::string("correspondence ") + ToString(input.validity);
        }
        return HistoryDecision::Reject;
    }
    if (input.generationMismatch) {
        if (reason != nullptr) {
            *reason = "generation mismatch";
        }
        return HistoryDecision::Reject;
    }
    if (input.topologyChanged) {
        if (reason != nullptr) {
            *reason = "topology changed";
        }
        return HistoryDecision::Reject;
    }
    if (input.reset) {
        if (reason != nullptr) {
            *reason = "history reset";
        }
        return HistoryDecision::Reject;
    }
    if (input.resized) {
        if (reason != nullptr) {
            *reason = "extent changed";
        }
        return HistoryDecision::Reject;
    }
    if (input.providerSwitched) {
        if (reason != nullptr) {
            *reason = "provider switched";
        }
        return HistoryDecision::Reject;
    }
    if (input.newPixel) {
        if (reason != nullptr) {
            *reason = "new pixel";
        }
        return HistoryDecision::Reject;
    }
    if (reason != nullptr) {
        reason->clear();
    }
    return HistoryDecision::Accept;
}

bool ResetAccumulator(ReferenceAccumulator* accumulator, uint32_t width, uint32_t height,
                      std::string* error) {
    if (accumulator == nullptr) {
        if (error != nullptr) {
            *error = "ResetAccumulator needs an accumulator";
        }
        return false;
    }
    if (width == 0 || height == 0) {
        if (error != nullptr) {
            *error = "accumulator extent must be non-zero";
        }
        return false;
    }
    accumulator->width = width;
    accumulator->height = height;
    const size_t pixels = static_cast<size_t>(width) * height;
    accumulator->sumRgb.assign(pixels * 3, 0.0f);
    accumulator->samples.assign(pixels, 0);
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool AccumulateSample(ReferenceAccumulator* accumulator, const float* rgb, std::string* error) {
    if (accumulator == nullptr || rgb == nullptr) {
        if (error != nullptr) {
            *error = "AccumulateSample needs an accumulator and a sample";
        }
        return false;
    }
    if (accumulator->width == 0 || accumulator->height == 0 ||
        accumulator->sumRgb.size() !=
            static_cast<size_t>(accumulator->width) * accumulator->height * 3 ||
        accumulator->samples.size() !=
            static_cast<size_t>(accumulator->width) * accumulator->height) {
        if (error != nullptr) {
            *error = "accumulator storage does not match its extent";
        }
        return false;
    }
    const size_t floats = accumulator->sumRgb.size();
    if (!IsFiniteRgb(rgb, floats)) {
        if (error != nullptr) {
            *error = "sample contains a non-finite value";
        }
        return false;
    }
    for (size_t i = 0; i < floats; ++i) {
        accumulator->sumRgb[i] += rgb[i];
    }
    for (size_t i = 0; i < accumulator->samples.size(); ++i) {
        ++accumulator->samples[i];
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool ResolveAccumulator(const ReferenceAccumulator& accumulator, float* mean,
                        std::string* error) {
    if (mean == nullptr) {
        if (error != nullptr) {
            *error = "ResolveAccumulator needs an output buffer";
        }
        return false;
    }
    if (accumulator.width == 0 || accumulator.height == 0 ||
        accumulator.sumRgb.size() !=
            static_cast<size_t>(accumulator.width) * accumulator.height * 3 ||
        accumulator.samples.size() !=
            static_cast<size_t>(accumulator.width) * accumulator.height) {
        if (error != nullptr) {
            *error = "accumulator storage does not match its extent";
        }
        return false;
    }
    const size_t pixels = accumulator.samples.size();
    for (size_t pixel = 0; pixel < pixels; ++pixel) {
        const uint32_t count = accumulator.samples[pixel];
        const float inverse = count > 0 ? 1.0f / static_cast<float>(count) : 0.0f;
        mean[pixel * 3 + 0] = accumulator.sumRgb[pixel * 3 + 0] * inverse;
        mean[pixel * 3 + 1] = accumulator.sumRgb[pixel * 3 + 1] * inverse;
        mean[pixel * 3 + 2] = accumulator.sumRgb[pixel * 3 + 2] * inverse;
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

uint64_t TotalSamples(const ReferenceAccumulator& accumulator) {
    uint64_t total = 0;
    for (uint32_t count : accumulator.samples) {
        total += count;
    }
    return total;
}

float MeanLuminance(const float* rgb, uint32_t width, uint32_t height) {
    if (rgb == nullptr || width == 0 || height == 0) {
        return 0.0f;
    }
    const size_t pixels = static_cast<size_t>(width) * height;
    double sum = 0.0;
    for (size_t pixel = 0; pixel < pixels; ++pixel) {
        sum += static_cast<double>(Luma(rgb + pixel * 3));
    }
    return static_cast<float>(sum / static_cast<double>(pixels));
}

float VarianceLuminance(const float* rgb, uint32_t width, uint32_t height) {
    if (rgb == nullptr || width == 0 || height == 0) {
        return 0.0f;
    }
    const size_t pixels = static_cast<size_t>(width) * height;
    double sum = 0.0;
    for (size_t pixel = 0; pixel < pixels; ++pixel) {
        sum += static_cast<double>(Luma(rgb + pixel * 3));
    }
    const double mean = sum / static_cast<double>(pixels);
    double variance = 0.0;
    for (size_t pixel = 0; pixel < pixels; ++pixel) {
        const double difference = static_cast<double>(Luma(rgb + pixel * 3)) - mean;
        variance += difference * difference;
    }
    return static_cast<float>(variance / static_cast<double>(pixels));
}

bool DenoiseBilateral(const float* rgb, const float* depth, const DenoiseDesc& desc,
                      float* output, std::string* error) {
    if (rgb == nullptr || output == nullptr) {
        if (error != nullptr) {
            *error = "DenoiseBilateral needs input and output buffers";
        }
        return false;
    }
    if (desc.width == 0 || desc.height == 0) {
        if (error != nullptr) {
            *error = "denoise extent must be non-zero";
        }
        return false;
    }
    if (desc.radius < 1 || desc.radius > 4) {
        if (error != nullptr) {
            *error = "denoise radius must be in 1..4";
        }
        return false;
    }
    if (!std::isfinite(desc.sigmaColor) || !(desc.sigmaColor > 0.0f) ||
        !std::isfinite(desc.sigmaDepth) || desc.sigmaDepth < 0.0f) {
        if (error != nullptr) {
            *error = "denoise sigmas must be finite and positive (depth may be zero)";
        }
        return false;
    }
    if (output == rgb) {
        if (error != nullptr) {
            *error = "denoise output must not alias the input";
        }
        return false;
    }

    const size_t pixels = static_cast<size_t>(desc.width) * desc.height;
    if (!IsFiniteRgb(rgb, pixels * 3)) {
        if (error != nullptr) {
            *error = "denoise input contains a non-finite value";
        }
        return false;
    }
    if (depth != nullptr) {
        for (size_t pixel = 0; pixel < pixels; ++pixel) {
            if (!std::isfinite(depth[pixel])) {
                if (error != nullptr) {
                    *error = "depth guide contains a non-finite value";
                }
                return false;
            }
        }
    }

    const float depthScale =
        desc.sigmaDepth > 0.0f ? -1.0f / desc.sigmaDepth : -1.0e30f;

    for (int y = 0; y < static_cast<int>(desc.height); ++y) {
        for (int x = 0; x < static_cast<int>(desc.width); ++x) {
            const size_t center = (static_cast<size_t>(y) * desc.width +
                                   static_cast<uint32_t>(x)) *
                                  3;
            const float centerLuma = Luma(rgb + center);
            const float centerDepth = depth != nullptr
                                          ? depth[static_cast<size_t>(y) * desc.width +
                                                  static_cast<uint32_t>(x)]
                                          : 0.0f;
            double weightSum = 0.0;
            double sumR = 0.0;
            double sumG = 0.0;
            double sumB = 0.0;
            for (int offsetY = -desc.radius; offsetY <= desc.radius; ++offsetY) {
                const int sampleY = y + offsetY;
                if (sampleY < 0 || sampleY >= static_cast<int>(desc.height)) {
                    continue;
                }
                for (int offsetX = -desc.radius; offsetX <= desc.radius; ++offsetX) {
                    const int sampleX = x + offsetX;
                    if (sampleX < 0 || sampleX >= static_cast<int>(desc.width)) {
                        continue;
                    }
                    const size_t neighbour =
                        (static_cast<size_t>(sampleY) * desc.width +
                         static_cast<uint32_t>(sampleX)) *
                        3;
                    const float neighbourLuma = Luma(rgb + neighbour);
                    const float spatial =
                        1.0f / (1.0f + static_cast<float>(offsetX * offsetX +
                                                          offsetY * offsetY));
                    // Absolute difference: a neighbour may be brighter or
                    // darker than the centre, and neither may be amplified.
                    const float colorWeight = std::exp(
                        -std::fabs(neighbourLuma - centerLuma) / desc.sigmaColor);
                    float depthWeight = 1.0f;
                    if (depth != nullptr) {
                        const float neighbourDepth =
                            depth[static_cast<size_t>(sampleY) * desc.width +
                                  static_cast<uint32_t>(sampleX)];
                        depthWeight =
                            std::exp(std::fabs(neighbourDepth - centerDepth) * depthScale);
                    }
                    const double weight = static_cast<double>(spatial * colorWeight *
                                                              depthWeight);
                    weightSum += weight;
                    sumR += weight * static_cast<double>(rgb[neighbour + 0]);
                    sumG += weight * static_cast<double>(rgb[neighbour + 1]);
                    sumB += weight * static_cast<double>(rgb[neighbour + 2]);
                }
            }
            if (weightSum <= 0.0) {
                output[center + 0] = rgb[center + 0];
                output[center + 1] = rgb[center + 1];
                output[center + 2] = rgb[center + 2];
                continue;
            }
            const double inverse = 1.0 / weightSum;
            output[center + 0] = static_cast<float>(sumR * inverse);
            output[center + 1] = static_cast<float>(sumG * inverse);
            output[center + 2] = static_cast<float>(sumB * inverse);
        }
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool TemporalAccumulator::Reset(uint32_t width, uint32_t height, std::string* error) {
    return ResetAccumulator(&accumulator_, width, height, error);
}

bool TemporalAccumulator::AddSample(const float* rgb, HistoryDecision decision,
                                    std::string* error) {
    if (rgb == nullptr) {
        if (error != nullptr) {
            *error = "TemporalAccumulator needs a sample";
        }
        return false;
    }
    if (decision == HistoryDecision::Reject) {
        if (!ResetAccumulator(&accumulator_, accumulator_.width, accumulator_.height, error)) {
            return false;
        }
    }
    return AccumulateSample(&accumulator_, rgb, error);
}

bool TemporalAccumulator::Resolve(float* mean, std::string* error) const {
    return ResolveAccumulator(accumulator_, mean, error);
}

uint32_t TemporalAccumulator::sampleCount() const {
    if (accumulator_.samples.empty()) {
        return 0;
    }
    return *std::min_element(accumulator_.samples.begin(), accumulator_.samples.end());
}

}  // namespace rhi
}  // namespace doom3
