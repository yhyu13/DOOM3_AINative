#pragma once

// Process video-memory probe for the Q4 harness.
//
// T056's remaining gap was the VRAM peak: the adapter's reported dedicated
// memory (134,217,728 bytes on the measured host) is a capacity figure, not a
// use figure. This probe asks DXGI for the calling process's own usage with
// `IDXGIAdapter3::QueryVideoMemoryInfo`, samples it while the harness runs, and
// keeps the peak. It never substitutes the adapter capacity for the peak.
//
// The probe is opt-in (`-VramProbe`). It is intentionally independent of the
// harness's resource creation: no allocation is attributed to a resource, the
// numbers are whatever the OS reports for this process on this adapter node.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct ID3D12Device;

namespace q4 {

struct VramSample {
    std::string phase;  // harness_ready, materials_loaded, scene_start, frame
    uint32_t frame = 0;
    uint64_t localBudget = 0;
    uint64_t localUsage = 0;
    uint64_t localAvailableForReservation = 0;
    uint64_t localReservation = 0;
    uint64_t nonLocalBudget = 0;
    uint64_t nonLocalUsage = 0;
    uint64_t nonLocalAvailableForReservation = 0;
    uint64_t nonLocalReservation = 0;
};

class VramProbe {
public:
    VramProbe();
    ~VramProbe();

    VramProbe(const VramProbe&) = delete;
    VramProbe& operator=(const VramProbe&) = delete;

    // Resolves the DXGI adapter that owns `device` by LUID and queries
    // IDXGIAdapter3. Returns false with a reason when the adapter or the
    // interface is unavailable (for example a software adapter).
    bool Initialize(ID3D12Device* device, std::string* error);

    bool available() const;
    const std::string& unavailableReason() const;
    const std::string& adapterName() const;
    const std::string& adapterLuid() const;
    uint64_t dedicatedVideoMemory() const;
    uint64_t sharedSystemMemory() const;

    // Appends one sample. No-op when the probe is unavailable.
    void Sample(const std::string& phase, uint32_t frame);

    const std::vector<VramSample>& samples() const;
    const VramSample* PeakLocal() const;
    const VramSample* PeakNonLocal() const;

    // One JSON object holding identity, every sample and the two peaks.
    std::string ToJson() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace q4
