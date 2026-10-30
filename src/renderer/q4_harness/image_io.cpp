#include "image_io.h"

#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <string>

using Microsoft::WRL::ComPtr;

namespace q4 {
namespace {

std::wstring ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int length =
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (length <= 1) {
        return {};
    }
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &result[0], length);
    result.resize(static_cast<size_t>(length - 1));
    return result;
}

bool Fail(const char* operation, HRESULT hr, std::string* error) {
    if (error != nullptr) {
        char buffer[128] = {};
        std::snprintf(buffer, sizeof(buffer), "%s failed (hr=0x%08lX)", operation,
                      static_cast<unsigned long>(static_cast<uint32_t>(hr)));
        *error = buffer;
    }
    return false;
}

}  // namespace

bool WritePngRgba8(const std::string& path, uint32_t width, uint32_t height,
                   const uint8_t* pixels, uint32_t rowPitch, std::string* error) {
    if (width == 0 || height == 0 || pixels == nullptr) {
        if (error != nullptr) {
            *error = "WritePngRgba8 received an empty image";
        }
        return false;
    }
    const std::wstring widePath = ToWide(path);
    if (widePath.empty()) {
        if (error != nullptr) {
            *error = "cannot convert the screenshot path to UTF-16";
        }
        return false;
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        return Fail("CoCreateInstance(WICImagingFactory)", hr, error);
    }

    ComPtr<IWICStream> stream;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr)) {
        return Fail("IWICImagingFactory::CreateStream", hr, error);
    }
    hr = stream->InitializeFromFilename(widePath.c_str(), GENERIC_WRITE);
    if (FAILED(hr)) {
        return Fail("IWICStream::InitializeFromFilename", hr, error);
    }

    ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (FAILED(hr)) {
        return Fail("IWICImagingFactory::CreateEncoder(PNG)", hr, error);
    }
    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        return Fail("IWICBitmapEncoder::Initialize", hr, error);
    }

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> properties;
    hr = encoder->CreateNewFrame(&frame, &properties);
    if (FAILED(hr)) {
        return Fail("IWICBitmapEncoder::CreateNewFrame", hr, error);
    }
    hr = frame->Initialize(properties.Get());
    if (FAILED(hr)) {
        return Fail("IWICBitmapFrameEncode::Initialize", hr, error);
    }
    hr = frame->SetSize(width, height);
    if (FAILED(hr)) {
        return Fail("IWICBitmapFrameEncode::SetSize", hr, error);
    }
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppRGBA;
    hr = frame->SetPixelFormat(&format);
    if (FAILED(hr)) {
        return Fail("IWICBitmapFrameEncode::SetPixelFormat", hr, error);
    }

    ComPtr<IWICBitmap> bitmap;
    const uint64_t byteCount = static_cast<uint64_t>(rowPitch) * height;
    hr = factory->CreateBitmapFromMemory(
        width, height, GUID_WICPixelFormat32bppRGBA, rowPitch,
        static_cast<UINT>(byteCount), const_cast<BYTE*>(pixels), &bitmap);
    if (FAILED(hr)) {
        return Fail("IWICImagingFactory::CreateBitmapFromMemory", hr, error);
    }

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        return Fail("IWICImagingFactory::CreateFormatConverter", hr, error);
    }
    hr = converter->Initialize(bitmap.Get(), format, WICBitmapDitherTypeNone,
                               nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        return Fail("IWICFormatConverter::Initialize", hr, error);
    }
    hr = frame->WriteSource(converter.Get(), nullptr);
    if (FAILED(hr)) {
        return Fail("IWICBitmapFrameEncode::WriteSource", hr, error);
    }
    hr = frame->Commit();
    if (FAILED(hr)) {
        return Fail("IWICBitmapFrameEncode::Commit", hr, error);
    }
    hr = encoder->Commit();
    if (FAILED(hr)) {
        return Fail("IWICBitmapEncoder::Commit", hr, error);
    }
    return true;
}

}  // namespace q4
