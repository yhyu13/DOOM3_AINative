#include "rhi/motion_history.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace doom3 {
namespace rhi {

namespace {

double Dot(const MotionVec3& a, const MotionVec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

MotionVec3 Subtract(const MotionVec3& a, const MotionVec3& b) {
    return MotionVec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

}  // namespace

const char* ToString(MotionValidity value) {
    switch (value) {
        case MotionValidity::Valid: return "VALID";
        case MotionValidity::NoHistory: return "NO_HISTORY";
        case MotionValidity::Disocclusion: return "DISOCCLUSION";
        case MotionValidity::BehindCamera: return "BEHIND_CAMERA";
        case MotionValidity::Topology: return "TOPOLOGY";
        case MotionValidity::ProviderSwitch: return "PROVIDER_SWITCH";
        case MotionValidity::Generation: return "GENERATION";
        case MotionValidity::Reset: return "RESET";
        case MotionValidity::Resize: return "RESIZE";
        case MotionValidity::EntityReuse: return "ENTITY_REUSE";
    }
    return "UNKNOWN";
}

bool ProjectHistoryPoint(const CameraState& camera, const MotionVec3& point,
                         MotionUv* uv) {
    if (uv == nullptr) {
        return false;
    }
    if (!(camera.tanX > 0.0) || !(camera.tanY > 0.0)) {
        return false;
    }
    const MotionVec3 delta = Subtract(point, camera.eye);
    const double depth = Dot(camera.forward, delta);
    // NaN-safe: a non-positive or unordered depth has no correspondence.
    if (!(depth > 0.0)) {
        return false;
    }
    const double xView = -Dot(camera.left, delta);
    const double yView = Dot(camera.up, delta);
    const double u = 0.5 + 0.5 * xView / (depth * camera.tanX);
    const double v = 0.5 - 0.5 * yView / (depth * camera.tanY);
    if (!std::isfinite(u) || !std::isfinite(v)) {
        return false;
    }
    uv->u = u;
    uv->v = v;
    return true;
}

bool ProjectRasterPoint(const CameraState& camera, const MotionVec3& point,
                        MotionUv* uv) {
    if (camera.width == 0 || camera.height == 0) {
        return false;
    }
    if (!ProjectHistoryPoint(camera, point, uv)) {
        return false;
    }
    // One-sided shift from tr_main.cpp:929-934. It changes only the raster
    // projection, never the history value.
    uv->u -= camera.jitterPixelsX / static_cast<double>(camera.width);
    uv->v += camera.jitterPixelsY / static_cast<double>(camera.height);
    return true;
}

MotionSample ReprojectPoint(const CameraState& previousCamera,
                            const RenderRecord& previous,
                            const CameraState& currentCamera,
                            const RenderRecord& current) {
    MotionSample sample;
    if (!previous.hasRenderedRecord) {
        sample.validity = MotionValidity::NoHistory;
        return sample;
    }

    MotionUv currentUv;
    if (!ProjectHistoryPoint(currentCamera, current.point, &currentUv)) {
        sample.validity = MotionValidity::BehindCamera;
        return sample;
    }
    MotionUv previousUv;
    if (!ProjectHistoryPoint(previousCamera, previous.point, &previousUv)) {
        sample.validity = MotionValidity::BehindCamera;
        return sample;
    }
    if (previousUv.u < 0.0 || previousUv.u > 1.0 || previousUv.v < 0.0 ||
        previousUv.v > 1.0) {
        sample.validity = MotionValidity::Disocclusion;
        return sample;
    }

    sample.validity = MotionValidity::Valid;
    sample.vector.du = previousUv.u - currentUv.u;
    sample.vector.dv = previousUv.v - currentUv.v;
    return sample;
}

uint16_t MotionHalfWord(double value) {
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));

    const uint16_t sign = static_cast<uint16_t>((bits >> 48) & 0x8000u);
    const uint32_t exponent = static_cast<uint32_t>((bits >> 52) & 0x7FFu);
    const uint64_t mantissa = bits & 0xFFFFFFFFFFFFFull;

