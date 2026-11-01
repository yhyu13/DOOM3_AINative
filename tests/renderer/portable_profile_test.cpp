// T072 portable capability/fallback contract fixtures.
//
// These fixtures encode the contract, not the implementation's output. The
// required limit table, the optional fallback ids and the excluded classes are
// independent literals copied from
// specs/004-quality-renderer/portable-profile-contract.md sections 3, 4 and 6,
// so a change to the executable table without a contract change fails here.
//
// The negative controls fail when selection silently upgrades a class the
// contract refuses: an advertised capability is not permission, a feature that
// is available does not mask a short required limit, and a quality fallback
// degrades the profile instead of being ignored.

#include "rhi/capability_profile.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

namespace rhi = doom3::rhi;

template <typename T, size_t N>
constexpr size_t CountOf(const T (&)[N]) {
    return N;
}

class Runner {
public:
    void Check(bool condition, const std::string& label,
               const std::string& detail = std::string()) {
        ++checks_;
        if (condition) {
            std::printf("PASS %s\n", label.c_str());
        } else {
            ++failures_;
            if (detail.empty()) {
                std::printf("FAIL %s\n", label.c_str());
            } else {
                std::printf("FAIL %s: %s\n", label.c_str(), detail.c_str());
            }
        }
        std::fflush(stdout);
    }

    int Finish(const char* name) const {
        std::printf("SUMMARY %s checks=%d failures=%d\n", name, checks_, failures_);
        return failures_ == 0 ? 0 : 1;
    }

private:
    int checks_ = 0;
    int failures_ = 0;
};

// Section 3: required limits and their direction.
struct ExpectedLimit {
    const char* name;
    uint64_t requirement;
    bool atLeast;
};

const ExpectedLimit kExpectedLimits[] = {
    {"maxBufferSize", 268435456u, true},
    {"maxStorageBufferBindingSize", 134217728u, true},
    {"maxTextureDimension2D", 8192u, true},
    {"maxBindGroups", 4u, true},
    {"maxSampledTexturesPerShaderStage", 16u, true},
    {"maxStorageBuffersPerShaderStage", 8u, true},
    {"maxUniformBuffersPerShaderStage", 12u, true},
    {"maxUniformBufferBindingSize", 65536u, true},
    {"minUniformBufferOffsetAlignment", 256u, false},
    {"maxVertexBuffers", 8u, true},
    {"maxVertexAttributes", 16u, true},
    {"maxColorAttachments", 8u, true},
    {"maxColorAttachmentBytesPerSample", 32u, true},
    {"maxComputeInvocationsPerWorkgroup", 256u, true},
};

// Section 4: optional capabilities and their declared fallbacks.
struct ExpectedFeature {
    const char* name;
    const char* fallbackId;
    rhi::FallbackImpact impact;
};

const ExpectedFeature kExpectedOptional[] = {
    {"timestamp-query", "cpu-frame-timing", rhi::FallbackImpact::Info},
    {"texture-compression-bc", "bc-transcode-required", rhi::FallbackImpact::Quality},
    {"depth-clip-control", "no-clip-plane", rhi::FallbackImpact::Info},
    {"float32-filterable", "nearest-or-half-float", rhi::FallbackImpact::Info},
};

// Section 6 plus the native-interop tier: classes the profile refuses.
struct ExpectedExcluded {
    const char* name;
};

const ExpectedExcluded kExpectedExcluded[] = {
    {"ray-tracing"},
    {"neural-upscaler"},
    {"bindless-bindings"},
    {"multiple-queues"},
    {"sparse-resources"},
    {"resource-aliasing"},
    {"depth-bounds"},
    {"polygon-mode"},
    {"fixed-function-alpha-test"},
    {"texgen"},
    {"legacy-l8-la8-a8"},
    {"guaranteed-bc"},
    {"native-provider-interop"},
    {"native-interop"},
};

std::string LimitId(const char* name) {
    return std::string("limit:") + name;
}

std::string FeatureId(const char* name) {
    return std::string("feature:") + name;
}

std::string ClassId(const char* name) {
    return std::string("class:") + name;
}

bool Contains(const std::vector<std::string>& values, const std::string& value) {
    for (const std::string& entry : values) {
        if (entry == value) {
            return true;
        }
    }
    return false;
}

