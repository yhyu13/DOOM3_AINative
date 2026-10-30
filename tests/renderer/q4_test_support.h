#pragma once

// Shared Q4 fixture support (spec 004 T050-T055, T086).
//
// Public test unit: it names no backend type, native handle or private state.
// Every expected value in the Q4 tests is an independent literal or a formula
// documented in the corresponding header, never a value copied from an
// implementation output.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace q4_test {

class Runner {
public:
    void Check(bool condition, const std::string& label,
               const std::string& detail = std::string()) {
        ++checks_;
        if (condition) {
            std::printf("PASS %s\n", label.c_str());
        } else {
            ++failures_;
            if (detail.empty()) {
                std::printf("FAIL %s\n", label.c_str());
            } else {
                std::printf("FAIL %s: %s\n", label.c_str(), detail.c_str());
            }
        }
        // Flushed immediately so a hung Debug run still shows the last check.
        std::fflush(stdout);
    }

    void Near(float actual, float expected, float tolerance, const std::string& label) {
        const bool finite = std::isfinite(actual) && std::isfinite(expected);
        const float delta = finite ? std::fabs(actual - expected) : 0.0f;
        Check(finite && delta <= tolerance, label,
              "expected " + std::to_string(expected) + ", observed " +
                  std::to_string(actual));
    }

    int Finish(const char* name) const {
        std::printf("SUMMARY %s checks=%d failures=%d\n", name, checks_, failures_);
        return failures_ == 0 ? 0 : 1;
    }

private:
    int checks_ = 0;
    int failures_ = 0;
};

}  // namespace q4_test
