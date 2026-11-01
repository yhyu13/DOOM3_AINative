// Implementation of the engine-facing resolver facade (spec 008).
// Port-owned C++17; compiled into the engine target at /std:c++17. The engine call sites stay at the
// engine's default language level and see only the C-string API in shader_engine.h.
#include "shader_engine.h"

#include "../shader_resolver.h"
#include "shader_manifest_data.h"

#include <cstddef>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace d3 {
namespace shaderport {

namespace {

using d3::shader::Decision;
using d3::shader::Manifest;
using d3::shader::Mode;
using d3::shader::Record;
using d3::shader::Request;
using d3::shader::Resolver;
using d3::shader::Status;
using d3::shader::Target;

struct Pending {
    std::string program;
    std::string key;
    int target = TARGET_VERTEX;
};

std::unique_ptr<Manifest> gManifest;
std::unique_ptr<Resolver> gResolver;
bool gActive = false;
std::string gStatusLine;
std::string gReloadReport;

std::vector<Pending> gPending;
std::map<std::pair<std::string, int>, int> gProgramIds;
std::size_t gDiagnosticSeen = 0;
int gStageIndex = -1;
std::string gDiagnostic;

void CaptureDiagnostic() {
    gDiagnostic.clear();
    if (!gResolver) {
        return;
    }
    const std::vector<Record>& records = gResolver->Records();
    if (records.size() > gDiagnosticSeen) {
        gDiagnostic = gResolver->Describe(records.back());
    }
    gDiagnosticSeen = records.size();
}

void CopyOut(char* dst, int size, const std::string& value) {
    if (dst == nullptr || size <= 0) {
        return;
    }
    const std::size_t room = static_cast<std::size_t>(size - 1);
    const std::size_t count = value.size() < room ? value.size() : room;
    if (count > 0) {
        std::memcpy(dst, value.data(), count);
    }
    dst[count] = 0;
}

Status ToStatus(int code) {
    switch (code) {
    case CODE_OK: return Status::OK;
    case CODE_UNRESOLVED: return Status::Unresolved;
    case CODE_STAGE_MISSING: return Status::StageMissing;
    case CODE_STUB: return Status::Stub;
    case CODE_UNSUPPORTED: return Status::Unsupported;
    case CODE_ABSENT: return Status::Absent;
    case CODE_AMBIGUOUS: return Status::Ambiguous;
    case CODE_NAME_TOO_LONG: return Status::NameTooLong;
    case CODE_MANIFEST_INVALID: return Status::ManifestInvalid;
    case CODE_COMPILE_FAILED: return Status::CompileFailed;
    case CODE_NOT_COMPILED: return Status::NotCompiled;
    default: return Status::Unresolved;
    }
}

int ToCode(Status status) {
    switch (status) {
    case Status::OK: return CODE_OK;
    case Status::Unresolved: return CODE_UNRESOLVED;
    case Status::StageMissing: return CODE_STAGE_MISSING;
    case Status::Stub: return CODE_STUB;
    case Status::Unsupported: return CODE_UNSUPPORTED;
    case Status::Absent: return CODE_ABSENT;
    case Status::Ambiguous: return CODE_AMBIGUOUS;
    case Status::NameTooLong: return CODE_NAME_TOO_LONG;
    case Status::ManifestInvalid: return CODE_MANIFEST_INVALID;
    case Status::CompileFailed: return CODE_COMPILE_FAILED;
    case Status::NotCompiled: return CODE_NOT_COMPILED;
    }
    return CODE_UNRESOLVED;
}

Target ToTarget(int target) {
    return target == TARGET_FRAGMENT ? Target::Fragment : Target::Vertex;
}

Mode ParseMode(const char* mode) {
    const std::string value = mode ? mode : "";
    if (value == "off") return Mode::Off;
    if (value == "tolerant") return Mode::Tolerant;
    if (value == "fatal") return Mode::Fatal;
    return Mode::Strict;
}

void AddPending(const Record& record) {
    if (record.key.empty()) {
        return;
    }
    // In strict/tolerant only an OK program can ever be drawn, so only those need a blob. `off`
    // reproduces the legacy draw, which binds data-provided ARB text even when the manifest says
    // stub/absent; queueing them keeps `off` behaviourally identical to the pre-resolver engine.
    const bool want = record.code == Status::OK ||
                      (gResolver != nullptr && gResolver->GetMode() == Mode::Off);
    if (!want) {
        return;
    }
    const int target = record.target == Target::Fragment ? TARGET_FRAGMENT : TARGET_VERTEX;
    for (const Pending& existing : gPending) {
        if (existing.key == record.key && existing.target == target) {
            return;
        }
    }
    Pending item;
    item.program = record.program;
    item.key = record.key;
    item.target = target;
    gPending.push_back(item);
}

bool AlreadyBound(const std::string& key, int target) {
    const auto it = gProgramIds.find(std::make_pair(key, target));
    return it != gProgramIds.end() && it->second != 0;
}

} // namespace

bool Init(const char* manifestJson, const char* mode, char* error, int errorSize) {
    Shutdown();
    Manifest parsed;
    std::string parseError;
    const std::string json = manifestJson ? manifestJson : "";
    if (!Manifest::Parse(json, parsed, parseError)) {
        CopyOut(error, errorSize, parseError);
        return false;
    }
    gManifest.reset(new Manifest(std::move(parsed)));
    gResolver.reset(new Resolver(*gManifest));
    gResolver->SetMode(ParseMode(mode));
    gActive = (ParseMode(mode) != Mode::Off);
    gDiagnosticSeen = 0;
    gStageIndex = -1;
    gDiagnostic.clear();
    return true;
}

bool InitEmbedded(const char* mode, char* error, int errorSize) {
    return Init(kClassicManifestJson, mode, error, errorSize);
}

void Shutdown() {
    gResolver.reset();
    gManifest.reset();
    gPending.clear();
    gProgramIds.clear();
    gActive = false;
    gStatusLine.clear();
    gReloadReport.clear();
    gDiagnosticSeen = 0;
    gDiagnostic.clear();
}

bool Active() {
    return gResolver != nullptr;
}

int ManifestStatus(const char* name) {
    if (!gManifest || name == nullptr) {
        return -1;
    }
    const std::string key = d3::shader::Resolver::NormalizeName(name);
    const std::vector<const d3::shader::ManifestEntry*>* entries = gManifest->Find(key);
    if (entries == nullptr || entries->empty()) {
        return -1;
    }
    if (entries->size() > 1) {
        return CODE_AMBIGUOUS;
    }
    switch (entries->front()->status) {
    case Status::OK: return CODE_OK;
    case Status::Stub: return CODE_STUB;
    case Status::Unsupported: return CODE_UNSUPPORTED;
    case Status::Absent: return CODE_ABSENT;
    default: return CODE_MANIFEST_INVALID;
    }
}

void SetMode(const char* mode) {
    if (gResolver) {
        gResolver->SetMode(ParseMode(mode));
        gActive = (ParseMode(mode) != Mode::Off);
    }
}

const char* ModeName() {
    return gResolver ? d3::shader::ToString(gResolver->GetMode()) : "off";
}

bool Tolerant() {
    return gResolver != nullptr && gResolver->GetMode() == Mode::Tolerant;
}

int Resolve(const char* keyword, const char* name, int target, char* outKey, int outKeySize) {
    if (outKey && outKeySize > 0) {
        outKey[0] = 0;
    }
    if (!gResolver) {
        return CODE_NOT_COMPILED;
    }
    Request request;
    request.keyword = keyword ? keyword : "fragmentProgram";
    request.name = name ? name : "";
    request.target = ToTarget(target);
    const Record record = gResolver->Resolve(request, gStageIndex);
    CopyOut(outKey, outKeySize, record.key);
    AddPending(record);
    CaptureDiagnostic();
    return ToCode(record.code);
}

void SetStage(int stageIndex) {
    gStageIndex = stageIndex;
}

void SetMaterial(const char* name) {
    if (gResolver) {
        gResolver->SetMaterial(name ? name : "");
    }
}

const char* TakeDiagnostic() {
    return gDiagnostic.empty() ? nullptr : gDiagnostic.c_str();
}

bool ResolvePair(const char* name, char* vsKey, int vsKeySize, int* vsCode,
                 char* psKey, int psKeySize, int* psCode) {
    if (vsKey && vsKeySize > 0) {
        vsKey[0] = 0;
    }
    if (psKey && psKeySize > 0) {
        psKey[0] = 0;
    }
    if (vsCode) {
        *vsCode = CODE_NOT_COMPILED;
    }
    if (psCode) {
        *psCode = CODE_NOT_COMPILED;
    }
    if (!gResolver) {
        return false;
    }
    Record vs;
    Record ps;
    const bool ok = gResolver->ResolveProgramPair(name ? name : "", gStageIndex, vs, ps);
    CopyOut(vsKey, vsKeySize, vs.key);
    CopyOut(psKey, psKeySize, ps.key);
    if (vsCode) {
        *vsCode = ToCode(vs.code);
    }
    if (psCode) {
        *psCode = ToCode(ps.code);
    }
    // FR-008: a one-sided pair installs neither stage, so neither leg is queued for blob binding.
    if (ok) {
        AddPending(vs);
        AddPending(ps);
    }
    CaptureDiagnostic();
    return ok;
}

void BeginBlobPhase() {
    gProgramIds.clear();
    if (gResolver) {
        gResolver->BeginBlobPhase();
    }
}

bool NextBlob(int* cursor, char* program, int programSize, char* key, int keySize, int* target,
              int* code) {
    if (cursor == nullptr) {
        return false;
    }
    while (*cursor >= 0 && static_cast<std::size_t>(*cursor) < gPending.size()) {
        const Pending& item = gPending[static_cast<std::size_t>(*cursor)];
        ++(*cursor);
        if (AlreadyBound(item.key, item.target)) {
            continue;
        }
        CopyOut(program, programSize, item.program);
        CopyOut(key, keySize, item.key);
        if (target) {
            *target = item.target;
        }
        if (code) {
            *code = CODE_OK;
        }
        return true;
    }
    return false;
}

void SetBlob(const char* key, int target, int code, int programId) {
    if (!gResolver || key == nullptr) {
        return;
    }
    const std::string name = key;
    const Status status = ToStatus(code);
    gResolver->SetBlob(status, name, ToTarget(target));
    const auto id = std::make_pair(name, target);
    if (status == Status::OK && programId != 0) {
        gProgramIds[id] = programId;
    } else {
        gProgramIds.erase(id);
    }
}

int ProgramId(const char* key, int target) {
    if (!gResolver || key == nullptr) {
        return 0;
    }
    const std::string name = key;
    const auto it = gProgramIds.find(std::make_pair(name, target));
    const int id = (it == gProgramIds.end()) ? 0 : it->second;
    // `off` is the documented debugging escape hatch: it reproduces the legacy draw and is never
    // accepted as evidence (resolver.md 7). Strict/tolerant/fatal gate on both phases.
    if (gResolver->GetMode() == Mode::Off) {
        return id;
    }
    if (!gResolver->MayDraw(name, ToTarget(target))) {
        return 0;
    }
    return id;
}

bool MayDraw(const char* key, int target) {
    if (gResolver == nullptr || key == nullptr) {
        return false;
    }
    if (gResolver->GetMode() == Mode::Off) {
        return true;
    }
    return gResolver->MayDraw(key, ToTarget(target));
}

const char* StatusLine() {
    gStatusLine = gResolver ? gResolver->StatusLine() : std::string("shader_status: inactive");
    return gStatusLine.c_str();
}

const char* Reload() {
    if (!gResolver) {
        gReloadReport = "shader_reload: inactive";
        return gReloadReport.c_str();
    }
    // The pending set is the durable list of requests (OK legs only; pairs are atomic). The core's
    // Reload replays and re-evaluates each logged request, so the set itself does not change.
    const std::vector<Pending> keep = gPending;
    gResolver->Reload();
    gPending = keep;
    gDiagnosticSeen = gResolver->Records().size();
    gDiagnostic.clear();
    gReloadReport = gResolver->LastReloadReport();
    return gReloadReport.c_str();
}

int SelfTest(char* report, int reportSize) {
    if (report != nullptr && reportSize > 0) {
        report[0] = 0;
    }
    std::string out;
    int failures = 0;

    const char* const kTestManifest =
        "{\"schema\":\"doom3-classic-programs/1\",\"programs\":["
        "{\"key\":\"ownedprog\",\"stages\":[\"vs\",\"ps\"],\"status\":\"owned\","
        "\"provenance\":\"PV-T01\",\"source\":\"shaders/classic/programs/test.hlsl\","
        "\"entry\":{\"vs\":\"vs_main\",\"ps\":\"ps_main\"}},"
        "{\"key\":\"onesided\",\"stages\":[\"vs\"],\"status\":\"owned\","
        "\"provenance\":\"PV-T02\",\"source\":\"shaders/classic/programs/shadow.hlsl\","
        "\"entry\":{\"vs\":\"vs_main\"}},"
        "{\"key\":\"stubprog\",\"stages\":[\"vs\",\"ps\"],\"status\":\"stub\","
        "\"provenance\":\"PV-T03\",\"notes\":\"test stub\"}]}";

    Manifest manifest;
    std::string error;
    if (!Manifest::Parse(kTestManifest, manifest, error)) {
        out += "SHADER SELFTEST: FAIL manifest: " + error + "\n";
        CopyOut(report, reportSize, out);
        return 1;
    }

    Resolver resolver(manifest);
    resolver.SetMode(Mode::Strict);
    resolver.SetMaterial("selftest/missing");

    // 1. missing program: loud, no draw.
    Request missing;
    missing.keyword = "fragmentProgram";
    missing.name = "does-not-exist.vfp";
    missing.target = d3::shader::Target::Fragment;
    const Record r1 = resolver.Resolve(missing, 0);
    if (r1.code != Status::Unresolved || r1.decision != Decision::RejectMaterial || r1.draw) {
        ++failures;
        out += "SHADER SELFTEST: FAIL missing (code=" + std::string(ToString(r1.code)) + ")\n";
    } else {
        out += "SHADER SELFTEST: PASS missing -> UNRESOLVED, material rejected, no draw\n";
    }

    // 2. one-sided file: neither stage installs.
    resolver.SetMaterial("selftest/one-sided");
    Record vs;
    Record ps;
    const bool pairOk = resolver.ResolveProgramPair("onesided.vfp", 1, vs, ps);
    if (pairOk || vs.code == Status::OK || ps.code == Status::OK) {
        ++failures;
        out += "SHADER SELFTEST: FAIL one-sided pair installed a stage\n";
    } else {
        out += "SHADER SELFTEST: PASS one-sided -> both stages blocked (" +
               std::string(ToString(vs.code)) + "/" + ToString(ps.code) + ")\n";
    }

    // 3. compile-failed blob: phase 2 blocks the draw.
    resolver.SetMaterial("selftest/compile-failed");
    resolver.SetMode(Mode::Strict);
    Request owned;
    owned.keyword = "program";
    owned.name = "ownedprog.vfp";
    owned.target = d3::shader::Target::Vertex;
    const Record vsOk = resolver.Resolve(owned, 2);
    resolver.BeginBlobPhase();
    resolver.SetBlob(Status::CompileFailed, vsOk.key, d3::shader::Target::Vertex);
    const bool compiledDraw = resolver.MayDraw(vsOk.key, d3::shader::Target::Vertex);
    if (vsOk.code != Status::OK || compiledDraw) {
        ++failures;
        out += "SHADER SELFTEST: FAIL compile-failed blob drew\n";
    } else {
        out += "SHADER SELFTEST: PASS compile-failed -> phase 2 blocks the draw\n";
    }

    // 4. tolerant: same failure is an error material, still no draw.
    resolver.SetMode(Mode::Tolerant);
    resolver.SetMaterial("selftest/tolerant");
    const Record r4 = resolver.Resolve(missing, 3);
    if (r4.decision != Decision::ErrorMaterial || r4.draw) {
        ++failures;
        out += "SHADER SELFTEST: FAIL tolerant substituted nothing\n";
    } else {
        out += "SHADER SELFTEST: PASS tolerant -> error material, no draw\n";
    }

    // 5. reload: re-resolves every request and reports a before/after delta.
    resolver.Reload();
    const std::string& reloadReport = resolver.LastReloadReport();
    if (reloadReport.find("shader_reload: non_ok before=") == std::string::npos ||
        reloadReport.find(" after=") == std::string::npos) {
        ++failures;
        out += "SHADER SELFTEST: FAIL reload report missing before/after\n";
    } else {
        out += "SHADER SELFTEST: PASS reload -> before/after report\n";
    }

    out += (failures == 0) ? "SHADER SELFTEST: PASS 5/5\n"
                           : "SHADER SELFTEST: FAIL " + std::to_string(failures) + "/5\n";
    CopyOut(report, reportSize, out);
    return failures;
}

} // namespace shaderport
} // namespace d3
