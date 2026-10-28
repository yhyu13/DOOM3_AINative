#include "q1a_test_support.h"

#include <string>

using doom3::rhi::ClearReadbackDesc;
using doom3::rhi::Device;
using doom3::rhi::DeviceOptions;
using doom3::rhi::ReadbackResult;
using doom3::rhi::ReleaseStatus;
using doom3::rhi::Status;
using doom3::rhi::Submission;

namespace {

constexpr uint32_t kLargeWidth = 2048;
constexpr uint32_t kLargeHeight = 2048;

// Compare a bounded set of rows from a large synthetic readback. The per-byte
// criterion is the same one used by the small clear test; the sample keeps the
// retained log readable while covering the first and last rows.
bool CompareSampledRows(const ReadbackResult& result, uint32_t expectedHeight,
                        std::string* detail) {
    bool ok = q1a_test::Require(result.width == kLargeWidth &&
                                    result.height == expectedHeight,
                                "large readback dimensions are wrong");
    ok &= q1a_test::Require(result.rowStride == kLargeWidth * 4,
                            "large readback stride is not width * 4");
    if (!ok) {
        if (detail != nullptr) {
            *detail = "large readback shape is wrong";
        }
        return false;
    }
    const uint32_t sampleRows[] = {0, 1, 2, 3, expectedHeight / 2,
                                   expectedHeight - 2, expectedHeight - 1};
    for (uint32_t row : sampleRows) {
        if (row >= expectedHeight) {
            continue;
        }
        const uint8_t* actual =
            result.pixels.data() + static_cast<size_t>(row) * result.rowStride;
        const uint8_t* pixel = q1a_test::ExpectedRow(row);
        uint32_t mismatches = 0;
        uint32_t maxDelta = 0;
        for (uint32_t i = 0; i < result.rowStride; ++i) {
            const int delta =
                std::abs(static_cast<int>(actual[i]) - static_cast<int>(pixel[i & 3]));
            maxDelta = std::max(maxDelta, static_cast<uint32_t>(delta));
            if (delta > q1a_test::kTolerance) {
                ++mismatches;
            }
        }
        q1a_test::Log("LARGE_ROW %u mismatches=%u max_delta=%u %s", row, mismatches,
                      maxDelta, mismatches == 0 ? "OK" : "FAIL");
        if (mismatches != 0) {
            ok = false;
        }
    }
    if (detail != nullptr) {
        *detail = ok ? "sampled large rows are within tolerance"
                     : "a sampled large row exceeded tolerance";
    }
    return ok;
}

int RunLifetime(Device& device) {
    bool ok = true;
    std::string error;

    const ClearReadbackDesc large =
        q1a_test::MakeLargeDesc(kLargeWidth, kLargeHeight);
    const ClearReadbackDesc small = q1a_test::MakePatternDesc();

    Submission first;
    Submission second;
    Status status = device.SubmitClearReadback(large, &first, &error);
    q1a_test::Log("LIFETIME first_submit=%s reason=%s",
                  doom3::rhi::ToString(status), error.c_str());
    ok &= q1a_test::Require(status == Status::Ok, "first SubmitClearReadback failed");
    if (status != Status::Ok) {
        return 1;
    }
    q1a_test::PrintSubmission("first", first);

    status = device.SubmitClearReadback(large, &second, &error);
    q1a_test::Log("LIFETIME second_submit=%s reason=%s",
                  doom3::rhi::ToString(status), error.c_str());
    ok &= q1a_test::Require(status == Status::Ok, "second SubmitClearReadback failed");
    if (status != Status::Ok) {
        return 1;
    }
    q1a_test::PrintSubmission("second", second);
    ok &= q1a_test::Require(first.id != second.id, "two submissions reused an id");
    ok &= q1a_test::Require(first.completionToken != second.completionToken,
                            "two submissions reused a completion token");

    // Attempt release immediately, with no intervening wait. The later
    // submission is queued behind the first, so at least one attempt must land
    // while its token is still incomplete.
    bool observedOutstanding = false;
    const Submission* order[] = {&second, &first};
    const char* labels[] = {"second", "first"};
    for (int attempt = 0; attempt < 2; ++attempt) {
        const Submission& candidate = *order[attempt];
        const bool complete = device.IsComplete(candidate);
        const ReleaseStatus release = device.TryRelease(candidate, &error);
        q1a_test::Log("LIFETIME in_flight_attempt submission=%s complete=%s release=%s "
                      "reason=%s",
                      labels[attempt], complete ? "true" : "false",
                      doom3::rhi::ToString(release), error.c_str());
        if (!complete && release == ReleaseStatus::RefusedOutstanding) {
            observedOutstanding = true;
        }
    }
    ok &= q1a_test::Require(
        observedOutstanding,
        "no release attempt observed an incomplete completion token; the in-flight "
        "assertion was not exercised");

    ok &= q1a_test::Require(q1a_test::WaitComplete(device, first),
                            "first completion token was not reached");
    ok &= q1a_test::Require(q1a_test::WaitComplete(device, second),
                            "second completion token was not reached");

    ReadbackResult firstReadback;
    status = device.Readback(first, &firstReadback, &error);
    q1a_test::Log("LIFETIME first_readback=%s reason=%s",
                  doom3::rhi::ToString(status), error.c_str());
    ok &= q1a_test::Require(status == Status::Ok, "first Readback failed");
    if (status == Status::Ok) {
        ok &= CompareSampledRows(firstReadback, kLargeHeight, &error);
    }

    ReadbackResult secondReadback;
    status = device.Readback(second, &secondReadback, &error);
    q1a_test::Log("LIFETIME second_readback=%s reason=%s",
                  doom3::rhi::ToString(status), error.c_str());
    ok &= q1a_test::Require(status == Status::Ok, "second Readback failed");
    if (status == Status::Ok) {
        ok &= CompareSampledRows(secondReadback, kLargeHeight, &error);
    }

    const ReleaseStatus firstRelease = device.TryRelease(first, &error);
    const ReleaseStatus secondRelease = device.TryRelease(second, &error);
    q1a_test::Log("LIFETIME releases first=%s second=%s",
                  doom3::rhi::ToString(firstRelease),
                  doom3::rhi::ToString(secondRelease));
    ok &= q1a_test::Require(firstRelease == ReleaseStatus::Released &&
                                secondRelease == ReleaseStatus::Released,
                            "completed submissions were not released");

    // Readback before completion is NotReady, not a silent success. Submit a
    // third copy and poll it, but first assert the immediate state is honest on
    // at least one fresh submission when it is still running.
    Submission third;
    status = device.SubmitClearReadback(small, &third, &error);
    ok &= q1a_test::Require(status == Status::Ok, "third SubmitClearReadback failed");
    if (status == Status::Ok) {
        ReadbackResult early;
        const Status earlyStatus = device.Readback(third, &early, &error);
        q1a_test::Log("LIFETIME early_readback=%s reason=%s",
                      doom3::rhi::ToString(earlyStatus), error.c_str());
        if (!device.IsComplete(third)) {
            ok &= q1a_test::Require(earlyStatus == Status::NotReady,
                                    "Readback before completion did not return NotReady");
        } else {
            q1a_test::Log("LIFETIME early_readback observation skipped=already_complete");
        }
        ok &= q1a_test::Require(q1a_test::WaitComplete(device, third),
                                "third completion token was not reached");
        ok &= q1a_test::Require(device.TryRelease(third, &error) ==
                                    ReleaseStatus::Released,
                                "third submission was not released");
    }
    return ok ? 0 : 1;
}

int RunTokenIdentity(Device& device) {
    bool ok = true;
    std::string error;
    const ClearReadbackDesc desc = q1a_test::MakePatternDesc();
    DeviceOptions options;
    options.adapterClass = device.adapter().adapterClass;

    Submission live;
    Status status = device.SubmitClearReadback(desc, &live, &error);
    ok &= q1a_test::Require(status == Status::Ok, "live submit failed");
    if (status != Status::Ok) {
        return 1;
    }
    ok &= q1a_test::Require(q1a_test::WaitComplete(device, live),
                            "live submission did not complete");

    Submission wrongToken = live;
    wrongToken.completionToken += 1;
    ReadbackResult scratch;
    const bool wrongComplete = device.IsComplete(wrongToken);
    const Status wrongRead = device.Readback(wrongToken, &scratch, &error);
    const ReleaseStatus wrongRelease = device.TryRelease(wrongToken, &error);
    q1a_test::Log("TOKEN mutated complete=%s readback=%s release=%s",
                  wrongComplete ? "true" : "false", doom3::rhi::ToString(wrongRead),
                  doom3::rhi::ToString(wrongRelease));
    ok &= q1a_test::Require(!wrongComplete, "a mutated token reported completion");
    ok &= q1a_test::Require(wrongRead == Status::InvalidArgument,
                            "Readback accepted a mutated token");
    ok &= q1a_test::Require(wrongRelease == ReleaseStatus::InvalidSubmission,
                            "TryRelease accepted a mutated token");

    Device other;
    status = other.Initialize(options, &error);
    ok &= q1a_test::Require(status == Status::Ok, "foreign device initialization failed");
    if (status == Status::Ok) {
        Submission foreign;
        status = other.SubmitClearReadback(desc, &foreign, &error);
        ok &= q1a_test::Require(status == Status::Ok, "foreign submit failed");
        if (status == Status::Ok) {
            ok &= q1a_test::Require(q1a_test::WaitComplete(other, foreign),
                                    "foreign submission did not complete");
            const bool foreignComplete = device.IsComplete(foreign);
            const Status foreignRead = device.Readback(foreign, &scratch, &error);
            const ReleaseStatus foreignRelease = device.TryRelease(foreign, &error);
            q1a_test::Log("TOKEN foreign complete=%s readback=%s release=%s",
                          foreignComplete ? "true" : "false",
                          doom3::rhi::ToString(foreignRead),
                          doom3::rhi::ToString(foreignRelease));
            ok &= q1a_test::Require(!foreignComplete,
                                    "a foreign token reported completion");
            ok &= q1a_test::Require(foreignRead == Status::InvalidArgument,
                                    "Readback accepted a foreign token");
            ok &= q1a_test::Require(foreignRelease == ReleaseStatus::InvalidSubmission,
                                    "TryRelease accepted a foreign token");
        }
        ok &= q1a_test::Require(other.Shutdown() == Status::Ok,
                                "foreign device did not shut down cleanly");
    }

    ok &= q1a_test::Require(device.TryRelease(live, &error) == ReleaseStatus::Released,
                            "live submission was not released");

    // Reinitialize the same Device and confirm an old token is still rejected.
    ok &= q1a_test::Require(device.Shutdown() == Status::Ok, "pre-reinit shutdown failed");
    status = device.Initialize(options, &error);
    ok &= q1a_test::Require(status == Status::Ok, "reinitialize failed");
    if (status == Status::Ok) {
        const bool staleComplete = device.IsComplete(live);
        const Status staleRead = device.Readback(live, &scratch, &error);
        const ReleaseStatus staleRelease = device.TryRelease(live, &error);
        q1a_test::Log("TOKEN stale complete=%s readback=%s release=%s",
                      staleComplete ? "true" : "false",
                      doom3::rhi::ToString(staleRead),
                      doom3::rhi::ToString(staleRelease));
        ok &= q1a_test::Require(!staleComplete, "a stale token reported completion");
        ok &= q1a_test::Require(staleRead == Status::InvalidArgument,
                                "Readback accepted a stale token");
        ok &= q1a_test::Require(staleRelease == ReleaseStatus::InvalidSubmission,
                                "TryRelease accepted a stale token");
    }
    return ok ? 0 : 1;
}

int RunShutdownOutstanding(Device& device) {
    std::string error;
    const ClearReadbackDesc large =
        q1a_test::MakeLargeDesc(1024, 1024);
    Submission submission;
    const Status submit = device.SubmitClearReadback(large, &submission, &error);
    q1a_test::Log("SHUTDOWN submit=%s reason=%s", doom3::rhi::ToString(submit),
                  error.c_str());
    bool ok = q1a_test::Require(submit == Status::Ok,
                                "shutdown test could not submit work");
    if (submit != Status::Ok) {
        return 1;
    }
    q1a_test::PrintSubmission("shutdown", submission);
    const bool completeBefore = device.IsComplete(submission);
    const Status shutdown = device.Shutdown();
    q1a_test::Log("SHUTDOWN complete_before=%s status=%s",
                  completeBefore ? "true" : "false", doom3::rhi::ToString(shutdown));
    ok &= q1a_test::Require(shutdown == Status::Ok,
                            "Shutdown with outstanding work did not complete it");
    ok &= q1a_test::Require(!device.IsInitialized(),
                            "device reports initialized after Shutdown");
    const Status second = device.Shutdown();
    q1a_test::Log("SHUTDOWN second_status=%s", doom3::rhi::ToString(second));
    ok &= q1a_test::Require(second == Status::Ok, "second Shutdown failed");
    return ok ? 0 : 1;
}

int RunIncompleteSetup() {
    bool ok = true;
    for (doom3::rhi::AdapterClass adapterClass :
         {doom3::rhi::AdapterClass::Hardware, doom3::rhi::AdapterClass::Software}) {
        Device device;
        DeviceOptions options;
        options.adapterClass = adapterClass;
        options.adapterIndex = 9999;
        std::string error;
        const Status status = device.Initialize(options, &error);
        q1a_test::Log("INCOMPLETE_SETUP class=%s status=%s reason=%s",
                      doom3::rhi::ToString(adapterClass), doom3::rhi::ToString(status),
                      error.c_str());
        ok &= q1a_test::Require(status == Status::Unsupported,
                                "out-of-range adapter index did not return Unsupported");
        ok &= q1a_test::Require(!device.IsInitialized(),
                                "device reports initialized after a failed Initialize");
        const Status firstShutdown = device.Shutdown();
        const Status secondShutdown = device.Shutdown();
        q1a_test::Log("INCOMPLETE_SETUP class=%s first_shutdown=%s second_shutdown=%s",
                      doom3::rhi::ToString(adapterClass),
                      doom3::rhi::ToString(firstShutdown),
                      doom3::rhi::ToString(secondShutdown));
        ok &= q1a_test::Require(firstShutdown == Status::Ok && secondShutdown == Status::Ok,
                                "Shutdown after a failed Initialize was not clean");
    }
    return ok ? 0 : 1;
}

}  // namespace

int main() {
    const int lifetime = q1a_test::ForEachAdapter(RunLifetime);
    const int identity = q1a_test::ForEachAdapter(RunTokenIdentity);
    const int outstanding = q1a_test::ForEachAdapter(RunShutdownOutstanding);
    const int incomplete = RunIncompleteSetup();
    if (lifetime == 1 || identity == 1 || outstanding == 1 || incomplete == 1) {
        return 1;
    }
    if (lifetime == q1a_test::kSkipExitCode && identity == q1a_test::kSkipExitCode &&
        outstanding == q1a_test::kSkipExitCode) {
        return q1a_test::kSkipExitCode;
    }
    return 0;
}