// A request that meets every required limit exactly at its threshold, advertises
// every optional feature, names one target and declares evidence. Individual
// fixtures mutate exactly one fact so each refusal is attributable.
rhi::ProfileRequest ConformingRequest() {
    rhi::ProfileRequest request;
    request.expectedTargetId = "portable-contract-fixture";
    request.declared.targetId = "portable-contract-fixture";
    request.declared.backend = "contract-fixture-backend/1";
    request.declared.contractVersion = rhi::kPortableContractVersion;
    request.declared.evidence = "specs/004-quality-renderer/evidence-q7.md";

    for (const ExpectedLimit& limit : kExpectedLimits) {
        rhi::AdvertisedLimit advertised;
        advertised.name = limit.name;
        advertised.reported = true;
        advertised.value = limit.requirement;
        request.advertised.limits.push_back(advertised);
    }
    for (const ExpectedFeature& feature : kExpectedOptional) {
        request.advertised.features.push_back(feature.name);
    }
    return request;
}

void SetLimitValue(rhi::ProfileRequest* request, const char* name, uint64_t value) {
    for (rhi::AdvertisedLimit& limit : request->advertised.limits) {
        if (limit.name == name) {
            limit.reported = true;
            limit.value = value;
            return;
        }
    }
}

void MarkLimitUnreported(rhi::ProfileRequest* request, const char* name) {
    for (rhi::AdvertisedLimit& limit : request->advertised.limits) {
        if (limit.name == name) {
            limit.reported = false;
            limit.value = 0;
            return;
        }
    }
}

void DropFeature(rhi::ProfileRequest* request, const char* name) {
    std::vector<std::string>& features = request->advertised.features;
    for (std::vector<std::string>::iterator it = features.begin();
         it != features.end(); ++it) {
        if (*it == name) {
            features.erase(it);
            return;
        }
    }
}

bool SelectOk(const rhi::ProfileRequest& request, rhi::ProfileSelection* selection) {
    std::string error;
    return rhi::SelectCapabilityProfile(request, selection, &error) ==
           rhi::Status::Ok;
}

// ---------------------------------------------------------------------------
// contract_tables: the executable table matches the published document.
// ---------------------------------------------------------------------------
void RunContractTables(Runner* runner) {
    const std::vector<rhi::LimitRule>& limits = rhi::RequiredLimitRules();
    runner->Check(limits.size() == CountOf(kExpectedLimits),
                  "T072 contract publishes 14 required limits",
                  std::to_string(limits.size()));
    for (size_t index = 0; index < CountOf(kExpectedLimits) && index < limits.size();
         ++index) {
        const ExpectedLimit& expected = kExpectedLimits[index];
        const rhi::LimitRule& actual = limits[index];
        runner->Check(std::string(actual.name) == expected.name,
                      std::string("T072 required limit ") +
                          std::to_string(index + 1) + " is " + expected.name,
                      actual.name);
        runner->Check(actual.requirement == expected.requirement,
                      std::string("T072 ") + expected.name + " requirement is " +
                          std::to_string(expected.requirement),
                      std::to_string(actual.requirement));
        runner->Check(actual.atLeast == expected.atLeast,
                      std::string("T072 ") + expected.name +
                          (expected.atLeast ? " is at-least" : " is at-most"));
        runner->Check(actual.reason != nullptr && actual.reason[0] != '\0',
                      std::string("T072 ") + expected.name +
                          " publishes a reason");
    }

    const std::vector<rhi::FeatureRule>& optional = rhi::OptionalFeatureRules();
    runner->Check(optional.size() == CountOf(kExpectedOptional),
                  "T072 contract publishes 4 optional features",
                  std::to_string(optional.size()));
    for (size_t index = 0; index < CountOf(kExpectedOptional) && index < optional.size();
         ++index) {
        const ExpectedFeature& expected = kExpectedOptional[index];
        const rhi::FeatureRule& actual = optional[index];
        runner->Check(std::string(actual.name) == expected.name,
                      std::string("T072 optional feature ") +
                          std::to_string(index + 1) + " is " + expected.name,
                      actual.name);
        runner->Check(std::string(actual.fallbackId) == expected.fallbackId,
                      std::string("T072 ") + expected.name + " fallback is " +
                          expected.fallbackId,
                      actual.fallbackId);
        runner->Check(actual.impact == expected.impact,
                      std::string("T072 ") + expected.name + " impact is " +
                          rhi::ToString(expected.impact),
                      rhi::ToString(actual.impact));
        runner->Check(actual.reason != nullptr && actual.reason[0] != '\0',
                      std::string("T072 ") + expected.name +
                          " publishes a fallback reason");
    }
    for (size_t left = 0; left < optional.size(); ++left) {
        for (size_t right = left + 1; right < optional.size(); ++right) {
            runner->Check(std::string(optional[left].fallbackId) !=
                              optional[right].fallbackId,
                          std::string("T072 fallback ids are unique: ") +
                              optional[left].fallbackId);
        }
    }

    const std::vector<rhi::ExcludedClass>& excluded = rhi::ExcludedClasses();
    runner->Check(excluded.size() == CountOf(kExpectedExcluded),
                  "T072 contract publishes 14 excluded classes",
                  std::to_string(excluded.size()));
    for (size_t index = 0;
         index < CountOf(kExpectedExcluded) && index < excluded.size(); ++index) {
        const ExpectedExcluded& expected = kExpectedExcluded[index];
        const rhi::ExcludedClass& actual = excluded[index];
        runner->Check(std::string(actual.name) == expected.name,
                      std::string("T072 excluded class ") +
                          std::to_string(index + 1) + " is " + expected.name,
                      actual.name);
        runner->Check(actual.reason != nullptr && actual.reason[0] != '\0',
                      std::string("T072 ") + expected.name +
                          " publishes a refusal reason");
    }
}

