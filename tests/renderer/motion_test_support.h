#pragma once

// Shared support for the Q3 motion consumer tests (T042/T044/T046).
//
// This translation unit is scanned as a public test unit: it names no backend
// type, handle or private vocabulary. Expected values are predeclared literals
// from tests/renderer/motion-fixtures.md, never derived from the seam.

#include "rhi/motion_history.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace motion_test {

namespace rhi = doom3::rhi;

constexpr double kTolFloor = 1.0e-7;
constexpr double kPi = 3.14159265358979323846;

// Test-local binary16 decode. The seam encodes; this oracle only decodes the
// published word, so a wrong seam encoding cannot also define its own verdict.
inline double HalfValue(uint16_t word) {
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

inline rhi::MotionVec3 P(double x, double y, double z) {
    rhi::MotionVec3 point;
    point.x = x;
    point.y = y;
    point.z = z;
    return point;
}

inline rhi::CameraState CameraAt(double x, double y, double z,
                                 double yawDegrees = 0.0) {
    rhi::CameraState camera;
    camera.eye = P(x, y, z);
    const double radians = yawDegrees * kPi / 180.0;
    camera.forward = P(std::cos(radians), std::sin(radians), 0.0);
    camera.left = P(-std::sin(radians), std::cos(radians), 0.0);
    camera.up = P(0.0, 0.0, 1.0);
    return camera;
}

inline rhi::RenderRecord Record(const rhi::MotionVec3& point,
                                bool hasRenderedRecord = true) {
    rhi::RenderRecord record;
    record.point = point;
    record.hasRenderedRecord = hasRenderedRecord;
    return record;
}

class Runner {
public:
    void Check(bool condition, const std::string& label,
               const std::string& detail = std::string()) {
        ++checks_;
        if (condition) {
            std::printf("PASS %s\n", label.c_str());
            return;
        }
        ++failures_;
        if (detail.empty()) {
            std::printf("FAIL %s\n", label.c_str());
        } else {
            std::printf("FAIL %s: %s\n", label.c_str(), detail.c_str());
        }
    }

    void Near(const std::string& label, double actual, double expected,
              double tolerance) {
        const bool ok = std::fabs(actual - expected) <= tolerance;
        char detail[256];
        std::snprintf(detail, sizeof(detail),
                      "expected %.12g actual %.12g tolerance %.3g", expected,
                      actual, tolerance);
        Check(ok, label, detail);
    }

    void HalfWord(const std::string& label, double value, uint16_t expected) {
        const uint16_t actual = rhi::MotionHalfWord(value);
        const int delta = static_cast<int>(actual) - static_cast<int>(expected);
        const bool ok = delta >= -1 && delta <= 1;
        char detail[256];
        std::snprintf(detail, sizeof(detail),
                      "expected word 0x%04x actual 0x%04x value %.12g",
                      static_cast<unsigned>(expected),
                      static_cast<unsigned>(actual), value);
        Check(ok, label, detail);
    }

    void Uv(const std::string& label, const rhi::MotionUv& actual,
            double expectedU, double expectedV, double tolerance) {
        const double du = std::fabs(actual.u - expectedU);
        const double dv = std::fabs(actual.v - expectedV);
        char detail[256];
        std::snprintf(detail, sizeof(detail),
                      "expected (%.12g, %.12g) actual (%.12g, %.12g) "
                      "tolerance %.3g",
                      expectedU, expectedV, actual.u, actual.v, tolerance);
        Check(du <= tolerance && dv <= tolerance, label, detail);
    }

    void Validity(const std::string& label, rhi::MotionValidity actual,
                  rhi::MotionValidity expected) {
        char detail[256];
        std::snprintf(detail, sizeof(detail), "expected %s actual %s",
                      rhi::ToString(expected), rhi::ToString(actual));
        Check(actual == expected, label, detail);
    }

    int Finish(const char* name) const {
        std::printf("SUMMARY %s checks=%d failures=%d\n", name, checks_,
                    failures_);
        return failures_ == 0 ? 0 : 1;
    }

private:
    int checks_ = 0;
    int failures_ = 0;
};

}  // namespace motion_test
