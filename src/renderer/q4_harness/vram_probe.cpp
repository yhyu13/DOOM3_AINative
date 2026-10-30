#include "vram_probe.h"

#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdio>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace q4 {
namespace {

std::string Narrow(const wchar_t* value) {
    if (value == nullptr || *value == L'\0') {
        return {};
    }
    const int length =
        WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) {
        return {};
    }
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, &result[0], length, nullptr,
                        nullptr);
    result.resize(static_cast<size_t>(length - 1));
    return result;
}

std::string JsonEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out += c;
                break;
        }
    }
    return out;
}

void AppendU64(std::string* out, uint64_t value) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%llu",
                  static_cast<unsigned long long>(value));
    *out += buffer;
}

void AppendSample(std::string* out, const VramSample& sample) {
    *out += "{\"phase\":\"";
    *out += JsonEscape(sample.phase);
    *out += "\",\"frame\":";
    AppendU64(out, sample.frame);
    *out += ",\"localBudget\":";
    AppendU64(out, sample.localBudget);
    *out += ",\"localUsage\":";
    AppendU64(out, sample.localUsage);
    *out += ",\"localAvailableForReservation\":";
    AppendU64(out, sample.localAvailableForReservation);
    *out += ",\"localReservation\":";
    AppendU64(out, sample.localReservation);
    *out += ",\"nonLocalBudget\":";
    AppendU64(out, sample.nonLocalBudget);
    *out += ",\"nonLocalUsage\":";
    AppendU64(out, sample.nonLocalUsage);
    *out += ",\"nonLocalAvailableForReservation\":";
    AppendU64(out, sample.nonLocalAvailableForReservation);
    *out += ",\"nonLocalReservation\":";
    AppendU64(out, sample.nonLocalReservation);
    *out += '}';
}

}  // namespace

struct VramProbe::Impl {
    ComPtr<IDXGIAdapter3> adapter;
    std::string adapterName;
    std::string luid;
    std::string unavailableReason;
    uint64_t dedicatedVideoMemory = 0;
    uint64_t sharedSystemMemory = 0;
    std::vector<VramSample> samples;
};

VramProbe::VramProbe() : impl_(std::make_unique<Impl>()) {}
VramProbe::~VramProbe() = default;

bool VramProbe::Initialize(ID3D12Device* device, std::string* error) {
    impl_->samples.clear();
    impl_->adapter.Reset();
    if (device == nullptr) {
        impl_->unavailableReason = "no D3D12 device";
        if (error != nullptr) {
            *error = impl_->unavailableReason;
        }
        return false;
    }

    const LUID deviceLuid = device->GetAdapterLuid();
    char luidBuffer[48] = {};
    std::snprintf(luidBuffer, sizeof(luidBuffer), "0x%08lX:%08lX",
                  static_cast<unsigned long>(deviceLuid.HighPart),
                  static_cast<unsigned long>(deviceLuid.LowPart));
    impl_->luid = luidBuffer;

    ComPtr<IDXGIFactory1> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) {
        impl_->unavailableReason = "CreateDXGIFactory1 failed";
        if (error != nullptr) {
            *error = impl_->unavailableReason;
        }
        return false;
    }
    ComPtr<IDXGIFactory4> factory4;
    hr = factory.As(&factory4);
    if (FAILED(hr) || !factory4) {
        impl_->unavailableReason = "IDXGIFactory4 is unavailable";
        if (error != nullptr) {
            *error = impl_->unavailableReason;
        }
        return false;
    }
    ComPtr<IDXGIAdapter1> adapter1;
    hr = factory4->EnumAdapterByLuid(deviceLuid, IID_PPV_ARGS(&adapter1));
    if (FAILED(hr) || !adapter1) {
        impl_->unavailableReason =
            "no DXGI adapter matches the device LUID";
        if (error != nullptr) {
            *error = impl_->unavailableReason;
        }
        return false;
    }
    hr = adapter1.As(&impl_->adapter);
    if (FAILED(hr) || !impl_->adapter) {
        impl_->unavailableReason = "IDXGIAdapter3 is unavailable";
        if (error != nullptr) {
            *error = impl_->unavailableReason;
        }
        return false;
    }

    DXGI_ADAPTER_DESC3 description{};
    ComPtr<IDXGIAdapter4> adapter4;
    if (SUCCEEDED(adapter1.As(&adapter4)) && adapter4 &&
        SUCCEEDED(adapter4->GetDesc3(&description))) {
        impl_->adapterName = Narrow(description.Description);
        impl_->dedicatedVideoMemory = description.DedicatedVideoMemory;
        impl_->sharedSystemMemory = description.SharedSystemMemory;
    } else {
        DXGI_ADAPTER_DESC1 description1{};
        if (SUCCEEDED(adapter1->GetDesc1(&description1))) {
            impl_->adapterName = Narrow(description1.Description);
            impl_->dedicatedVideoMemory = description1.DedicatedVideoMemory;
        }
    }

    DXGI_QUERY_VIDEO_MEMORY_INFO info{};
    hr = impl_->adapter->QueryVideoMemoryInfo(
        0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info);
    if (FAILED(hr)) {
        impl_->unavailableReason =
            "IDXGIAdapter3::QueryVideoMemoryInfo(LOCAL) failed";
        if (error != nullptr) {
            *error = impl_->unavailableReason;
        }
        impl_->adapter.Reset();
        return false;
    }
    return true;
}