// ---------------------------------------------------------------------------
// required_limits: every required class passes at its threshold and refuses
// when unreported or one step outside the requirement.
// ---------------------------------------------------------------------------
void RunRequiredLimits(Runner* runner) {
    const rhi::ProfileRequest baseline = ConformingRequest();

    rhi::ProfileSelection clean;
    runner->Check(SelectOk(baseline, &clean),
                  "T072 conforming limit table evaluates");
    runner->Check(clean.status == rhi::ProfileStatus::Ready,
                  "T072 a conforming table is READY");
    runner->Check(clean.accepted && clean.supportClaimAllowed,
                  "T072 a conforming table with evidence may support a claim");
    runner->Check(clean.refusalReasons.empty(),
                  "T072 a conforming table has no refusal");
    for (const ExpectedLimit& limit : kExpectedLimits) {
        const rhi::CapabilityRow* row = rhi::FindRow(clean, LimitId(limit.name));
        runner->Check(row != nullptr &&
                          row->result == rhi::CapabilityResult::Pass,
                      std::string("T072 ") + limit.name +
                          " passes exactly at its threshold");
    }

    for (const ExpectedLimit& limit : kExpectedLimits) {
        {
            rhi::ProfileRequest request = baseline;
            MarkLimitUnreported(&request, limit.name);
            rhi::ProfileSelection selection;
            runner->Check(SelectOk(request, &selection),
                          std::string("T072 ") + limit.name +
                              " unreported case evaluates");
            const rhi::CapabilityRow* row =
                rhi::FindRow(selection, LimitId(limit.name));
            runner->Check(row != nullptr &&
                              row->result == rhi::CapabilityResult::Unsupported,
                          std::string("T072 ") + limit.name +
                              " unreported is UNSUPPORTED");
            runner->Check(selection.status == rhi::ProfileStatus::Unsupported &&
                              !selection.accepted,
                          std::string("T072 ") + limit.name +
                              " unreported refuses the profile");
            runner->Check(!selection.refusalReasons.empty(),
                          std::string("T072 ") + limit.name +
                              " unreported carries a reason");
        }
        {
            rhi::ProfileRequest request = baseline;
            SetLimitValue(&request, limit.name,
                          limit.atLeast ? limit.requirement - 1
                                        : limit.requirement + 1);
            rhi::ProfileSelection selection;
            runner->Check(SelectOk(request, &selection),
                          std::string("T072 ") + limit.name +
                              " outside case evaluates");
            const rhi::CapabilityRow* row =
                rhi::FindRow(selection, LimitId(limit.name));
            runner->Check(row != nullptr &&
                              row->result == rhi::CapabilityResult::Unsupported,
                          std::string("T072 ") + limit.name +
                              " one step outside is UNSUPPORTED");
            runner->Check(!selection.accepted,
                          std::string("T072 ") + limit.name +
                              " outside the requirement refuses the profile");
            runner->Check(row != nullptr && !row->reason.empty(),
                          std::string("T072 ") + limit.name +
                              " refusal carries a reason");
        }
    }

    // A completely absent entry is the same refusal as an unreported one.
    {
        rhi::ProfileRequest request = baseline;
        std::vector<rhi::AdvertisedLimit>& limits = request.advertised.limits;
        for (std::vector<rhi::AdvertisedLimit>::iterator it = limits.begin();
             it != limits.end(); ++it) {
            if (it->name == "maxBufferSize") {
                limits.erase(it);
                break;
            }
        }
        rhi::ProfileSelection selection;
        runner->Check(SelectOk(request, &selection),
                      "T072 absent limit entry evaluates");
        const rhi::CapabilityRow* row =
            rhi::FindRow(selection, "limit:maxBufferSize");
        runner->Check(row != nullptr &&
                          row->result == rhi::CapabilityResult::Unsupported,
                      "T072 an absent limit entry is UNSUPPORTED");
    }
}

