#pragma once

// Linear colour values for the Q4 quality path (spec 004 T051/T052/T053).
//
// The classic path carries textured colours in the engine's own encoding. The
// hybrid opaque path instead consumes explicit linear values, because lighting
// and indirect accumulation are only meaningful before the display transfer.
// HDR values are not clamped: an emissive channel may legitimately exceed 1.0,
// and the display transform is the last step, not this header.
//
// Nothing here is a colour-management promise. It is one documented transfer
// pair (the sRGB piecewise curve), a linear container and the finite/non-negative
// checks the callers need so a bad input is refused instead of propagated as a
// NaN.

#include <cmath>

namespace doom3 {
namespace rhi {

struct LinearRgb {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

inline bool IsFinite(float value) { return std::isfinite(value); }

inline bool IsFinite(const LinearRgb& value) {
    return IsFinite(value.r) && IsFinite(value.g) && IsFinite(value.b);
}

inline bool IsNonNegativeFinite(const LinearRgb& value) {
    return IsFinite(value) && value.r >= 0.0f && value.g >= 0.0f && value.b >= 0.0f;
}

inline float SrgbChannelToLinear(float channel) {
    if (!(channel > 0.0f)) {
        return 0.0f;
    }
    if (channel <= 0.04045f) {
        return channel / 12.92f;
    }
    return std::pow((channel + 0.055f) / 1.055f, 2.4f);
}

// Encoded (sRGB) colour to linear light. The caller keeps the HDR range: this
// function decodes a [0,1] encoded channel and does not clamp the linear result
// of a wider input.
inline LinearRgb SrgbToLinear(const LinearRgb& srgb) {
    LinearRgb linear;
    linear.r = SrgbChannelToLinear(srgb.r);
    linear.g = SrgbChannelToLinear(srgb.g);
    linear.b = SrgbChannelToLinear(srgb.b);
    return linear;
}

inline float MaxChannel(const LinearRgb& value) {
    const float rg = value.r > value.g ? value.r : value.g;
    return rg > value.b ? rg : value.b;
}

inline float Luminance(const LinearRgb& value) {
    return 0.2126f * value.r + 0.7152f * value.g + 0.0722f * value.b;
}

inline LinearRgb Lerp(const LinearRgb& a, const LinearRgb& b, float t) {
    LinearRgb result;
    result.r = a.r + (b.r - a.r) * t;
    result.g = a.g + (b.g - a.g) * t;
    result.b = a.b + (b.b - a.b) * t;
    return result;
}

}  // namespace rhi
}  // namespace doom3
