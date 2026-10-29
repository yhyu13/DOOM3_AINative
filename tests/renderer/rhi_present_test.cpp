// Public-seam window-surface fixture.
//
// A surface is created at a known size, whole frames are composed and read
// back, resized twice, destroyed and recreated after a device restart. Window
// handles, minimize/restore and desktop capture belong to the backend-private
// companion test so this translation unit stays free of native vocabulary.

#include "q1b_test_support.h"

#include <atomic>

namespace {

using doom3::rhi::ClearRow;
using doom3::rhi::Device;
using doom3::rhi::DeviceOptions;
using doom3::rhi::ReadbackResult;
using doom3::rhi::Status;
using doom3::rhi::Submission;
using doom3::rhi::Surface;
using doom3::rhi::SurfaceDesc;

std::atomic<uint32_t> gSurfaceSerial{1};

ClearRow MakeColour(uint8_t red, uint8_t green, uint8_t blue) {
    ClearRow colour;
    colour.red = red;
    colour.green = green;
    colour.blue = blue;
    colour.alpha = 255;
    return colour;
}

int ComposeAndVerify(Device& device, const Surface& surface, const ClearRow& colour,
                     uint32_t width, uint32_t height, const char* label) {
    std::string error;
    Submission submission;
    if (device.SubmitSurfaceFrame(surface, colour, &submission, &error) != Status::Ok) {
        q1b_test::Log("SURFACE submit %s failed: %s", label, error.c_str());
        return 1;
    }
    if (!q1b_test::WaitComplete(device, submission)) {
        q1b_test::Log("SURFACE wait %s failed", label);
        return 1;
    }
    ReadbackResult image;
    if (device.ReadbackAttachment(submission, 0, &image, &error) != Status::Ok) {
        q1b_test::Log("SURFACE readback %s failed: %s", label, error.c_str());
        return 1;
    }
    if (image.width != width || image.height != height) {
        q1b_test::Log("SURFACE %s size %ux%u expected %ux%u", label, image.width,
                      image.height, width, height);
        return 1;
    }
    if (!q1b_test::VerifySolidColour(image, colour)) {
        q1b_test::Log("SURFACE %s is not the requested solid colour", label);
        return 1;
    }
    q1b_test::WriteBmp(
        q1a_test::EvidencePath("q1b-surface-" + std::string(label) + ".bmp"), image, 8);
    q1b_test::Log("SURFACE %s composed %ux%u solid frame", label, width, height);
    if (device.TryRelease(submission, &error) != doom3::rhi::ReleaseStatus::Released) {
        q1b_test::Log("SURFACE release %s failed: %s", label, error.c_str());
        return 1;
    }
    return 0;
}

int RunSurface(Device& device) {
    std::string error;
    const std::string title =
        "Q1b Surface " + std::to_string(gSurfaceSerial.fetch_add(1));
    SurfaceDesc description;
    description.title = title;
    description.width = 64;
    description.height = 48;
    Surface surface;
    const Status created = device.CreateSurface(description, &surface, &error);
    if (created == Status::Unsupported) {
        q1b_test::Log("SURFACE unavailable: %s", error.c_str());
        return q1a_test::kSkipExitCode;
    }
    if (created != Status::Ok) {
        q1b_test::Log("SURFACE create failed: %s", error.c_str());
        return 1;
    }
    q1b_test::Log("SURFACE created %s at 64x48", title.c_str());

    if (ComposeAndVerify(device, surface, MakeColour(10, 20, 30), 64, 48, "first") !=
        0) {
        return 1;
    }
    if (ComposeAndVerify(device, surface, MakeColour(200, 100, 50), 64, 48,
                         "second") != 0) {
        return 1;
    }

    if (device.ResizeSurface(surface, 96, 64, &error) != Status::Ok) {
        q1b_test::Log("SURFACE resize failed: %s", error.c_str());
        return 1;
    }
    q1b_test::Log("SURFACE resized to 96x64");
    if (ComposeAndVerify(device, surface, MakeColour(1, 2, 3), 96, 64, "resized") != 0) {
        return 1;
    }

    if (device.ResizeSurface(surface, 80, 50, &error) != Status::Ok) {
        q1b_test::Log("SURFACE second resize failed: %s", error.c_str());
        return 1;
    }
    q1b_test::Log("SURFACE resized again to 80x50");
    if (ComposeAndVerify(device, surface, MakeColour(30, 60, 90), 80, 50, "restored") !=
        0) {
        return 1;
    }

    if (device.DestroySurface(surface, &error) != Status::Ok) {
        q1b_test::Log("SURFACE destroy failed: %s", error.c_str());
        return 1;
    }
    q1b_test::Log("SURFACE destroyed");

    const doom3::rhi::AdapterClass adapterClass = device.adapter().adapterClass;
    if (device.Shutdown() != Status::Ok) {
        q1b_test::Log("SURFACE first device shutdown failed");
        return 1;
    }
    DeviceOptions options;
    options.adapterClass = adapterClass;
    if (device.Initialize(options, &error) != Status::Ok) {
        q1b_test::Log("SURFACE restart initialize failed: %s", error.c_str());
        return 1;
    }
    q1b_test::Log("SURFACE device restarted");
    const std::string secondTitle =
        "Q1b Surface " + std::to_string(gSurfaceSerial.fetch_add(1));
    SurfaceDesc restartDescription;
    restartDescription.title = secondTitle;
    restartDescription.width = 32;
    restartDescription.height = 32;
    Surface restarted;
    if (device.CreateSurface(restartDescription, &restarted, &error) != Status::Ok) {
        q1b_test::Log("SURFACE create after restart failed: %s", error.c_str());
        return 1;
    }
    if (ComposeAndVerify(device, restarted, MakeColour(120, 130, 140), 32, 32,
                         "restarted") != 0) {
        return 1;
    }
    if (device.DestroySurface(restarted, &error) != Status::Ok) {
        return 1;
    }
    q1b_test::Log("SURFACE restart path verified");
    return 0;
}

}  // namespace

int main() {
    return q1b_test::ForEachAdapter([](Device& device) { return RunSurface(device); });
}
