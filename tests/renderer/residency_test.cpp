// T054 residency fixtures: budget, mip/quality selection, throttled upload and
// completion-safe release.
//
// The safety property is asserted directly: a release whose last use is not yet
// observed complete stays queued, and an upload that does not fit is deferred
// rather than satisfied by releasing a resource early.

#include "q4_test_support.h"
#include "rhi/residency.h"

#include <string>

namespace {

namespace rhi = doom3::rhi;

rhi::TextureRequest Request(uint32_t coarsestMip = 2) {
    rhi::TextureRequest request;
    request.id = 7;
    request.mipCount = 3;
    request.coarsestMip = coarsestMip;
    request.mips[0] = rhi::TextureMipInfo{64, 64, 400};
    request.mips[1] = rhi::TextureMipInfo{32, 32, 100};
    request.mips[2] = rhi::TextureMipInfo{16, 16, 25};
    return request;
}

void RunMipSelection(q4_test::Runner* runner) {
    rhi::MipSelection selection;
    std::string error;

    runner->Check(rhi::SelectMip(Request(), 500, &selection, &error) == rhi::Status::Ok,
                 "T054 full budget selects the finest mip", error);
    runner->Check(selection.mip == 0 && selection.bytes == 400 && !selection.reduced,
                 "T054 finest mip is level 0 at 400 bytes");

    runner->Check(rhi::SelectMip(Request(), 250, &selection, &error) == rhi::Status::Ok,
                 "T054 partial budget selects a coarser mip", error);
    runner->Check(selection.mip == 1 && selection.bytes == 100 && selection.reduced,
                 "T054 level 1 is selected and reported as reduced");

    runner->Check(rhi::SelectMip(Request(), 50, &selection, &error) == rhi::Status::Ok,
                 "T054 small budget selects the coarsest mip", error);
    runner->Check(selection.mip == 2 && selection.bytes == 25,
                 "T054 coarsest declared mip is selected");

    runner->Check(rhi::SelectMip(Request(1), 50, &selection, &error) ==
                     rhi::Status::Unsupported,
                 "T054 quality floor below the fitting mip is Unsupported");
    runner->Check(!selection.error.empty(), "T054 quality-floor refusal has a reason");

    rhi::TextureRequest malformed = Request();
    malformed.coarsestMip = 3;
    runner->Check(rhi::SelectMip(malformed, 500, &selection, &error) ==
                     rhi::Status::InvalidArgument,
                 "T054 minimum mip outside the chain is InvalidArgument");
    malformed = Request();
    malformed.id = 0;
    runner->Check(rhi::SelectMip(malformed, 500, &selection, &error) ==
                     rhi::Status::InvalidArgument,
                 "T054 texture without identity is InvalidArgument");
    malformed = Request();
    malformed.mipCount = 0;
    runner->Check(rhi::SelectMip(malformed, 500, &selection, &error) ==
                     rhi::Status::InvalidArgument,
                 "T054 empty mip chain is InvalidArgument");
}

void RunThrottle(q4_test::Runner* runner) {
    rhi::ResidencyPolicy policy;
    std::string error;
    runner->Check(policy.Configure(1000, 150, &error) == rhi::Status::Ok,
                 "T054 policy configures", error);
    runner->Check(policy.RequestUpload(1, 100) && policy.RequestUpload(2, 100),
                 "T054 two uploads are queued");
    runner->Check(!policy.RequestUpload(1, 100),
                 "T054 duplicate upload is refused");

    const rhi::ResidencyTick first = policy.Tick(0);
    runner->Check(first.uploadsStarted == 1 && first.uploadedBytes == 100,
                 "T054 first frame starts one upload", std::to_string(first.uploadsStarted));
    runner->Check(first.uploadsDeferred == 1,
                 "T054 second upload is deferred by the per-frame throttle");
    runner->Check(policy.residentBytes() == 100, "T054 resident bytes after first frame");

    const rhi::ResidencyTick second = policy.Tick(1);
    runner->Check(second.uploadsStarted == 1 && policy.residentBytes() == 200,
                 "T054 second frame starts the remaining upload");
    runner->Check(policy.pendingUploads() == 0, "T054 upload queue drains");
}

void RunBudgetAndRelease(q4_test::Runner* runner) {
    rhi::ResidencyPolicy policy;
    std::string error;
    runner->Check(policy.Configure(1000, 1000, &error) == rhi::Status::Ok,
                 "T054 budget policy configures", error);
    policy.RequestUpload(1, 400);
    policy.RequestUpload(2, 600);
    const rhi::ResidencyTick fill = policy.Tick(0);
    runner->Check(fill.uploadsStarted == 2 && fill.uploadedBytes == 1000,
                 "T054 two uploads fill the budget");
    runner->Check(policy.residentBytes() == 1000, "T054 resident bytes at budget");

    policy.RequestUpload(3, 100);
    const rhi::ResidencyTick blocked = policy.Tick(1);
    runner->Check(blocked.budgetBlocked == 1 && blocked.uploadsDeferred == 1,
                 "T054 an upload over budget is deferred and reported");
    runner->Check(policy.residentBytes() == 1000 &&
                     !policy.resident(3),
                 "T054 no resource is evicted to make room");

    runner->Check(policy.RequestRelease(1, 5), "T054 release is queued");
    const rhi::ResidencyTick early = policy.Tick(4);
    runner->Check(early.releasesDeferred == 1 && early.resourcesReleased == 0,
                 "T054 release before the last use completes is deferred");
    runner->Check(policy.resident(1) && policy.residentBytes() == 1000,
                 "T054 an in-flight resource stays resident");

    const rhi::ResidencyTick complete = policy.Tick(5);
    runner->Check(complete.resourcesReleased == 1 && complete.freedBytes == 400,
                 "T054 completed last use releases the resource");
    runner->Check(complete.uploadsStarted == 1 && policy.resident(3),
                 "T054 freed room admits the waiting upload");
    runner->Check(policy.residentBytes() == 700, "T054 resident bytes after release and upload");

    rhi::MipSelection selection;
    runner->Check(policy.SelectTextureMip(Request(), &selection, &error) == rhi::Status::Ok,
                 "T054 policy mip selection evaluates", error);
    runner->Check(selection.mip == 1 && selection.bytes == 100,
                 "T054 policy selects against the remaining budget");

    runner->Check(!policy.RequestRelease(99, 1),
                 "T054 release of a non-resident resource is refused");
}

void RunUnconfigured(q4_test::Runner* runner) {
    rhi::ResidencyPolicy policy;
    const rhi::ResidencyTick tick = policy.Tick(0);
    runner->Check(tick.uploadsDeferred == 0 && tick.releasesDeferred == 0,
                 "T054 unconfigured policy has no queued work");
    runner->Check(!policy.RequestUpload(1, 10),
                 "T054 unconfigured policy refuses uploads");
    std::string error;
    runner->Check(policy.Configure(0, 10, &error) == rhi::Status::InvalidArgument,
                 "T054 zero budget is InvalidArgument");
    runner->Check(policy.Configure(10, 0, &error) == rhi::Status::InvalidArgument,
                 "T054 zero upload budget is InvalidArgument");
}

}  // namespace

int main() {
    q4_test::Runner runner;
    RunMipSelection(&runner);
    RunThrottle(&runner);
    RunBudgetAndRelease(&runner);
    RunUnconfigured(&runner);
    return runner.Finish("residency");
}