// ---------------------------------------------------------------------------
// optional_fallbacks: each absent optional feature selects its named fallback
// and only a quality fallback degrades the profile.
// ---------------------------------------------------------------------------
void RunOptionalFallbacks(Runner* runner) {
    const rhi::ProfileRequest baseline = ConformingRequest();

    rhi::ProfileSelection clean;
    runner->Check(SelectOk(baseline, &clean),
                  "T072 available optional features evaluate");
    runner->Check(clean.selectedFallbacks.empty(),
                  "T072 available optional features select no fallback");
    for (const ExpectedFeature& feature : kExpectedOptional) {
        const rhi::CapabilityRow* row =
            rhi::FindRow(clean, FeatureId(feature.name));
        runner->Check(row != nullptr &&
                          row->result == rhi::CapabilityResult::Pass,
                      std::string("T072 ") + feature.name + " is PASS");
        runner->Check(row != nullptr && row->fallbackId.empty(),
                      std::string("T072 ") + feature.name +
                          " selects no fallback while available");
    }

    for (const ExpectedFeature& feature : kExpectedOptional) {
        rhi::ProfileRequest request = baseline;
        DropFeature(&request, feature.name);
        rhi::ProfileSelection selection;
        runner->Check(SelectOk(request, &selection),
                      std::string("T072 ") + feature.name +
                          " absent case evaluates");
        const rhi::CapabilityRow* row =
            rhi::FindRow(selection, FeatureId(feature.name));
        runner->Check(row != nullptr &&
                          row->result == rhi::CapabilityResult::Fallback,
                      std::string("T072 ") + feature.name +
                          " absent is FALLBACK");
        runner->Check(row != nullptr &&
                          row->fallbackId == feature.fallbackId,
                      std::string("T072 ") + feature.name +
                          " selects fallback " + feature.fallbackId);
        runner->Check(row != nullptr && row->impact == feature.impact,
                      std::string("T072 ") + feature.name +
                          " reports impact " + rhi::ToString(feature.impact));
        runner->Check(Contains(selection.selectedFallbacks, feature.fallbackId),
                      std::string("T072 ") + feature.name +
                          " records the selected fallback");
        const rhi::ProfileStatus expectedStatus =
            feature.impact == rhi::FallbackImpact::Quality
                ? rhi::ProfileStatus::Degraded
                : rhi::ProfileStatus::Ready;
        runner->Check(selection.status == expectedStatus,
                      std::string("T072 ") + feature.name + " yields " +
                          rhi::ToString(expectedStatus),
                      rhi::ToString(selection.status));
        runner->Check(selection.accepted,
                      std::string("T072 ") + feature.name +
                          " fallback keeps the profile accepted");
        runner->Check(selection.refusalReasons.empty(),
                      std::string("T072 ") + feature.name +
                          " fallback is not a refusal");
    }
}