bool VramProbe::available() const { return impl_->adapter != nullptr; }

const std::string& VramProbe::unavailableReason() const {
    return impl_->unavailableReason;
}

const std::string& VramProbe::adapterName() const {
    return impl_->adapterName;
}

const std::string& VramProbe::adapterLuid() const { return impl_->luid; }

uint64_t VramProbe::dedicatedVideoMemory() const {
    return impl_->dedicatedVideoMemory;
}

uint64_t VramProbe::sharedSystemMemory() const {
    return impl_->sharedSystemMemory;
}

void VramProbe::Sample(const std::string& phase, uint32_t frame) {
    if (!available()) {
        return;
    }
    DXGI_QUERY_VIDEO_MEMORY_INFO local{};
    DXGI_QUERY_VIDEO_MEMORY_INFO nonLocal{};
    const HRESULT localHr = impl_->adapter->QueryVideoMemoryInfo(
        0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local);
    const HRESULT nonLocalHr = impl_->adapter->QueryVideoMemoryInfo(
        0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &nonLocal);
    if (FAILED(localHr)) {
        return;
    }
    VramSample sample;
    sample.phase = phase;
    sample.frame = frame;
    sample.localBudget = local.Budget;
    sample.localUsage = local.CurrentUsage;
    sample.localAvailableForReservation = local.AvailableForReservation;
    sample.localReservation = local.CurrentReservation;
    if (SUCCEEDED(nonLocalHr)) {
        sample.nonLocalBudget = nonLocal.Budget;
        sample.nonLocalUsage = nonLocal.CurrentUsage;
        sample.nonLocalAvailableForReservation =
            nonLocal.AvailableForReservation;
        sample.nonLocalReservation = nonLocal.CurrentReservation;
    }
    impl_->samples.push_back(std::move(sample));
}

const std::vector<VramSample>& VramProbe::samples() const {
    return impl_->samples;
}

const VramSample* VramProbe::PeakLocal() const {
    const VramSample* peak = nullptr;
    for (const VramSample& sample : impl_->samples) {
        if (peak == nullptr || sample.localUsage > peak->localUsage) {
            peak = &sample;
        }
    }
    return peak;
}

const VramSample* VramProbe::PeakNonLocal() const {
    const VramSample* peak = nullptr;
    for (const VramSample& sample : impl_->samples) {
        if (peak == nullptr || sample.nonLocalUsage > peak->nonLocalUsage) {
            peak = &sample;
        }
    }
    return peak;
}

std::string VramProbe::ToJson() const {
    std::string out;
    out.reserve(256 + impl_->samples.size() * 220);
    out += "{\"schema\":\"doom3-q4-vram-sample/1\",\"available\":";
    out += available() ? "true" : "false";
    out += ",\"adapter\":\"";
    out += JsonEscape(impl_->adapterName);
    out += "\",\"luid\":\"";
    out += JsonEscape(impl_->luid);
    out += "\",\"dedicatedVideoMemory\":";
    AppendU64(&out, impl_->dedicatedVideoMemory);
    out += ",\"sharedSystemMemory\":";
    AppendU64(&out, impl_->sharedSystemMemory);
    if (!available()) {
        out += ",\"unavailableReason\":\"";
        out += JsonEscape(impl_->unavailableReason);
        out += "\"}";
        return out;
    }
    const VramSample* peakLocal = PeakLocal();
    const VramSample* peakNonLocal = PeakNonLocal();
    out += ",\"sampleCount\":";
    AppendU64(&out, impl_->samples.size());
    if (peakLocal != nullptr) {
        out += ",\"peakLocal\":";
        AppendSample(&out, *peakLocal);
    }
    if (peakNonLocal != nullptr) {
        out += ",\"peakNonLocalUsage\":";
        AppendU64(&out, peakNonLocal->nonLocalUsage);
    }
    out += ",\"samples\":[";
    for (size_t i = 0; i < impl_->samples.size(); ++i) {
        if (i != 0) {
            out += ',';
        }
        AppendSample(&out, impl_->samples[i]);
    }
    out += "]}";
    return out;
}

}  // namespace q4
