#pragma once

// Portable capability/fallback contract (spec 004 US8 / T072).
//
// This unit evaluates declared capability facts for one explicitly named
// target. It is profile-neutral: it names no graphics API, device handle or
// backend-private state, and it does not query hardware. A backend supplies the
// advertised limits and features; the caller supplies the target id it expects.
//
// The rules are fixed by specs/004-quality-renderer/portable-profile-contract.md:
//
//   * a required limit that is missing or on the wrong side of its threshold is
//     UNSUPPORTED, never silently lowered;
//   * an absent optional feature produces a FALLBACK row with a named id; a
//     quality fallback degrades the profile, an info fallback does not;
//   * a class the portable profile excludes is refused even when the backend
//     advertises it, because availability is not permission; no alternative is
//     chosen automatically;
//   * a check that did not execute is NOT RUN, and a skipped check is not a
//     pass. A support claim also needs a declared evidence record.
//
// The evaluator never switches the target, the backend or an excluded class on
// its own. Selecting another target or an explicit alternative stays a caller
// decision.

#include "rhi/rhi.h"

#include <cstdint>
#include <string>
#include <vector>

namespace doom3 {
namespace rhi {

// Version of the portable profile contract this table was evaluated against.
inline constexpr uint32_t kPortableContractVersion = 1;

// Result vocabulary. Exactly one value is reported per row.
enum class CapabilityResult {
    // The advertised fact meets the declared requirement.
    Pass,
    // The optional capability is absent and the named fallback was selected.
    Fallback,
    // The target cannot provide a required capability, or the identity is wrong.
    Unsupported,
    // The check did not execute. A specific reason is always supplied.
    NotRun,
    // The check executed and did not meet the requirement.
    Fail,
};

// Overall status of the evaluated profile.
enum class ProfileStatus {
    Ready,
    Degraded,
    Unsupported,
};

// Effect of selecting a fallback instead of the optional capability.
enum class FallbackImpact {
    None,
    // Recorded, but does not by itself change the overall status.
    Info,
    // Degrades the profile; the fallback and its limits must be published.
    Quality,
};

// Which table a row came from. Kept explicit so a report can group rows without
// parsing their ids.
enum class CapabilityClass {
    Identity,
    RequiredLimit,
    OptionalFeature,
    ExcludedClass,
    Evidence,
    ExecutedCheck,
};

const char* ToString(CapabilityResult value);
const char* ToString(ProfileStatus value);
const char* ToString(FallbackImpact value);
const char* ToString(CapabilityClass value);

// One required limit. A backend that cannot report the fact leaves the
// advertised entry unreported; that is UNSUPPORTED, never a real zero.
struct LimitRule {
    const char* name;
    uint64_t requirement;
    // true  -> the advertised value must be greater than or equal to the
    //          requirement (at-least).
    // false -> the advertised value must be less than or equal to the
    //          requirement (at-most).
    bool atLeast;
    const char* reason;
};

// One optional capability and the fallback selected when it is absent.
struct FeatureRule {
    const char* name;
    // Fallback id recorded in the FALLBACK row and in the selected-fallback
    // list. It is declared here, not inferred at selection time.
    const char* fallbackId;
    FallbackImpact impact;
    const char* reason;
};

// One capability class the portable profile explicitly does not provide.
// Requesting it is UNSUPPORTED even when the backend advertises it.
struct ExcludedClass {
    const char* name;
    const char* reason;
};

// The executable tables. Section 3 and section 4 of the contract are the
// authority; a test fails when these tables drift from the published document.
const std::vector<LimitRule>& RequiredLimitRules();
const std::vector<FeatureRule>& OptionalFeatureRules();
const std::vector<ExcludedClass>& ExcludedClasses();

// One advertised limit. `reported == false` means the backend could not produce
// the fact; `value` is then unavailable and is never read as a real zero.
struct AdvertisedLimit {
    std::string name;
    bool reported = false;
    uint64_t value = 0;
};

struct AdvertisedCapabilities {
    std::vector<AdvertisedLimit> limits;
    // Feature ids the backend advertises. Absence selects the declared
    // fallback; it never means "try the capability anyway".
    std::vector<std::string> features;
};

// What the target declares about itself. Every field is required by section 1
// of the contract except `evidence`, whose absence blocks a support claim
// rather than refusing the target.
struct ProfileDeclaration {
    std::string targetId;
    // Backend implementation and version. The host OS alone is not a backend.
    std::string backend;
    uint32_t contractVersion = 0;
    // Path to the target's own executed evidence record. This unit records the
    // declaration and does not read the path; T074 supplies the record and T075
    // reviews it.
    std::string evidence;
};

// One executed check supplied by the caller. This unit never runs a check; it
// encodes the vocabulary so a target that executed a check can report PASS or
// FAIL through the same table.
struct ExecutedCheck {
    std::string id;
    bool met = false;
};

struct ProfileRequest {
    // The id this evaluation is for. An empty value is a malformed request; a
    // declared id that differs from it is UNSUPPORTED.
    std::string expectedTargetId;
    ProfileDeclaration declared;
    AdvertisedCapabilities advertised;
    // Capability classes the caller asks for that are outside the portable
    // profile. Any excluded class refuses the target; an unknown class is
    // refused rather than granted.
    std::vector<std::string> requestedClasses;
    std::vector<ExecutedCheck> checks;
};

struct CapabilityRow {
    // Stable id, for example "limit:maxBufferSize", "feature:timestamp-query",
    // "class:native-dxr", "identity:target", "evidence" or "check:<id>".
    std::string id;
    std::string name;
    CapabilityClass kind = CapabilityClass::RequiredLimit;
    CapabilityResult result = CapabilityResult::Unsupported;
    // Non-empty only for a FALLBACK row: the declared fallback id that was
    // selected. An excluded class never gets an automatic fallback.
    std::string fallbackId;
    FallbackImpact impact = FallbackImpact::None;
    std::string reason;
};

struct ProfileSelection {
    ProfileStatus status = ProfileStatus::Unsupported;
    // True only when no required row failed or is missing and no excluded class
    // was requested. A DEGRADED profile is accepted with published limits.
    bool accepted = false;
    // True only when the profile is accepted, every row executed, and the
    // target declared its own evidence record. This is section 2's "may support
    // a claim" flag and is stricter than `accepted`.
    bool supportClaimAllowed = false;
    std::vector<CapabilityRow> rows;
    std::vector<std::string> selectedFallbacks;
    // One entry per UNSUPPORTED or FAIL row, in row order.
    std::vector<std::string> refusalReasons;
    // Set only for a malformed request. A refused profile is a completed
    // evaluation, not an error.
    std::string error;
};

// Evaluates one profile request. Returns Ok when the evaluation completed,
// whatever the verdict; the verdict is `selection->status`, `accepted` and
// `supportClaimAllowed`. InvalidArgument is reserved for a malformed request:
// a null output, an empty expected target id, or a duplicated advertised limit,
// requested class or check id.
Status SelectCapabilityProfile(const ProfileRequest& request,
                               ProfileSelection* selection,
                               std::string* error);

// Returns the first row with `id`, or nullptr. Stable linear lookup; the table
// is small and ordered.
const CapabilityRow* FindRow(const ProfileSelection& selection,
                             const std::string& id);

}  // namespace rhi
}  // namespace doom3
