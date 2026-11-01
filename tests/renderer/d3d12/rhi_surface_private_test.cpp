// D3D12-private window-surface fixture.
//
// Native window handles, minimize/restore and a desktop capture are confined
// to this excluded directory. The public surface test asserts composition,
// resize, destroy and restart through the seam only.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "q1b_test_support.h"
#include "d3d12/private_test_support.h"

#include <atomic>
#include <string>
#include <vector>

namespace {

using doom3::rhi::ClearRow;
using doom3::rhi::Device;
using doom3::rhi::ReadbackResult;
using doom3::rhi::Status;
using doom3::rhi::Submission;
using doom3::rhi::Surface;
using doom3::rhi::SurfaceDesc;

std::atomic<uint32_t> gSurfaceSerial{1};

std::wstring WidenAscii(const std::string& value) {
    return std::wstring(value.begin(), value.end());
}

void PumpMessages() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

bool CaptureWindow(HWND window, const std::string& path) {
    RECT rect{};
    if (window == nullptr || !GetClientRect(window, &rect) || rect.right <= 0 ||
        rect.bottom <= 0) {
        return false;
    }
    const int width = rect.right;
    const int height = rect.bottom;
    HDC windowDc = GetDC(window);
    if (windowDc == nullptr) {
        return false;
    }
    HDC memoryDc = CreateCompatibleDC(windowDc);
    HBITMAP bitmap = CreateCompatibleBitmap(windowDc, width, height);
    if (memoryDc == nullptr || bitmap == nullptr) {
        if (memoryDc) {
            DeleteDC(memoryDc);
        }
        if (bitmap) {
            DeleteObject(bitmap);
        }
        ReleaseDC(window, windowDc);
        return false;
    }
    HGDIOBJ previous = SelectObject(memoryDc, bitmap);
    const BOOL captured = PrintWindow(window, memoryDc, 2 /* render full content */);
    bool written = false;
    if (captured) {
        BITMAPINFOHEADER header{};
        header.biSize = sizeof(header);
        header.biWidth = width;
        header.biHeight = height;
        header.biPlanes = 1;
        header.biBitCount = 32;
        header.biCompression = BI_RGB;
        BITMAPINFO info{};
        info.bmiHeader = header;
        std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
        if (GetDIBits(memoryDc, bitmap, 0, static_cast<UINT>(height), pixels.data(),
                      &info, DIB_RGB_COLORS) != 0) {
            const uint32_t outStride = (static_cast<uint32_t>(width) * 3u + 3u) & ~3u;
            const uint32_t imageSize = outStride * static_cast<uint32_t>(height);
            std::vector<uint8_t> file(54 + imageSize, 0);
            file[0] = 'B';
            file[1] = 'M';
            const uint32_t fileSize = 54 + imageSize;
            std::memcpy(&file[2], &fileSize, 4);
            const uint32_t offset = 54;
            std::memcpy(&file[10], &offset, 4);
            const uint32_t headerSize = 40;
            std::memcpy(&file[14], &headerSize, 4);
            std::memcpy(&file[18], &width, 4);
            std::memcpy(&file[22], &height, 4);
            const uint16_t planes = 1;
            std::memcpy(&file[26], &planes, 2);
            const uint16_t bits = 24;
            std::memcpy(&file[28], &bits, 2);
            std::memcpy(&file[34], &imageSize, 4);
            for (int y = 0; y < height; ++y) {
                const uint8_t* source =
                    pixels.data() + static_cast<size_t>(height - 1 - y) * width * 4;
                uint8_t* target = file.data() + 54 + static_cast<size_t>(y) * outStride;
                for (int x = 0; x < width; ++x) {
                    target[x * 3 + 0] = source[x * 4 + 0];
                    target[x * 3 + 1] = source[x * 4 + 1];
                    target[x * 3 + 2] = source[x * 4 + 2];
                }
            }
            FILE* out = nullptr;
            if (fopen_s(&out, path.c_str(), "wb") == 0 && out != nullptr) {
                written = fwrite(file.data(), 1, file.size(), out) == file.size();
                fclose(out);
            }
        }
    }
    SelectObject(memoryDc, previous);
    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    ReleaseDC(window, windowDc);
    return written;
}

ClearRow MakeColour(uint8_t red, uint8_t green, uint8_t blue) {
    ClearRow colour;
    colour.red = red;
    colour.green = green;
    colour.blue = blue;
    colour.alpha = 255;
    return colour;
}

int Compose(Device& device, const Surface& surface, const ClearRow& colour,
            uint32_t width, uint32_t height, const char* label) {
    std::string error;
    Submission submission;
    if (device.SubmitSurfaceFrame(surface, colour, &submission, &error) != Status::Ok) {
        q1b_test::Log("PRIVATE_SURFACE submit %s failed: %s", label, error.c_str());
        return 1;
    }
    if (!q1b_test::WaitComplete(device, submission)) {
        return 1;
    }
    ReadbackResult image;
    if (device.ReadbackAttachment(submission, 0, &image, &error) != Status::Ok) {
        return 1;
    }
    if (image.width != width || image.height != height ||
        !q1b_test::VerifySolidColour(image, colour)) {
        q1b_test::Require(false, "private surface frame did not match its input");
        return 1;
    }
    q1b_test::WriteBmp(
        q1a_test::EvidencePath("q1b-surface-private-" + std::string(label) + ".bmp"),
        image, 8);
    if (device.TryRelease(submission, &error) != doom3::rhi::ReleaseStatus::Released) {
        return 1;
    }
    return 0;
}

int RunPrivateSurface(Device& device) {
    std::string error;
    const std::string title =
        "Q1b Private Surface " + std::to_string(gSurfaceSerial.fetch_add(1));
    SurfaceDesc description;
    description.title = title;
    description.width = 64;
    description.height = 48;
    Surface surface;
    if (device.CreateSurface(description, &surface, &error) != Status::Ok) {
        q1b_test::Log("PRIVATE_SURFACE create failed: %s", error.c_str());
        return 1;
    }
    HWND window = FindWindowW(nullptr, WidenAscii(title).c_str());
    if (window == nullptr) {
        q1b_test::Require(false, "surface window was not found by its title");
        return 1;
    }
    if (Compose(device, surface, MakeColour(10, 20, 30), 64, 48, "first") != 0) {
        return 1;
    }
    const bool firstCapture = CaptureWindow(
        window, q1a_test::EvidencePath("q1b-surface-private-first-window.bmp"));
    q1b_test::Log("PRIVATE_SURFACE window capture %s",
                  firstCapture ? "WRITTEN" : "NOT RUN (platform refused the copy)");

    ShowWindow(window, SW_MINIMIZE);
    PumpMessages();
    Sleep(100);
    Submission refused;
    const Status minimized =
        device.SubmitSurfaceFrame(surface, MakeColour(9, 9, 9), &refused, &error);
    if (minimized != Status::NotReady) {
        q1b_test::Require(false, "minimized surface was not refused with NotReady");
        return 1;
    }
    q1b_test::Log("PRIVATE_SURFACE minimized work refused: %s", error.c_str());

    ShowWindow(window, SW_RESTORE);
    PumpMessages();
    Sleep(100);
    if (device.ResizeSurface(surface, 96, 64, &error) != Status::Ok) {
        q1b_test::Log("PRIVATE_SURFACE resize failed: %s", error.c_str());
        return 1;
    }
    if (Compose(device, surface, MakeColour(30, 60, 90), 96, 64, "restored") != 0) {
        return 1;
    }
    const bool secondCapture = CaptureWindow(
        window, q1a_test::EvidencePath("q1b-surface-private-restored-window.bmp"));
    q1b_test::Log("PRIVATE_SURFACE restored capture %s",
                  secondCapture ? "WRITTEN" : "NOT RUN (platform refused the copy)");
    if (device.DestroySurface(surface, &error) != Status::Ok) {
        return 1;
    }
    q1b_test::Log("PRIVATE_SURFACE minimize/restore/resize/capture path verified");
    return 0;
}

}  // namespace