// ---------------------------------------------------------------------------
// excluded_classes: every class the profile refuses is refused for its own
// reason, even when the backend advertises it.
// ---------------------------------------------------------------------------
void RunExcludedClasses(Runner* runner) {
    const rhi::ProfileRequest baseline = ConformingRequest();

    rhi::ProfileSelection clean;
    runner->Check(SelectOk(baseline, &clean),
                  "T072 unrequested excluded classes evaluate");
    for (const ExpectedExcluded& excluded : kExpectedExcluded) {
        runner->Check(rhi::FindRow(clean, ClassId(excluded.name)) == nullptr,
                      std::string("T072 ") + excluded.name +
                          " adds no row until it is requested");
    }

    for (const ExpectedExcluded& excluded : kExpectedExcluded) {
        rhi::ProfileRequest request = baseline;
        request.requestedClasses.push_back(excluded.name);
        // Advertise the class as available. Availability must not grant it.
        request.advertised.features.push_back(excluded.name);
        rhi::ProfileSelection selection;
        runner->Check(SelectOk(request, &selection),
                      std::string("T072 ") + excluded.name +
                          " request evaluates");
        const rhi::CapabilityRow* row =
            rhi::FindRow(selection, ClassId(excluded.name));
        runner->Check(row != nullptr &&
                          row->result == rhi::CapabilityResult::Unsupported,
                      std::string("T072 ") + excluded.name +
                          " is UNSUPPORTED when requested",
                      row != nullptr ? rhi::ToString(row->result)
                                     : std::string("row missing"));
        runner->Check(row != nullptr && row->fallbackId.empty(),
                      std::string("T072 ") + excluded.name +
                          " selects no automatic fallback");
        runner->Check(selection.status == rhi::ProfileStatus::Unsupported &&
                          !selection.accepted && !selection.supportClaimAllowed,
                      std::string("T072 ") + excluded.name +
                          " refuses the profile");
        runner->Check(!selection.refusalReasons.empty(),
                      std::string("T072 ") + excluded.name +
                          " carries a refusal reason");
    }

    {
        rhi::ProfileRequest request = baseline;
        request.requestedClasses.push_back("teleportation");
        rhi::ProfileSelection selection;
        runner->Check(SelectOk(request, &selection),
                      "T072 unknown class request evaluates");
        const rhi::CapabilityRow* row =
            rhi::FindRow(selection, ClassId("teleportation"));
        runner->Check(row != nullptr &&
                          row->result == rhi::CapabilityResult::Unsupported,
                      "T072 an unknown class is refused rather than granted");
        runner->Check(!selection.accepted,
                      "T072 an unknown class refuses the profile");
    }

    {
        rhi::ProfileRequest request = baseline;
        request.requestedClasses.push_back("ray-tracing");
        request.requestedClasses.push_back("ray-tracing");
        rhi::ProfileSelection selection;
        std::string error;
        runner->Check(rhi::SelectCapabilityProfile(request, &selection, &error) ==
                          rhi::Status::InvalidArgument &&
                          !error.empty(),
                      "T072 a duplicated requested class is InvalidArgument");
    }
}