    if (exponent == 0x7FFu) {
        const uint16_t payload = mantissa != 0 ? 0x0200u : 0x0000u;
        return static_cast<uint16_t>(sign | 0x7C00u | payload);
    }
    if (exponent == 0u) {
        // A double subnormal is far below the half subnormal range.
        return sign;
    }

    const int32_t unbiased = static_cast<int32_t>(exponent) - 1023;

    if (unbiased > 15) {
        return static_cast<uint16_t>(sign | 0x7C00u);
    }

    if (unbiased >= -14) {
        // Normal half: 10 of the 52 mantissa bits survive.
        const uint32_t halfMantissa =
            static_cast<uint32_t>((mantissa >> 42) & 0x3FFu);
        const uint64_t remainder = mantissa & ((1ull << 42) - 1ull);
        const uint64_t halfway = 1ull << 41;
        uint32_t rounded = halfMantissa;
        if (remainder > halfway ||
            (remainder == halfway && (rounded & 1u) != 0u)) {
            ++rounded;
        }
        const uint32_t halfExponent = static_cast<uint32_t>(unbiased + 15);
        // A mantissa carry increments the exponent and can produce infinity.
        return static_cast<uint16_t>(sign | (halfExponent << 10) | rounded);
    }

    if (unbiased < -24) {
        return sign;
    }

    // Subnormal half: quantize the implicit-bit significand to units of 2^-24.
    const uint64_t significand = mantissa | (1ull << 52);
    const int shift = 28 - unbiased;
    const uint32_t rounded = static_cast<uint32_t>(significand >> shift);
    const uint64_t remainder =
        significand & ((1ull << static_cast<uint32_t>(shift)) - 1ull);
    const uint64_t halfway = 1ull << static_cast<uint32_t>(shift - 1);
    uint32_t result = rounded;
    if (remainder > halfway || (remainder == halfway && (result & 1u) != 0u)) {
        ++result;
    }
    return static_cast<uint16_t>(sign | result);
}

double DecodeMotionHalfWord(uint16_t word) {
    const uint32_t sign = (word & 0x8000u) != 0u ? 1u : 0u;
    const uint32_t exponent = (word >> 10) & 0x1Fu;
    const uint32_t mantissa = word & 0x3FFu;
    double value = 0.0;
    if (exponent == 0u) {
        value = std::ldexp(static_cast<double>(mantissa), -24);
    } else if (exponent == 31u) {
        value = mantissa != 0u ? std::nan("") : HUGE_VAL;
    } else {
        value = std::ldexp(static_cast<double>(mantissa | 0x400u),
                           static_cast<int>(exponent) - 25);
    }
    return sign != 0u ? -value : value;
}

CorrespondenceResult EvaluateCorrespondence(SurfaceClass surface,
                                            const CorrespondenceDesc& desc) {
    CorrespondenceResult result;

    // A subview or provider transition has no main-view correspondence even
    // when every other input looks current.
    if (desc.providerSwitched || desc.subview ||
        surface == SurfaceClass::Reflection) {
        result.validity = MotionValidity::ProviderSwitch;
        return result;
    }
    if (desc.resetOccurred) {
        result.validity = MotionValidity::Reset;
        return result;
    }
    if (desc.resized) {
        result.validity = MotionValidity::Resize;
        return result;
    }
    if (!desc.hasPreviousRenderedRecord) {
        result.validity = MotionValidity::NoHistory;
        return result;
    }
    if (desc.generationMismatch) {
        result.validity = MotionValidity::Generation;
        return result;
    }
    if (desc.entitySlotReused ||
        (surface == SurfaceClass::Particle && desc.respawned)) {
        result.validity = MotionValidity::EntityReuse;
        return result;
    }
    if (!desc.sameSurfaceIdentity || !desc.sameTopology) {
        result.validity = MotionValidity::Topology;
        return result;
    }
    if (desc.newPixel) {
        result.validity = MotionValidity::Disocclusion;
        return result;
    }

    result.validity = MotionValidity::Valid;
    result.canUseVector = true;
    return result;
}

}  // namespace rhi
}  // namespace doom3