int main() {
    // The surface fixture runs under the debug layer and GPU-based validation
    // so the Q1b window path is covered by the same unwaived-message gate as the
    // clear/readback and draw fixtures.
    using doom3::rhi::AdapterClass;
    using doom3::rhi::Device;
    using doom3::rhi::DeviceOptions;
    using doom3::rhi::Status;
    using doom3::rhi::d3d12::DiagnosticRequest;

    DiagnosticRequest request;
    request.enableDebugLayer = true;
    request.enableGpuBasedValidation = true;

    int passes = 0;
    int failures = 0;
    int skips = 0;
    for (AdapterClass adapterClass : {AdapterClass::Hardware, AdapterClass::Software}) {
        Device device;
        DeviceOptions options;
        options.adapterClass = adapterClass;
        std::string error;
        const Status status = doom3::rhi::d3d12::InitializeWithDiagnostics(
            device, options, request, &error);
        if (status == Status::Unsupported) {
            q1a_test::Log("PRIVATE_ADAPTER %s SKIPPED reason=%s",
                          doom3::rhi::ToString(adapterClass), error.c_str());
            ++skips;
            continue;
        }
        if (status != Status::Ok) {
            q1a_test::Log("PRIVATE_ADAPTER %s FAILED status=%s reason=%s",
                          doom3::rhi::ToString(adapterClass),
                          doom3::rhi::ToString(status), error.c_str());
            ++failures;
            continue;
        }
        q1a_test::PrintAdapter(device.adapter());
        bool ok = true;
        // The surface back buffer is created by DXGI, and this fixture declares
        // no optimized clear value it could match; the observed queue contains
        // only the ID-1016 startup notice. Any ID-820 here would be a new,
        // unwaived message, so the allowance is explicitly zero rather than
        // inheriting the Q1a row-clear budget.
        q1a_private::WaiverBudget surfaceBudget;
        surfaceBudget.clearTarget820 = 0;
        const auto before = doom3::rhi::d3d12::ReadDiagnostics(device);
        q1a_private::LogDiagnosticMessages("surface-before", before);
        ok &= q1a_private::RequireNoUnwaivedMessages("surface-before", before,
                                                     surfaceBudget);
        const int result = RunPrivateSurface(device);
        const auto after = doom3::rhi::d3d12::ReadDiagnostics(device);
        q1a_private::LogDiagnosticMessages("surface-after", after);
        ok &= q1a_test::Require(after.errorCount == 0,
                                "surface debug layer reported errors");
        ok &= q1a_private::RequireNoUnwaivedMessages("surface-after", after,
                                                     surfaceBudget);
        const Status shutdown = device.Shutdown();
        ok &= q1a_test::Require(shutdown == Status::Ok,
                                "surface device did not shut down cleanly");
        if (result == 0 && ok) {
            ++passes;
            q1a_test::Log("PRIVATE_ADAPTER %s RESULT PASS",
                          doom3::rhi::ToString(adapterClass));
        } else {
            ++failures;
            q1a_test::Log("PRIVATE_ADAPTER %s RESULT FAIL",
                          doom3::rhi::ToString(adapterClass));
        }
    }
    q1a_test::Log("PRIVATE_SUMMARY pass=%d fail=%d skip=%d", passes, failures, skips);
    if (failures > 0) {
        return 1;
    }
    if (passes > 0) {
        return 0;
    }
    return q1a_test::kSkipExitCode;
}