// ---------------------------------------------------------------------------
// identity_and_vocabulary: target identity, evidence, executed checks and the
// result vocabulary itself.
// ---------------------------------------------------------------------------
void RunIdentityAndVocabulary(Runner* runner) {
    const rhi::ProfileRequest baseline = ConformingRequest();

    {
        rhi::ProfileRequest request = baseline;
        request.declared.targetId = "another-target";
        rhi::ProfileSelection selection;
        runner->Check(SelectOk(request, &selection),
                      "T072 a different declared target evaluates");
        const rhi::CapabilityRow* row =
            rhi::FindRow(selection, "identity:target");
        runner->Check(row != nullptr &&
                          row->result == rhi::CapabilityResult::Unsupported,
                      "T072 a different declared target is UNSUPPORTED");
        runner->Check(selection.status == rhi::ProfileStatus::Unsupported,
                      "T072 a different target refuses the profile");
    }
    {
        rhi::ProfileRequest request = baseline;
        request.declared.targetId.clear();
        rhi::ProfileSelection selection;
        runner->Check(SelectOk(request, &selection),
                      "T072 a missing declared target evaluates");
        const rhi::CapabilityRow* row =
            rhi::FindRow(selection, "identity:target");
        runner->Check(row != nullptr &&
                          row->result == rhi::CapabilityResult::Unsupported,
                      "T072 a missing declared target is UNSUPPORTED");
    }
    {
        rhi::ProfileRequest request = baseline;
        request.declared.backend.clear();
        rhi::ProfileSelection selection;
        runner->Check(SelectOk(request, &selection),
                      "T072 a missing backend evaluates");
        const rhi::CapabilityRow* row =
            rhi::FindRow(selection, "identity:backend");
        runner->Check(row != nullptr &&
                          row->result == rhi::CapabilityResult::Unsupported,
                      "T072 a missing backend is UNSUPPORTED");
    }
    for (uint32_t version : {0u, 99u}) {
        rhi::ProfileRequest request = baseline;
        request.declared.contractVersion = version;
        rhi::ProfileSelection selection;
        runner->Check(SelectOk(request, &selection),
                      "T072 contract version mismatch evaluates");
        const rhi::CapabilityRow* row =
            rhi::FindRow(selection, "identity:contractVersion");
        runner->Check(row != nullptr &&
                          row->result == rhi::CapabilityResult::Unsupported,
                      "T072 contract version " + std::to_string(version) +
                          " is UNSUPPORTED");
    }
    {
        rhi::ProfileRequest request = baseline;
        request.declared.evidence.clear();
        rhi::ProfileSelection selection;
        runner->Check(SelectOk(request, &selection),
                      "T072 a profile without evidence evaluates");
        const rhi::CapabilityRow* row = rhi::FindRow(selection, "evidence");
        runner->Check(row != nullptr &&
                          row->result == rhi::CapabilityResult::NotRun,
                      "T072 missing evidence is NOT RUN");
        runner->Check(selection.status == rhi::ProfileStatus::Ready &&
                          selection.accepted,
                      "T072 missing evidence does not change the capability verdict");
        runner->Check(!selection.supportClaimAllowed,
                      "T072 missing evidence blocks a support claim");
    }
    {
        rhi::ProfileRequest request = baseline;
        request.checks.push_back(rhi::ExecutedCheck{"render-readback", true});
        rhi::ProfileSelection selection;
        runner->Check(SelectOk(request, &selection),
                      "T072 a met executed check evaluates");
        const rhi::CapabilityRow* row =
            rhi::FindRow(selection, "check:render-readback");
        runner->Check(row != nullptr &&
                          row->result == rhi::CapabilityResult::Pass,
                      "T072 a met executed check is PASS");
    }
    {
        rhi::ProfileRequest request = baseline;
        request.checks.push_back(rhi::ExecutedCheck{"render-readback", false});
        rhi::ProfileSelection selection;
        runner->Check(SelectOk(request, &selection),
                      "T072 a failed executed check evaluates");
        const rhi::CapabilityRow* row =
            rhi::FindRow(selection, "check:render-readback");
        runner->Check(row != nullptr &&
                          row->result == rhi::CapabilityResult::Fail,
                      "T072 an unmet executed check is FAIL");
        runner->Check(selection.status == rhi::ProfileStatus::Unsupported &&
                          !selection.accepted && !selection.supportClaimAllowed,
                      "T072 a failed executed check refuses the profile");
    }

    runner->Check(std::string(rhi::ToString(rhi::CapabilityResult::Pass)) == "PASS",
                  "T072 result vocabulary spells PASS");
    runner->Check(std::string(rhi::ToString(rhi::CapabilityResult::Fallback)) ==
                      "FALLBACK",
                  "T072 result vocabulary spells FALLBACK");
    runner->Check(
        std::string(rhi::ToString(rhi::CapabilityResult::Unsupported)) ==
            "UNSUPPORTED",
        "T072 result vocabulary spells UNSUPPORTED");
    runner->Check(std::string(rhi::ToString(rhi::CapabilityResult::NotRun)) ==
                      "NOT RUN",
                  "T072 result vocabulary spells NOT RUN");
    runner->Check(std::string(rhi::ToString(rhi::CapabilityResult::Fail)) == "FAIL",
                  "T072 result vocabulary spells FAIL");
    runner->Check(std::string(rhi::ToString(rhi::ProfileStatus::Ready)) == "READY",
                  "T072 status vocabulary spells READY");
    runner->Check(std::string(rhi::ToString(rhi::ProfileStatus::Degraded)) ==
                      "DEGRADED",
                  "T072 status vocabulary spells DEGRADED");
    runner->Check(std::string(rhi::ToString(rhi::ProfileStatus::Unsupported)) ==
                      "UNSUPPORTED",
                  "T072 status vocabulary spells UNSUPPORTED");
    runner->Check(std::string(rhi::ToString(rhi::FallbackImpact::Info)) == "info",
                  "T072 impact vocabulary spells info");
    runner->Check(std::string(rhi::ToString(rhi::FallbackImpact::Quality)) ==
                      "quality",
                  "T072 impact vocabulary spells quality");
    runner->Check(std::string(rhi::ToString(rhi::CapabilityClass::ExcludedClass)) ==
                      "excluded-class",
                  "T072 class vocabulary names an excluded class");

    rhi::ProfileSelection clean;
    runner->Check(SelectOk(baseline, &clean), "T072 lookup fixture evaluates");
    runner->Check(rhi::FindRow(clean, "limit:not-a-limit") == nullptr,
                  "T072 FindRow reports an absent id");

    {
        rhi::ProfileSelection selection;
        std::string error;
        runner->Check(rhi::SelectCapabilityProfile(baseline, nullptr, &error) ==
                              rhi::Status::InvalidArgument &&
                          !error.empty(),
                      "T072 a null output is InvalidArgument");
    }
    {
        rhi::ProfileRequest request = baseline;
        request.expectedTargetId.clear();
        rhi::ProfileSelection selection;
        std::string error;
        runner->Check(rhi::SelectCapabilityProfile(request, &selection, &error) ==
                              rhi::Status::InvalidArgument &&
                          !error.empty(),
                      "T072 a missing expected target id is InvalidArgument");
    }
    {
        rhi::ProfileRequest request = baseline;
        request.advertised.limits.push_back(request.advertised.limits.front());
        rhi::ProfileSelection selection;
        std::string error;
        runner->Check(rhi::SelectCapabilityProfile(request, &selection, &error) ==
                              rhi::Status::InvalidArgument &&
                          !error.empty(),
                      "T072 a duplicated advertised limit is InvalidArgument");
    }
    {
        rhi::ProfileRequest request = baseline;
        request.checks.push_back(rhi::ExecutedCheck{"same", true});
        request.checks.push_back(rhi::ExecutedCheck{"same", false});
        rhi::ProfileSelection selection;
        std::string error;
        runner->Check(rhi::SelectCapabilityProfile(request, &selection, &error) ==
                              rhi::Status::InvalidArgument &&
                          !error.empty(),
                      "T072 a duplicated check id is InvalidArgument");
    }
}

