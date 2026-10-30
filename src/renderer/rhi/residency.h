#pragma once

// Texture residency policy for Q4 (spec 004 T054, FR-013).
//
// The policy decides, in portable code, which mip level fits, how many bytes
// may be uploaded per frame, and when a resident resource may be released. It
// is intentionally a bookkeeping policy: the backend lowers "released" to its
// own completion observation, and this unit never names a native
// synchronization object.
//
// The safety rule is the point of the unit:
//  * a resource is released only after its last-use completion epoch is
//    observed complete; otherwise the release stays pending and is reported as
//    deferred, never performed early;
//  * an upload that would exceed the byte budget is deferred, not satisfied by
//    an unsafe release;
//  * a mip that does not fit is reported, never silently substituted by an
//    undeclared quality drop below the caller's floor.

#include "rhi/rhi.h"

#include <cstdint>
#include <deque>
#include <map>
#include <string>

namespace doom3 {
namespace rhi {

struct TextureMipInfo {
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t bytes = 0;
};

struct TextureRequest {
    uint32_t id = 0;
    // Number of valid entries in `mips`, finest first. At most 16.
    uint32_t mipCount = 0;
    // The coarsest mip the caller accepts. A finer mip is always preferred;
    // nothing coarser than this level may be selected.
    uint32_t coarsestMip = 15;
    TextureMipInfo mips[16];
};

struct MipSelection {
    Status status = Status::Ok;
    std::string error;
    uint32_t mip = 0;
    uint64_t bytes = 0;
    // True when the finest mip did not fit and a coarser one was selected.
    bool reduced = false;
};

// Chooses the finest mip whose bytes fit `availableBytes` and which is no
// coarser than `request.coarsestMip`. No mip fits -> Unsupported with the
// reason; a malformed request -> InvalidArgument.
Status SelectMip(const TextureRequest& request, uint64_t availableBytes, MipSelection* selection,
                 std::string* error);

struct ResidencyTick {
    uint64_t uploadedBytes = 0;
    uint64_t freedBytes = 0;
    uint32_t uploadsStarted = 0;
    uint32_t uploadsDeferred = 0;
    uint32_t resourcesReleased = 0;
    uint32_t releasesDeferred = 0;
    // A deferred upload that could not fit even after every eligible release.
    uint32_t budgetBlocked = 0;
};

class ResidencyPolicy {
public:
    // Both values must be > 0. Resets every queue and resident entry.
    Status Configure(uint64_t budgetBytes, uint64_t uploadBytesPerFrame, std::string* error);

    uint64_t budgetBytes() const { return budgetBytes_; }
    uint64_t uploadBytesPerFrame() const { return uploadBytesPerFrame_; }
    uint64_t residentBytes() const { return residentBytes_; }
    bool resident(uint32_t id) const;
    uint32_t pendingUploads() const { return static_cast<uint32_t>(uploads_.size()); }
    uint32_t pendingReleases() const { return static_cast<uint32_t>(releases_.size()); }

    // Picks a mip against the remaining budget. Does not reserve it.
    Status SelectTextureMip(const TextureRequest& request, MipSelection* selection,
                            std::string* error) const;

    // Queues an upload. False for a duplicate resident/pending id, zero bytes or
    // an unconfigured policy.
    bool RequestUpload(uint32_t id, uint64_t bytes);

    // Queues a release of a resident resource. `lastUseEpoch` is the epoch of
    // the last submission that read it. False when the id is not resident, is
    // already pending a release, or the policy is unconfigured.
    bool RequestRelease(uint32_t id, uint32_t lastUseEpoch);

    // Processes releases whose last use is at or before `completedEpoch`, then
    // starts uploads in order while the per-frame and total budgets allow.
    // Deferred work stays queued.
    ResidencyTick Tick(uint32_t completedEpoch);

private:
    struct Upload {
        uint32_t id = 0;
        uint64_t bytes = 0;
    };
    struct Release {
        uint32_t id = 0;
        uint64_t bytes = 0;
        uint32_t lastUseEpoch = 0;
    };

    bool configured_ = false;
    uint64_t budgetBytes_ = 0;
    uint64_t uploadBytesPerFrame_ = 0;
    uint64_t residentBytes_ = 0;
    std::map<uint32_t, uint64_t> resident_;
    std::deque<Upload> uploads_;
    std::deque<Release> releases_;
};

}  // namespace rhi
}  // namespace doom3
