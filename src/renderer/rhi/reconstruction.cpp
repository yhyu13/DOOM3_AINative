// Non-proprietary reconstruction and off path (spec 004 T055).
//
// Arithmetic-only: a bilinear upscale and an identity path. This is deliberately
// not a temporal or learned reconstruction; it exists so the renderer has a
// usable display-stage fallback without any proprietary dependency, and so the
// ordering fixture can run the real off path.

#include "rhi/reconstruction.h"

#include <cmath>
#include <cstring>

namespace doom3 {
namespace rhi {

namespace {

Status Fail(std::string* error, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
    return Status::InvalidArgument;
}

void ClearError(std::string* error) {
    if (error != nullptr) {
        error->clear();
    }
}

uint32_t ClampIndex(int64_t value, uint32_t limit) {
    if (value < 0) {
        return 0;
    }
    if (static_cast<uint64_t>(value) >= limit) {
        return limit - 1;
    }
    return static_cast<uint32_t>(value);
}

}  // namespace

const char* ToString(ReconstructionMode value) {
    switch (value) {
        case ReconstructionMode::Off: return "Off";
        case ReconstructionMode::SpatialUpscale: return "SpatialUpscale";
    }
    return "Unknown";
}

Status PlanReconstruction(const ReconstructionDesc& desc, const ReconstructionOptions& options,
                          ReconstructionPlan* plan, std::string* error) {
    if (plan == nullptr) {
        return Fail(error, "PlanReconstruction needs an output");
    }
    *plan = ReconstructionPlan();

    if (desc.renderWidth == 0 || desc.renderHeight == 0 || desc.displayWidth == 0 ||
        desc.displayHeight == 0) {
        plan->status = Status::InvalidArgument;
        plan->error = "reconstruction extents must be non-zero";
        if (error != nullptr) {
            *error = plan->error;
        }
        return Status::InvalidArgument;
    }
    if (desc.displayWidth < desc.renderWidth || desc.displayHeight < desc.renderHeight) {
        plan->status = Status::InvalidArgument;
        plan->error = "reconstruction only upscales; the display extent is smaller";
        if (error != nullptr) {
            *error = plan->error;
        }
        return Status::InvalidArgument;
    }

    plan->providerRequired = false;
    plan->providerFellBack = options.preferProvider && !options.providerAvailable;
    if (desc.displayWidth == desc.renderWidth && desc.displayHeight == desc.renderHeight) {
        plan->mode = ReconstructionMode::Off;
        plan->identity = true;
    } else {
        plan->mode = ReconstructionMode::SpatialUpscale;
        plan->identity = false;
    }
    plan->status = Status::Ok;
    ClearError(error);
    return Status::Ok;
}

Status ReconstructRgba(const float* input, uint32_t inputWidth, uint32_t inputHeight,
                       float* output, uint32_t outputWidth, uint32_t outputHeight,
                       ReconstructionMode mode, std::string* error) {
    if (input == nullptr || output == nullptr) {
        return Fail(error, "reconstruction needs input and output buffers");
    }
    if (inputWidth == 0 || inputHeight == 0 || outputWidth == 0 || outputHeight == 0) {
        return Fail(error, "reconstruction extents must be non-zero");
    }

    const size_t inputTexels =
        static_cast<size_t>(inputWidth) * static_cast<size_t>(inputHeight) * 4u;
    for (size_t index = 0; index < inputTexels; ++index) {
        if (!std::isfinite(input[index])) {
            return Fail(error, "reconstruction input contains a non-finite sample");
        }
    }

    if (mode == ReconstructionMode::Off) {
        if (inputWidth != outputWidth || inputHeight != outputHeight) {
            return Fail(error, "the off path requires equal render and display extents");
        }
        std::memcpy(output, input, inputTexels * sizeof(float));
        ClearError(error);
        return Status::Ok;
    }

    if (outputWidth < inputWidth || outputHeight < inputHeight) {
        return Fail(error, "spatial upscale requires a display extent at least as large");
    }

    const float scaleX = static_cast<float>(inputWidth) / static_cast<float>(outputWidth);
    const float scaleY = static_cast<float>(inputHeight) / static_cast<float>(outputHeight);

    for (uint32_t y = 0; y < outputHeight; ++y) {
        const float sourceY = (static_cast<float>(y) + 0.5f) * scaleY - 0.5f;
        const int64_t y0 = static_cast<int64_t>(std::floor(sourceY));
        const float fy = sourceY - static_cast<float>(y0);
        const uint32_t row0 = ClampIndex(y0, inputHeight);
        const uint32_t row1 = ClampIndex(y0 + 1, inputHeight);

        for (uint32_t x = 0; x < outputWidth; ++x) {
            const float sourceX = (static_cast<float>(x) + 0.5f) * scaleX - 0.5f;
            const int64_t x0 = static_cast<int64_t>(std::floor(sourceX));
            const float fx = sourceX - static_cast<float>(x0);
            const uint32_t col0 = ClampIndex(x0, inputWidth);
            const uint32_t col1 = ClampIndex(x0 + 1, inputWidth);

            const size_t texel00 = (static_cast<size_t>(row0) * inputWidth + col0) * 4u;
            const size_t texel10 = (static_cast<size_t>(row0) * inputWidth + col1) * 4u;
            const size_t texel01 = (static_cast<size_t>(row1) * inputWidth + col0) * 4u;
            const size_t texel11 = (static_cast<size_t>(row1) * inputWidth + col1) * 4u;
            const size_t target = (static_cast<size_t>(y) * outputWidth + x) * 4u;

            for (uint32_t channel = 0; channel < 4; ++channel) {
                const float top =
                    input[texel00 + channel] +
                    (input[texel10 + channel] - input[texel00 + channel]) * fx;
                const float bottom =
                    input[texel01 + channel] +
                    (input[texel11 + channel] - input[texel01 + channel]) * fx;
                output[target + channel] = top + (bottom - top) * fy;
            }
        }
    }

    ClearError(error);
    return Status::Ok;
}

}  // namespace rhi
}  // namespace doom3