// ---------------------------------------------------------------------------
// negative_controls: a silently upgraded class must fail this test.
// ---------------------------------------------------------------------------
void RunNegativeControls(Runner* runner) {
    const rhi::ProfileRequest baseline = ConformingRequest();

    // A short required limit is refused even when every optional feature is
    // available and evidence is declared. Feature availability must not mask a
    // required shortfall.
    {
        rhi::ProfileRequest request = baseline;
        SetLimitValue(&request, "maxBindGroups", 3u);
        rhi::ProfileSelection selection;
        runner->Check(SelectOk(request, &selection),
                      "T072 shortfall with every feature available evaluates");
        runner->Check(selection.status == rhi::ProfileStatus::Unsupported &&
                          !selection.accepted && !selection.supportClaimAllowed,
                      "T072 feature availability does not mask a required shortfall");
        const rhi::CapabilityRow* row =
            rhi::FindRow(selection, "limit:maxBindGroups");
        runner->Check(row != nullptr &&
                          row->result == rhi::CapabilityResult::Unsupported,
                      "T072 the short limit is not upgraded to PASS");
        runner->Check(selection.selectedFallbacks.empty(),
                      "T072 no fallback is invented for a required limit");
    }

    // Every excluded class is refused even when advertised and requested. This
    // is the silent-upgrade guard.
    for (const ExpectedExcluded& excluded : kExpectedExcluded) {
        rhi::ProfileRequest request = baseline;
        request.advertised.features.push_back(excluded.name);
        request.requestedClasses.push_back(excluded.name);
        rhi::ProfileSelection selection;
        runner->Check(SelectOk(request, &selection),
                      std::string("T072 ") + excluded.name +
                          " advertised-and-requested case evaluates");
        const rhi::CapabilityRow* row =
            rhi::FindRow(selection, ClassId(excluded.name));
        const bool upgraded =
            row == nullptr ||
            row->result == rhi::CapabilityResult::Pass ||
            row->result == rhi::CapabilityResult::Fallback;
        runner->Check(!upgraded,
                      std::string("T072 ") + excluded.name +
                          " is never upgraded to PASS or FALLBACK",
                      row != nullptr ? rhi::ToString(row->result)
                                     : std::string("row missing"));
        runner->Check(selection.status == rhi::ProfileStatus::Unsupported &&
                          !selection.accepted,
                      std::string("T072 ") + excluded.name +
                          " keeps the profile refused");
        runner->Check(!Contains(selection.selectedFallbacks, excluded.name),
                      std::string("T072 ") + excluded.name +
                          " is not recorded as a selected fallback");
    }

    // Threshold boundaries in both directions.
    for (const ExpectedLimit& limit : kExpectedLimits) {
        rhi::ProfileRequest inside = baseline;
        SetLimitValue(&inside, limit.name,
                      limit.atLeast ? limit.requirement + 1
                                    : limit.requirement - 1);
        rhi::ProfileSelection insideSelection;
        runner->Check(SelectOk(inside, &insideSelection),
                      std::string("T072 ") + limit.name + " inside evaluates");
        const rhi::CapabilityRow* insideRow =
            rhi::FindRow(insideSelection, LimitId(limit.name));
        runner->Check(insideRow != nullptr &&
                          insideRow->result == rhi::CapabilityResult::Pass &&
                          insideSelection.accepted,
                      std::string("T072 ") + limit.name +
                          " one step inside still passes");

        rhi::ProfileRequest outside = baseline;
        SetLimitValue(&outside, limit.name,
                      limit.atLeast ? limit.requirement - 1
                                    : limit.requirement + 1);
        rhi::ProfileSelection outsideSelection;
        runner->Check(SelectOk(outside, &outsideSelection),
                      std::string("T072 ") + limit.name + " outside evaluates");
        const rhi::CapabilityRow* outsideRow =
            rhi::FindRow(outsideSelection, LimitId(limit.name));
        runner->Check(outsideRow != nullptr &&
                          outsideRow->result == rhi::CapabilityResult::Unsupported &&
                          !outsideSelection.accepted,
                      std::string("T072 ") + limit.name +
                          " one step outside never passes");
    }

    // A quality fallback degrades; an info fallback does not.
    {
        rhi::ProfileRequest request = baseline;
        DropFeature(&request, "texture-compression-bc");
        rhi::ProfileSelection selection;
        runner->Check(SelectOk(request, &selection),
                      "T072 quality fallback evaluates");
        runner->Check(selection.status == rhi::ProfileStatus::Degraded &&
                          selection.accepted,
                      "T072 a quality fallback degrades rather than passes silently");
    }
    {
        rhi::ProfileRequest request = baseline;
        DropFeature(&request, "timestamp-query");
        rhi::ProfileSelection selection;
        runner->Check(SelectOk(request, &selection),
                      "T072 info fallback evaluates");
        runner->Check(selection.status == rhi::ProfileStatus::Ready &&
                          selection.accepted,
                      "T072 an info fallback does not degrade the profile");
    }

    // Invariants that hold for every refused profile.
    {
        rhi::ProfileRequest request = baseline;
        request.requestedClasses.push_back("ray-tracing");
        rhi::ProfileSelection selection;
        runner->Check(SelectOk(request, &selection),
                      "T072 refusal invariant fixture evaluates");
        bool anyRefusingRow = false;
        for (const rhi::CapabilityRow& row : selection.rows) {
            if (row.result == rhi::CapabilityResult::Unsupported ||
                row.result == rhi::CapabilityResult::Fail) {
                anyRefusingRow = true;
            }
        }
        runner->Check(anyRefusingRow,
                      "T072 a refused profile records at least one refusing row");
        runner->Check(anyRefusingRow == !selection.accepted,
                      "T072 acceptance follows the refusing rows");
        runner->Check((selection.status == rhi::ProfileStatus::Unsupported) ==
                          anyRefusingRow,
                      "T072 the overall status follows the refusing rows");
    }

    // NOT RUN never authorizes a claim, and never counts as a pass.
    {
        rhi::ProfileRequest request = baseline;
        request.declared.evidence.clear();
        rhi::ProfileSelection selection;
        runner->Check(SelectOk(request, &selection),
                      "T072 NOT RUN invariant fixture evaluates");
        bool anyNotRun = false;
        for (const rhi::CapabilityRow& row : selection.rows) {
            if (row.result == rhi::CapabilityResult::NotRun) {
                anyNotRun = true;
            }
        }
        runner->Check(anyNotRun,
                      "T072 a missing evidence row is recorded as NOT RUN");
        runner->Check(selection.accepted,
                      "T072 NOT RUN does not change the capability verdict");
        runner->Check(!selection.supportClaimAllowed,
                      "T072 NOT RUN never authorizes a support claim");
    }
}

}  // namespace

int main(int argc, char** argv) {
    const std::string group = argc > 1 ? argv[1] : "all";
    const bool all = group == "all";
    const bool known = all || group == "contract_tables" ||
                       group == "required_limits" ||
                       group == "optional_fallbacks" ||
                       group == "excluded_classes" ||
                       group == "identity_and_vocabulary" ||
                       group == "negative_controls";
    if (!known) {
        std::printf("FAIL unknown group %s\n", group.c_str());
        return 2;
    }

    Runner runner;
    if (all || group == "contract_tables") {
        RunContractTables(&runner);
    }
    if (all || group == "required_limits") {
        RunRequiredLimits(&runner);
    }
    if (all || group == "optional_fallbacks") {
        RunOptionalFallbacks(&runner);
    }
    if (all || group == "excluded_classes") {
        RunExcludedClasses(&runner);
    }
    if (all || group == "identity_and_vocabulary") {
        RunIdentityAndVocabulary(&runner);
    }
    if (all || group == "negative_controls") {
        RunNegativeControls(&runner);
    }
    return runner.Finish("portable_profile");
}
