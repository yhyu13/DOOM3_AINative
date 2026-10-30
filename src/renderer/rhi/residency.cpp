// Texture residency policy (spec 004 T054).
//
// Pure bookkeeping: mip selection, a per-frame upload throttle and a
// completion-epoch release rule. No native synchronization object is named or
// touched here.

#include "rhi/residency.h"

namespace doom3 {
namespace rhi {

namespace {

Status Fail(std::string* error, Status status, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
    return status;
}

void ClearError(std::string* error) {
    if (error != nullptr) {
        error->clear();
    }
}

}  // namespace

Status SelectMip(const TextureRequest& request, uint64_t availableBytes, MipSelection* selection,
                 std::string* error) {
    if (selection == nullptr) {
        return Fail(error, Status::InvalidArgument, "SelectMip needs an output");
    }
    *selection = MipSelection();

    if (request.id == 0) {
        return Fail(error, Status::InvalidArgument, "texture request has no identity");
    }
    if (request.mipCount == 0 || request.mipCount > 16) {
        return Fail(error, Status::InvalidArgument, "texture request must declare 1..16 mips");
    }
    if (request.coarsestMip >= request.mipCount) {
        return Fail(error, Status::InvalidArgument,
                    "coarsest accepted mip is outside the declared mip chain");
    }
    for (uint32_t mip = 0; mip < request.mipCount; ++mip) {
        if (request.mips[mip].width == 0 || request.mips[mip].height == 0 ||
            request.mips[mip].bytes == 0) {
            return Fail(error, Status::InvalidArgument,
                        "every declared mip needs non-zero dimensions and bytes");
        }
    }

    for (uint32_t mip = 0; mip <= request.coarsestMip; ++mip) {
        if (request.mips[mip].bytes <= availableBytes) {
            selection->mip = mip;
            selection->bytes = request.mips[mip].bytes;
            selection->reduced = mip != 0;
            ClearError(error);
            return Status::Ok;
        }
    }

    selection->error = "no mip at or above the quality floor fits the remaining budget";
    if (error != nullptr) {
        *error = selection->error;
    }
    selection->status = Status::Unsupported;
    return Status::Unsupported;
}

Status ResidencyPolicy::Configure(uint64_t budgetBytes, uint64_t uploadBytesPerFrame,
                                  std::string* error) {
    if (budgetBytes == 0 || uploadBytesPerFrame == 0) {
        return Fail(error, Status::InvalidArgument,
                    "residency budget and per-frame upload budget must be positive");
    }
    configured_ = true;
    budgetBytes_ = budgetBytes;
    uploadBytesPerFrame_ = uploadBytesPerFrame;
    residentBytes_ = 0;
    resident_.clear();
    uploads_.clear();
    releases_.clear();
    ClearError(error);
    return Status::Ok;
}

bool ResidencyPolicy::resident(uint32_t id) const {
    return resident_.find(id) != resident_.end();
}

Status ResidencyPolicy::SelectTextureMip(const TextureRequest& request, MipSelection* selection,
                                         std::string* error) const {
    if (!configured_) {
        return Fail(error, Status::NotReady, "residency policy is not configured");
    }
    const uint64_t available = residentBytes_ < budgetBytes_ ? budgetBytes_ - residentBytes_ : 0;
    return SelectMip(request, available, selection, error);
}

bool ResidencyPolicy::RequestUpload(uint32_t id, uint64_t bytes) {
    if (!configured_ || id == 0 || bytes == 0) {
        return false;
    }
    if (resident(id)) {
        return false;
    }
    for (const Upload& upload : uploads_) {
        if (upload.id == id) {
            return false;
        }
    }
    uploads_.push_back(Upload{id, bytes});
    return true;
}

bool ResidencyPolicy::RequestRelease(uint32_t id, uint32_t lastUseEpoch) {
    if (!configured_ || !resident(id)) {
        return false;
    }
    for (const Release& release : releases_) {
        if (release.id == id) {
            return false;
        }
    }
    Release release;
    release.id = id;
    release.bytes = resident_[id];
    release.lastUseEpoch = lastUseEpoch;
    releases_.push_back(release);
    return true;
}

ResidencyTick ResidencyPolicy::Tick(uint32_t completedEpoch) {
    ResidencyTick tick;
    if (!configured_) {
        tick.uploadsDeferred = static_cast<uint32_t>(uploads_.size());
        tick.releasesDeferred = static_cast<uint32_t>(releases_.size());
        return tick;
    }

    // Releases first: a completed last use frees room for this frame's uploads.
    // An outstanding last use is kept pending, never performed early.
    std::deque<Release> stillPending;
    for (const Release& release : releases_) {
        if (release.lastUseEpoch <= completedEpoch) {
            const auto found = resident_.find(release.id);
            if (found != resident_.end()) {
                const uint64_t bytes = found->second;
                residentBytes_ -= bytes;
                resident_.erase(found);
                tick.freedBytes += bytes;
                ++tick.resourcesReleased;
            }
            continue;
        }
        ++tick.releasesDeferred;
        stillPending.push_back(release);
    }
    releases_.swap(stillPending);

    // Uploads in order, bounded by both budgets. Anything that still does not
    // fit stays queued; the policy never evicts a resource the caller did not
    // ask it to release.
    uint32_t deferred = 0;
    uint32_t blocked = 0;
    std::deque<Upload> stillQueued;
    while (!uploads_.empty()) {
        const Upload upload = uploads_.front();
        const bool frameRoom = tick.uploadedBytes + upload.bytes <= uploadBytesPerFrame_;
        const bool totalRoom = residentBytes_ + upload.bytes <= budgetBytes_;
        if (frameRoom && totalRoom) {
            uploads_.pop_front();
            resident_[upload.id] = upload.bytes;
            residentBytes_ += upload.bytes;
            tick.uploadedBytes += upload.bytes;
            ++tick.uploadsStarted;
            continue;
        }
        if (!totalRoom) {
            ++blocked;
        }
        // Keep the remaining order stable and stop: a later small upload may
        // not jump a larger one, so the policy stays FIFO and predictable.
        while (!uploads_.empty()) {
            stillQueued.push_back(uploads_.front());
            uploads_.pop_front();
        }
        break;
    }
    uploads_.swap(stillQueued);
    deferred = static_cast<uint32_t>(uploads_.size());
    tick.uploadsDeferred = deferred;
    tick.budgetBlocked = blocked;
    return tick;
}

}  // namespace rhi
}  // namespace doom3
