#include "rhi/capability_profile.h"

#include <utility>

namespace doom3 {
namespace rhi {
namespace {

const std::vector<LimitRule>& RequiredLimitTable() {
    // Section 3 of specs/004-quality-renderer/portable-profile-contract.md.
    // Order is part of the report shape; each value is an independent literal
    // and is compared against the published table by the T072 fixture.
    static const std::vector<LimitRule> rules = {
        {"maxBufferSize", 268435456u, true,
         "chunked staging and uploads need 256 MiB"},
        {"maxStorageBufferBindingSize", 134217728u, true,
         "GI and reflection buffers must fit one binding or be split"},
        {"maxTextureDimension2D", 8192u, true,
         "display and offscreen targets up to 8192 px"},
        {"maxBindGroups", 4u, true,
         "frame, view-light, material and per-draw groups"},
        {"maxSampledTexturesPerShaderStage", 16u, true,
         "seven interaction textures plus headroom"},
        {"maxStorageBuffersPerShaderStage", 8u, true,
         "budgeted compute jobs"},
        {"maxUniformBuffersPerShaderStage", 12u, true,
         "classic path uniform blocks"},
        {"maxUniformBufferBindingSize", 65536u, true,
         "per-light and per-draw data packing"},
        {"minUniformBufferOffsetAlignment", 256u, false,
         "uniform ring slots are 256-byte aligned"},
        {"maxVertexBuffers", 8u, true,
         "one interleaved DOOM 3 vertex stream"},
        {"maxVertexAttributes", 16u, true,
         "position, ST, normal, tangents and colour"},
        {"maxColorAttachments", 8u, true,
         "G-buffer plus auxiliary targets"},
        {"maxColorAttachmentBytesPerSample", 32u, true,
         "28-byte G-buffer plus headroom"},
        {"maxComputeInvocationsPerWorkgroup", 256u, true,
         "compute tile sizes"},
    };
    return rules;
}

const std::vector<FeatureRule>& OptionalFeatureTable() {
    // Section 4 of specs/004-quality-renderer/portable-profile-contract.md.
    static const std::vector<FeatureRule> rules = {
        {"timestamp-query", "cpu-frame-timing", FallbackImpact::Info,
         "use CPU frame timing and make no GPU performance claim"},
        {"texture-compression-bc", "bc-transcode-required", FallbackImpact::Quality,
         "transcode block-compressed textures before upload; no such asset is exercised today"},
        {"depth-clip-control", "no-clip-plane", FallbackImpact::Info,
         "DOOM 3 does not require clip planes; alphaNotch is shader-side"},
        {"float32-filterable", "nearest-or-half-float", FallbackImpact::Info,
         "use nearest filtering or a half-float format for float textures"},
    };
    return rules;
}

const std::vector<ExcludedClass>& ExcludedClassTable() {
    // Section 6 of specs/004-quality-renderer/portable-profile-contract.md plus
    // the native-interop row of contracts/rhi-foundation.md. Naming a class
    // here is a refusal, not an implementation, and no alternative is selected
    // automatically.
    static const std::vector<ExcludedClass> classes = {
        {"ray-tracing",
         "ray tracing and acceleration structures are outside the portable profile"},
        {"neural-upscaler",
         "the native neural upscaler is outside the portable profile"},
        {"bindless-bindings",
         "bindless binding arrays are outside the portable profile"},
        {"multiple-queues",
         "multiple queues are outside the portable profile"},
        {"sparse-resources",
         "sparse resources are outside the portable profile"},
        {"resource-aliasing",
         "resource aliasing is outside the portable profile"},
        {"depth-bounds",
         "depth bounds are outside the portable profile"},
        {"polygon-mode",
         "polygon mode is outside the portable profile"},
        {"fixed-function-alpha-test",
         "fixed-function alpha test is outside the portable profile"},
        {"texgen",
         "fixed-function texgen is outside the portable profile"},
        {"legacy-l8-la8-a8",
         "L8, LA8 and A8 formats are outside the portable profile"},
        {"guaranteed-bc",
         "guaranteed block compression is outside the portable profile"},
        {"native-provider-interop",
         "native provider interop is outside the portable profile"},
        {"native-interop",
         "backend-specific native interop is outside the portable profile"},
    };
    return classes;
}

const ExcludedClass* FindExcludedClass(const std::string& name) {
    for (const ExcludedClass& excluded : ExcludedClassTable()) {
        if (name == excluded.name) {
            return &excluded;
        }
    }
    return nullptr;
}

const AdvertisedLimit* FindAdvertisedLimit(const AdvertisedCapabilities& advertised,
                                          const std::string& name) {
    for (const AdvertisedLimit& limit : advertised.limits) {
        if (limit.name == name) {
            return &limit;
        }
    }
    return nullptr;
}

bool AdvertisesFeature(const AdvertisedCapabilities& advertised,
                       const std::string& name) {
    for (const std::string& feature : advertised.features) {
        if (feature == name) {
            return true;
        }
    }
    return false;
}

}  // namespace

const char* ToString(CapabilityResult value) {
    switch (value) {
        case CapabilityResult::Pass:
            return "PASS";
        case CapabilityResult::Fallback:
            return "FALLBACK";
        case CapabilityResult::Unsupported:
            return "UNSUPPORTED";
        case CapabilityResult::NotRun:
            return "NOT RUN";
        case CapabilityResult::Fail:
            return "FAIL";
    }
    return "UNSUPPORTED";
}

const char* ToString(ProfileStatus value) {
    switch (value) {
        case ProfileStatus::Ready:
            return "READY";
        case ProfileStatus::Degraded:
            return "DEGRADED";
        case ProfileStatus::Unsupported:
            return "UNSUPPORTED";
    }
    return "UNSUPPORTED";
}

const char* ToString(FallbackImpact value) {
    switch (value) {
        case FallbackImpact::None:
            return "none";
        case FallbackImpact::Info:
            return "info";
        case FallbackImpact::Quality:
            return "quality";
    }
    return "none";
}

const char* ToString(CapabilityClass value) {
    switch (value) {
        case CapabilityClass::Identity:
            return "identity";
        case CapabilityClass::RequiredLimit:
            return "required-limit";
        case CapabilityClass::OptionalFeature:
            return "optional-feature";
        case CapabilityClass::ExcludedClass:
            return "excluded-class";
        case CapabilityClass::Evidence:
            return "evidence";
        case CapabilityClass::ExecutedCheck:
            return "executed-check";
    }
    return "required-limit";
}

const std::vector<LimitRule>& RequiredLimitRules() {
    return RequiredLimitTable();
}

const std::vector<FeatureRule>& OptionalFeatureRules() {
    return OptionalFeatureTable();
}

const std::vector<ExcludedClass>& ExcludedClasses() {
    return ExcludedClassTable();
}

Status SelectCapabilityProfile(const ProfileRequest& request,
                               ProfileSelection* selection,
                               std::string* error) {
    if (selection == nullptr) {
        if (error != nullptr) {
            *error = "selection output is required";
        }
        return Status::InvalidArgument;
    }
    *selection = ProfileSelection{};

    auto reject = [&](const std::string& message) {
        selection->error = message;
        if (error != nullptr) {
            *error = message;
        }
        return Status::InvalidArgument;
    };

    if (request.expectedTargetId.empty()) {
        return reject("an explicit expected target id is required");
    }

    // A duplicated fact is a malformed table, not a capability: two values for
    // one name would make the comparison order-dependent.
    for (size_t first = 0; first < request.advertised.limits.size(); ++first) {
        for (size_t second = first + 1; second < request.advertised.limits.size();
             ++second) {
            if (request.advertised.limits[first].name ==
                request.advertised.limits[second].name) {
                return reject("advertised limit \"" +
                              request.advertised.limits[second].name +
                              "\" is listed twice");
            }
        }
    }
    for (size_t first = 0; first < request.requestedClasses.size(); ++first) {
        for (size_t second = first + 1; second < request.requestedClasses.size();
             ++second) {
            if (request.requestedClasses[first] == request.requestedClasses[second]) {
                return reject("requested capability class \"" +
                              request.requestedClasses[second] +
                              "\" is listed twice");
            }
        }
    }
    for (size_t first = 0; first < request.checks.size(); ++first) {
        for (size_t second = first + 1; second < request.checks.size(); ++second) {
            if (request.checks[first].id == request.checks[second].id) {
                return reject("executed check \"" + request.checks[second].id +
                              "\" is listed twice");
            }
        }
    }

    auto add_row = [&](CapabilityRow row) {
        if (row.result == CapabilityResult::Unsupported ||
            row.result == CapabilityResult::Fail) {
            selection->refusalReasons.push_back(row.id + ": " + row.reason);
        }
        selection->rows.push_back(std::move(row));
    };

    // ---- Identity -----------------------------------------------------------
    {
        CapabilityRow row;
        row.id = "identity:target";
        row.name = "targetId";
        row.kind = CapabilityClass::Identity;
        if (request.declared.targetId.empty()) {
            row.result = CapabilityResult::Unsupported;
            row.reason = "declared target id is missing; a profile must name itself";
        } else if (request.declared.targetId != request.expectedTargetId) {
            row.result = CapabilityResult::Unsupported;
            row.reason = "declared target \"" + request.declared.targetId +
                         "\" is not the requested \"" + request.expectedTargetId +
                         "\"; a different target cannot stand in";
        } else {
            row.result = CapabilityResult::Pass;
            row.reason = "declared target matches the requested id";
        }
        add_row(std::move(row));
    }
    {
        CapabilityRow row;
        row.id = "identity:backend";
        row.name = "backend";
        row.kind = CapabilityClass::Identity;
        if (request.declared.backend.empty()) {
            row.result = CapabilityResult::Unsupported;
            row.reason =
                "backend implementation and version are required; the host OS alone is not a backend";
        } else {
            row.result = CapabilityResult::Pass;
            row.reason = "backend implementation and version are declared";
        }
        add_row(std::move(row));
    }
    {
        CapabilityRow row;
        row.id = "identity:contractVersion";
        row.name = "contractVersion";
        row.kind = CapabilityClass::Identity;
        if (request.declared.contractVersion != kPortableContractVersion) {
            row.result = CapabilityResult::Unsupported;
            row.reason = "declared contract version " +
                         std::to_string(request.declared.contractVersion) +
                         " does not match " +
                         std::to_string(kPortableContractVersion);
        } else {
            row.result = CapabilityResult::Pass;
            row.reason = "declared contract version matches";
        }
        add_row(std::move(row));
    }

    // ---- Required limits ----------------------------------------------------
    for (const LimitRule& rule : RequiredLimitRules()) {
        CapabilityRow row;
        row.id = std::string("limit:") + rule.name;
        row.name = rule.name;
        row.kind = CapabilityClass::RequiredLimit;

        const AdvertisedLimit* advertised =
            FindAdvertisedLimit(request.advertised, rule.name);
        if (advertised == nullptr || !advertised->reported) {
            row.result = CapabilityResult::Unsupported;
            row.reason =
                "advertised limit is missing; a required limit is never assumed";
        } else if (rule.atLeast && advertised->value < rule.requirement) {
            row.result = CapabilityResult::Unsupported;
            row.reason = "advertised " + std::to_string(advertised->value) +
                         " is below the required " +
                         std::to_string(rule.requirement);
        } else if (!rule.atLeast && advertised->value > rule.requirement) {
            row.result = CapabilityResult::Unsupported;
            row.reason = "advertised " + std::to_string(advertised->value) +
                         " is above the allowed " +
                         std::to_string(rule.requirement);
        } else {
            row.result = CapabilityResult::Pass;
            row.reason = rule.atLeast
                             ? "advertised value meets the at-least requirement"
                             : "advertised value meets the at-most requirement";
        }
        add_row(std::move(row));
    }

    // ---- Optional features and their declared fallbacks ---------------------
    for (const FeatureRule& rule : OptionalFeatureRules()) {
        CapabilityRow row;
        row.id = std::string("feature:") + rule.name;
        row.name = rule.name;
        row.kind = CapabilityClass::OptionalFeature;
        if (AdvertisesFeature(request.advertised, rule.name)) {
            row.result = CapabilityResult::Pass;
            row.reason = "advertised feature is available";
        } else {
            row.result = CapabilityResult::Fallback;
            row.fallbackId = rule.fallbackId;
            row.impact = rule.impact;
            row.reason = rule.reason;
            selection->selectedFallbacks.push_back(rule.fallbackId);
        }
        add_row(std::move(row));
    }

    // ---- Requested classes outside the portable profile ---------------------
    for (const std::string& requested : request.requestedClasses) {
        CapabilityRow row;
        row.id = std::string("class:") + requested;
        row.name = requested;
        row.kind = CapabilityClass::ExcludedClass;
        row.result = CapabilityResult::Unsupported;
        const ExcludedClass* excluded = FindExcludedClass(requested);
        if (excluded != nullptr) {
            // Advertising the class does not authorize it. The caller may
            // choose a different profile or stop; this unit does not switch.
            row.reason = excluded->reason;
        } else {
            row.reason =
                "capability class is not selectable in the portable profile; an unknown request is refused rather than granted";
        }
        add_row(std::move(row));
    }

    // ---- Executed checks ----------------------------------------------------
    for (const ExecutedCheck& check : request.checks) {
        CapabilityRow row;
        row.id = std::string("check:") + check.id;
        row.name = check.id;
        row.kind = CapabilityClass::ExecutedCheck;
        row.result = check.met ? CapabilityResult::Pass : CapabilityResult::Fail;
        row.reason = check.met ? "executed check met its requirement"
                               : "executed check did not meet its requirement";
        add_row(std::move(row));
    }

    // ---- Evidence -----------------------------------------------------------
    {
        CapabilityRow row;
        row.id = "evidence";
        row.name = "evidence";
        row.kind = CapabilityClass::Evidence;
        if (request.declared.evidence.empty()) {
            row.result = CapabilityResult::NotRun;
            row.reason =
                "no target evidence record; a capability table alone does not authorize a support claim";
        } else {
            row.result = CapabilityResult::Pass;
            row.reason =
                "the target declares an evidence record; this unit records the declaration and does not read it";
        }
        add_row(std::move(row));
    }

    // ---- Verdict ------------------------------------------------------------
    bool refused = false;
    bool qualityFallback = false;
    bool anyNotRun = false;
    for (const CapabilityRow& row : selection->rows) {
        if (row.result == CapabilityResult::Unsupported ||
            row.result == CapabilityResult::Fail) {
            refused = true;
        }
        if (row.result == CapabilityResult::NotRun) {
            anyNotRun = true;
        }
        if (row.result == CapabilityResult::Fallback &&
            row.impact == FallbackImpact::Quality) {
            qualityFallback = true;
        }
    }

    selection->status = refused
                            ? ProfileStatus::Unsupported
                            : (qualityFallback ? ProfileStatus::Degraded
                                               : ProfileStatus::Ready);
    selection->accepted = !refused;
    selection->supportClaimAllowed = selection->accepted && !anyNotRun &&
                                     !request.declared.evidence.empty();
    return Status::Ok;
}

const CapabilityRow* FindRow(const ProfileSelection& selection,
                             const std::string& id) {
    for (const CapabilityRow& row : selection.rows) {
        if (row.id == id) {
            return &row;
        }
    }
    return nullptr;
}

}  // namespace rhi
}  // namespace doom3
