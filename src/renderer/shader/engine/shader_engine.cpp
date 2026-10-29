// Implementation of the engine-facing resolver facade (spec 008).
// Port-owned C++17; compiled into the engine target at /std:c++17. The engine call sites stay at the
// engine's default language level and see only the C-string API in shader_engine.h.
#include "shader_engine.h"

#include "../shader_resolver.h"

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
    if (record.code != Status::OK || record.key.empty()) {
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
    return true;
}

void Shutdown() {
    gResolver.reset();
    gManifest.reset();
    gPending.clear();
    gProgramIds.clear();
    gActive = false;
    gStatusLine.clear();
    gReloadReport.clear();
}

bool Active() {
    return gActive && gResolver != nullptr;
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
    const Record record = gResolver->Resolve(request, -1);
    CopyOut(outKey, outKeySize, record.key);
    AddPending(record);
    return ToCode(record.code);
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
    const bool ok = gResolver->ResolveProgramPair(name ? name : "", -1, vs, ps);
    CopyOut(vsKey, vsKeySize, vs.key);
    CopyOut(psKey, psKeySize, ps.key);
    if (vsCode) {
        *vsCode = ToCode(vs.code);
    }
    if (psCode) {
        *psCode = ToCode(ps.code);
    }
    AddPending(vs);
    AddPending(ps);
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
    if (!gResolver->MayDraw(name, ToTarget(target))) {
        return 0;
    }
    const auto it = gProgramIds.find(std::make_pair(name, target));
    return it == gProgramIds.end() ? 0 : it->second;
}

bool MayDraw(const char* key, int target) {
    return gResolver != nullptr && key != nullptr && gResolver->MayDraw(key, ToTarget(target));
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
    gResolver->Reload();
    gReloadReport = gResolver->LastReloadReport();
    return gReloadReport.c_str();
}

} // namespace shaderport
} // namespace d3
