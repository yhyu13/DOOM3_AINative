// Standalone tests for the fail-loud shader resolver core (spec 008).
//
// Usage: shader_resolver_test <subcommand> [--repo-root <path>] [--vectors <path>]
// Exit codes: 0 pass, 1 assertion failure, 77 required fixture absent.

#include "shader_json.h"
#include "shader_resolver.h"

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef SHADER_REPO_ROOT
#define SHADER_REPO_ROOT "."
#endif

namespace {

using namespace d3::shader;

std::string g_repoRoot = SHADER_REPO_ROOT;
std::string g_vectorsPath;
int g_failures = 0;

void Expect(bool condition, const std::string& what) {
    if (!condition) {
        std::cout << "FAIL: " << what << "\n";
        ++g_failures;
    }
}

std::string RepoPath(const std::string& relative) {
    if (g_repoRoot.empty()) {
        return relative;
    }
    if (!relative.empty() && (relative.front() == '/' || relative.front() == '\\')) {
        return relative;
    }
    const char last = g_repoRoot.back();
    if (last == '/' || last == '\\') {
        return g_repoRoot + relative;
    }
    return g_repoRoot + "/" + relative;
}

bool ReadFile(const std::string& path, std::string& out) {
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) {
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    out = buffer.str();
    return true;
}

std::string EnvironmentValue(const char* name) {
#if defined(_MSC_VER)
    char* buffer = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&buffer, &length, name) != 0 || buffer == nullptr) {
        return std::string();
    }
    const std::string value(buffer);
    std::free(buffer);
    return value;
#else
    const char* value = std::getenv(name);
    return (value != nullptr) ? std::string(value) : std::string();
#endif
}

std::string ResolveVectorsPath() {
    if (!g_vectorsPath.empty()) {
        return g_vectorsPath;
    }
    const std::string fromEnvironment = EnvironmentValue("SHADER_VECTORS_PATH");
    if (!fromEnvironment.empty()) {
        return fromEnvironment;
    }
    return RepoPath("specs/008-classic-shaders/tools/resolver-vectors.json");
}

json::Value WrapManifestArray(const json::Value& programs) {
    json::Value document;
    document.type = json::Type::Object;

    json::Value schema;
    schema.type = json::Type::String;
    schema.string = "doom3-classic-programs/1";
    document.object.emplace_back("schema", std::move(schema));

    json::Value copy = programs;
    document.object.emplace_back("programs", std::move(copy));
    return document;
}

bool GetString(const json::Value& object, const char* field, std::string& out) {
    const json::Value* value = object.Find(field);
    if (value == nullptr || !value->IsString()) {
        return false;
    }
    out = value->string;
    return true;
}

bool ParseMode(const std::string& text, Mode& out) {
    if (text == "off") {
        out = Mode::Off;
        return true;
    }
    if (text == "tolerant") {
        out = Mode::Tolerant;
        return true;
    }
    if (text == "strict") {
        out = Mode::Strict;
        return true;
    }
    if (text == "fatal") {
        out = Mode::Fatal;
        return true;
    }
    return false;
}

bool ParseTarget(const std::string& text, Target& out) {
    if (text == "vs") {
        out = Target::Vertex;
        return true;
    }
    if (text == "ps") {
        out = Target::Fragment;
        return true;
    }
    return false;
}

enum class LoadResult { Ok, Failed, Absent };

LoadResult LoadVectorsManifest(Manifest& manifest, std::string& message) {
    std::string text;
    if (!ReadFile(ResolveVectorsPath(), text)) {
        message = "vectors fixture absent: " + ResolveVectorsPath();
        return LoadResult::Absent;
    }
    json::Value root;
    if (!json::Parse(text, root, message)) {
        message = "vectors JSON: " + message;
        return LoadResult::Failed;
    }
    const json::Value* programs = root.Find("manifest");
    if (programs == nullptr || !programs->IsArray()) {
        message = "vectors fixture has no 'manifest' array";
        return LoadResult::Failed;
    }
    json::Value document = WrapManifestArray(*programs);
    std::string documentText;
    json::Write(document, documentText);
    if (!Manifest::Parse(documentText, manifest, message)) {
        message = "embedded manifest: " + message;
        return LoadResult::Failed;
    }
    return LoadResult::Ok;
}

int Finish() {
    return g_failures == 0 ? 0 : 1;
}

Record AggregatePair(const Record& vs, const Record& ps) {
    Record result = (vs.code != Status::OK) ? vs : ps;
    if (vs.code == Status::OK && ps.code == Status::OK) {
        result = vs;
    }
    result.draw = vs.draw && ps.draw;
    return result;
}

bool ExtractCount(const std::string& line, const std::string& field, std::size_t& out) {
    const std::string needle = field + "=";
    std::size_t start = line.find(needle);
    if (start == std::string::npos) {
        return false;
    }
    start += needle.size();
    std::size_t end = start;
    while (end < line.size() && line[end] >= '0' && line[end] <= '9') {
        ++end;
    }
    if (end == start) {
        return false;
    }
    out = static_cast<std::size_t>(std::stoul(line.substr(start, end - start)));
    return true;
}

void ExpectCount(const std::string& line, const std::string& field, std::size_t expected) {
    std::size_t actual = 0;
    if (!ExtractCount(line, field, actual)) {
        Expect(false, "status line has no '" + field + "' counter: " + line);
        return;
    }
    if (actual != expected) {
        Expect(false, "status line " + field + "=" + std::to_string(actual) + ", expected " +
                          std::to_string(expected));
    }
}

// ---------------------------------------------------------------------------

int RunVectors() {
    const std::string path = ResolveVectorsPath();
    std::string text;
    if (!ReadFile(path, text)) {
        std::cout << "SKIP: vectors fixture absent: " << path << "\n";
        return 77;
    }

    json::Value root;
    std::string error;
    if (!json::Parse(text, root, error)) {
        std::cout << "FAIL: vectors JSON: " << error << "\n";
        return 1;
    }

    const json::Value* manifestArray = root.Find("manifest");
    const json::Value* cases = root.Find("cases");
    Expect(manifestArray != nullptr && manifestArray->IsArray(),
           "vectors: 'manifest' must be an array");
    Expect(cases != nullptr && cases->IsArray(), "vectors: 'cases' must be an array");
    if (g_failures != 0) {
        return 1;
    }

    Expect(cases->array.size() == 14,
           "vectors: expected exactly 14 cases, found " + std::to_string(cases->array.size()));

    json::Value document = WrapManifestArray(*manifestArray);
    std::string manifestText;
    json::Write(document, manifestText);

    Manifest manifest;
    std::string manifestError;
    if (!Manifest::Parse(manifestText, manifest, manifestError)) {
        std::cout << "FAIL: embedded manifest: " << manifestError << "\n";
        return 1;
    }

    std::set<std::string> ids;
    std::size_t caseIndex = 0;
    for (const json::Value& item : cases->array) {
        ++caseIndex;
        const std::string position = "case #" + std::to_string(caseIndex);

        std::string id;
        if (!GetString(item, "id", id) || id.empty()) {
            Expect(false, position + ": missing string 'id'");
            continue;
        }
        Expect(ids.insert(id).second, "vectors: duplicate case id '" + id + "'");

        const json::Value* request = item.Find("request");
        const json::Value* expect = item.Find("expect");
        std::string modeText;
        if (request == nullptr || !request->IsObject() || expect == nullptr ||
            !expect->IsObject() || !GetString(item, "mode", modeText)) {
            Expect(false, id + ": missing request/expect/mode");
            continue;
        }

        std::string keyword;
        std::string name;
        if (!GetString(*request, "keyword", keyword) || !GetString(*request, "name", name)) {
            Expect(false, id + ": request requires string 'keyword' and 'name'");
            continue;
        }

        Mode mode = Mode::Strict;
        if (!ParseMode(modeText, mode)) {
            Expect(false, id + ": unknown mode '" + modeText + "'");
            continue;
        }

        std::string expectedCode;
        std::string expectedDecision;
        const json::Value* expectedDraw = expect->Find("draw");
        if (!GetString(*expect, "code", expectedCode) ||
            !GetString(*expect, "decision", expectedDecision) || expectedDraw == nullptr ||
            !expectedDraw->IsBool()) {
            Expect(false, id + ": expect requires code/decision/draw");
            continue;
        }

        Resolver resolver(manifest);
        resolver.SetMode(mode);
        resolver.SetMaterial("material/" + id);

        Record actual;
        if (keyword == "program") {
            Record vs;
            Record ps;
            resolver.ResolveProgramPair(name, 1, vs, ps);
            actual = AggregatePair(vs, ps);
        } else {
            std::string targetText;
            Target target = Target::Vertex;
            if (!GetString(*request, "target", targetText) || !ParseTarget(targetText, target)) {
                Expect(false, id + ": request requires target 'vs' or 'ps'");
                continue;
            }
            Request requestValue;
            requestValue.keyword = keyword;
            requestValue.name = name;
            requestValue.target = target;
            actual = resolver.Resolve(requestValue, 1);
        }

        const std::string actualCode = ToString(actual.code);
        const std::string actualDecision = ToString(actual.decision);
        if (actualCode != expectedCode || actualDecision != expectedDecision ||
            actual.draw != expectedDraw->boolean) {
            std::cout << "FAIL: " << id << " expected code=" << expectedCode
                      << " decision=" << expectedDecision
                      << " draw=" << (expectedDraw->boolean ? "true" : "false")
                      << "; got code=" << actualCode << " decision=" << actualDecision
                      << " draw=" << (actual.draw ? "true" : "false") << "\n";
            ++g_failures;
        }
    }

    return Finish();
}

// ---------------------------------------------------------------------------

int RunManifestShipping() {
    const std::string path = RepoPath("shaders/manifest/classic-programs.json");
    std::string text;
    if (!ReadFile(path, text)) {
        std::cout << "SKIP: shipping manifest absent: " << path << "\n";
        return 77;
    }

    Manifest manifest;
    std::string error;
    if (!Manifest::Parse(text, manifest, error)) {
        std::cout << "FAIL: shipping manifest: " << error << "\n";
        return 1;
    }

    Expect(manifest.Size() == 14,
           "shipping manifest: expected 14 keys, found " + std::to_string(manifest.Size()));

    struct ExpectedEntry {
        const char* key;
        Status status;
    };
    const ExpectedEntry expected[] = {
        {"interaction", Status::Stub},
        {"environment", Status::Stub},
        {"bumpyenvironment", Status::Stub},
        {"shadow", Status::Stub},
        {"arbfp_glasswarp", Status::Stub},
        {"test", Status::Stub},
        {"megatexture", Status::Unsupported},
        {"ambientlight", Status::Absent},
        {"arbvp_glasswarp", Status::Absent},
        {"r200_interaction", Status::Absent},
        {"nv20_bumpandlight", Status::Absent},
        {"nv20_diffusecolor", Status::Absent},
        {"nv20_specularcolor", Status::Absent},
        {"nv20_diffuseandspecularcolor", Status::Absent},
    };

    std::vector<std::string> missingSources;
    std::size_t stubExpected = 0;
    std::size_t unsupportedExpected = 0;
    std::size_t absentExpected = 0;

    for (const ExpectedEntry& item : expected) {
        if (item.status == Status::Stub) {
            ++stubExpected;
        } else if (item.status == Status::Unsupported) {
            ++unsupportedExpected;
        } else if (item.status == Status::Absent) {
            ++absentExpected;
        }

        const std::vector<const ManifestEntry*>* found = manifest.Find(item.key);
        if (found == nullptr || found->size() != 1) {
            Expect(false, std::string("shipping manifest: key '") + item.key + "' missing or duplicated");
            continue;
        }
        const ManifestEntry& entry = *found->front();
        Expect(entry.status == item.status,
               std::string("shipping manifest: '") + item.key + "' status is " +
                   ToString(entry.status) + ", expected " + ToString(item.status));
        Expect(entry.hasVs || entry.hasPs,
               std::string("shipping manifest: '") + item.key + "' declares no stage");

        if (entry.status == Status::Stub && !entry.source.empty()) {
            const std::string sourcePath = RepoPath(entry.source);
            std::ifstream probe(sourcePath.c_str(), std::ios::binary);
            if (!probe) {
                missingSources.push_back(entry.source);
            }
            if (entry.hasVs) {
                Expect(!entry.entryVs.empty(),
                       std::string("shipping manifest: stub '") + item.key + "' has no vs entry name");
            }
            if (entry.hasPs) {
                Expect(!entry.entryPs.empty(),
                       std::string("shipping manifest: stub '") + item.key + "' has no ps entry name");
            }
        }
    }

    Expect(manifest.CountOf(Status::OK) == 0, "shipping manifest: no entry is owned yet");
    Expect(manifest.CountOf(Status::Stub) == stubExpected,
           "shipping manifest: stub count is " + std::to_string(manifest.CountOf(Status::Stub)) +
               ", expected " + std::to_string(stubExpected));
    Expect(manifest.CountOf(Status::Unsupported) == unsupportedExpected,
           "shipping manifest: unsupported count");
    Expect(manifest.CountOf(Status::Absent) == absentExpected,
           "shipping manifest: absent count");

    if (g_failures != 0) {
        return 1;
    }

    if (!missingSources.empty()) {
        std::cout << "SKIP: authored shader sources referenced by stub entries are absent:\n";
        for (const std::string& source : missingSources) {
            std::cout << "  " << source << "\n";
        }
        return 77;
    }

    return Finish();
}

// ---------------------------------------------------------------------------

int RunPhase2() {
    Manifest manifest;
    std::string message;
    const LoadResult loaded = LoadVectorsManifest(manifest, message);
    if (loaded == LoadResult::Absent) {
        std::cout << "SKIP: " << message << "\n";
        return 77;
    }
    if (loaded == LoadResult::Failed) {
        std::cout << "FAIL: " << message << "\n";
        return 1;
    }

    Resolver resolver(manifest);
    resolver.SetMode(Mode::Strict);
    resolver.SetMaterial("materials/phase2");

    Request request;
    request.keyword = "vertexProgram";
    request.name = "interaction.vfp";
    request.target = Target::Vertex;
    const Record phase1 = resolver.Resolve(request, 0);
    Expect(phase1.code == Status::OK, "phase2: phase 1 resolves interaction.vfp");
    const std::string key = phase1.key;

    Expect(!resolver.MayDraw(key, Target::Vertex), "phase2: MayDraw false before any blob phase");

    resolver.BeginBlobPhase();
    Expect(!resolver.MayDraw(key, Target::Vertex),
           "phase2: MayDraw false when no blob is bound");

    resolver.SetBlob(Status::CompileFailed, key, Target::Vertex);
    Expect(!resolver.MayDraw(key, Target::Vertex), "phase2: MayDraw false on COMPILE_FAILED");
    const Record failedGate = resolver.GateForDraw(key, Target::Vertex);
    Expect(failedGate.code == Status::CompileFailed,
           "phase2: GateForDraw reports COMPILE_FAILED, got " + std::string(ToString(failedGate.code)));
    Expect(!failedGate.draw, "phase2: COMPILE_FAILED gate does not draw");

    resolver.SetBlob(Status::NotCompiled, key, Target::Vertex);
    Expect(!resolver.MayDraw(key, Target::Vertex), "phase2: MayDraw false on NOT_COMPILED");

    resolver.SetBlob(Status::OK, key, Target::Vertex);
    Expect(resolver.MayDraw(key, Target::Vertex), "phase2: MayDraw true when both phases are OK");
    const Record okGate = resolver.GateForDraw(key, Target::Vertex);
    Expect(okGate.draw && okGate.code == Status::OK, "phase2: OK gate draws");

    resolver.Reload();
    Expect(resolver.RequestCount() == 1,
           "phase2: reload replays the logged request once, got " +
               std::to_string(resolver.RequestCount()));
    Expect(resolver.Records().empty(), "phase2: reload keeps the record set empty");
    Expect(!resolver.MayDraw(key, Target::Vertex), "phase2: reload clears phase 2");
    ExpectCount(resolver.StatusLine(), "requests", 1);
    ExpectCount(resolver.StatusLine(), "ok", 1);
    Expect(resolver.LastReloadReport().find("before=0 after=0") != std::string::npos,
           "phase2: reload report states before/after non-OK counts");

    return Finish();
}

// ---------------------------------------------------------------------------

int RunDiagnostics() {
    Manifest manifest;
    std::string message;
    const LoadResult loaded = LoadVectorsManifest(manifest, message);
    if (loaded == LoadResult::Absent) {
        std::cout << "SKIP: " << message << "\n";
        return 77;
    }
    if (loaded == LoadResult::Failed) {
        std::cout << "FAIL: " << message << "\n";
        return 1;
    }

    Resolver resolver(manifest);
    resolver.SetMode(Mode::Strict);
    resolver.SetMaterial("textures/base/wall");

    Request request;
    request.keyword = "vertexProgram";
    request.name = "foo.vfp";
    request.target = Target::Vertex;

    const Record record = resolver.Resolve(request, 2);
    Expect(record.code == Status::Unresolved, "diagnostics: missing program is UNRESOLVED");
    Expect(record.decision == Decision::RejectMaterial,
           "diagnostics: strict mode rejects the material");
    Expect(!record.draw, "diagnostics: missing program does not draw");
    Expect(resolver.Records().size() == 1, "diagnostics: exactly one emitted record");

    const Record& stored = resolver.Records().front();
    Expect(stored.material == "textures/base/wall", "diagnostics: material field populated");
    Expect(stored.stageIndex == 2, "diagnostics: stage index field populated");
    Expect(stored.keyword == "vertexProgram", "diagnostics: keyword field populated");
    Expect(stored.program == "foo.vfp", "diagnostics: program field populated");
    Expect(stored.key == "foo", "diagnostics: normalized key field populated");
    Expect(stored.target == Target::Vertex, "diagnostics: target field populated");
    Expect(stored.code == Status::Unresolved, "diagnostics: code field populated");
    Expect(stored.mode == Mode::Strict, "diagnostics: mode field populated");
    Expect(!stored.detail.empty(), "diagnostics: detail text populated");
    Expect(!stored.action.empty(), "diagnostics: action text populated");

    const std::string block = resolver.Describe(stored);
    Expect(block.rfind("SHADER ERROR:", 0) == 0, "diagnostics: block starts with SHADER ERROR:");
    Expect(block.find("material=\"textures/base/wall\"") != std::string::npos,
           "diagnostics: block names the material");
    Expect(block.find("code=UNRESOLVED") != std::string::npos,
           "diagnostics: block names the code");
    Expect(block.find("action=\"material rejected; stage not drawn\"") != std::string::npos,
           "diagnostics: block states the visible consequence");

    resolver.Resolve(request, 2);
    Expect(resolver.Records().size() == 1, "diagnostics: repeat does not add a record");
    Expect(resolver.SuppressedRepeats() == 1,
           "diagnostics: repeat increments SuppressedRepeats, got " +
               std::to_string(resolver.SuppressedRepeats()));

    const std::string line = resolver.StatusLine();
    ExpectCount(line, "manifest", manifest.Size());
    ExpectCount(line, "owned", manifest.CountOf(Status::OK));
    ExpectCount(line, "stub", manifest.CountOf(Status::Stub));
    ExpectCount(line, "unsupported", manifest.CountOf(Status::Unsupported));
    ExpectCount(line, "absent", manifest.CountOf(Status::Absent));
    ExpectCount(line, "requests", 2);
    ExpectCount(line, "ok", 0);
    ExpectCount(line, "unresolved", 2);
    ExpectCount(line, "stage_missing", 0);
    ExpectCount(line, "ambiguous", 0);
    ExpectCount(line, "materials_rejected", 1);
    ExpectCount(line, "materials_error", 0);
    Expect(line.find("mode=strict") != std::string::npos, "diagnostics: status line names the mode");

    return Finish();
}

// ---------------------------------------------------------------------------

int RunManifestInvalid() {
    const char* valid =
        R"({"schema":"doom3-classic-programs/1","programs":[{"key":"a","stages":["vs"],"status":"owned","provenance":"PV-1","source":"shaders/a.hlsl","entry":{"vs":"vs_main"}}]})";

    Manifest control;
    std::string controlError;
    Expect(Manifest::Parse(valid, control, controlError),
           "manifest_invalid: valid control parses (" + controlError + ")");

    struct Negative {
        const char* name;
        const char* json;
    };
    const Negative negatives[] = {
        {"missing schema",
         R"({"programs":[]})"},
        {"unknown schema",
         R"({"schema":"doom3-classic-programs/2","programs":[]})"},
        {"programs not an array",
         R"({"schema":"doom3-classic-programs/1","programs":{}})"},
        {"programs missing",
         R"({"schema":"doom3-classic-programs/1"})"},
        {"entry missing key",
         R"({"schema":"doom3-classic-programs/1","programs":[{"stages":["vs"],"status":"owned","provenance":"PV-1","source":"s.hlsl","entry":{"vs":"m"}}]})"},
        {"entry missing stages",
         R"({"schema":"doom3-classic-programs/1","programs":[{"key":"a","status":"owned","provenance":"PV-1","source":"s.hlsl","entry":{"vs":"m"}}]})"},
        {"entry missing status",
         R"({"schema":"doom3-classic-programs/1","programs":[{"key":"a","stages":["vs"],"provenance":"PV-1","source":"s.hlsl","entry":{"vs":"m"}}]})"},
        {"entry missing provenance",
         R"({"schema":"doom3-classic-programs/1","programs":[{"key":"a","stages":["vs"],"status":"owned","source":"s.hlsl","entry":{"vs":"m"}}]})"},
        {"empty stages",
         R"({"schema":"doom3-classic-programs/1","programs":[{"key":"a","stages":[],"status":"owned","provenance":"PV-1","source":"s.hlsl","entry":{}}]})"},
        {"unknown stage token",
         R"({"schema":"doom3-classic-programs/1","programs":[{"key":"a","stages":["gs"],"status":"owned","provenance":"PV-1","source":"s.hlsl","entry":{"gs":"m"}}]})"},
        {"planned status",
         R"({"schema":"doom3-classic-programs/1","programs":[{"key":"a","stages":["vs"],"status":"planned","provenance":"PV-1","notes":"x"}]})"},
        {"non-enumerated status",
         R"({"schema":"doom3-classic-programs/1","programs":[{"key":"a","stages":["vs"],"status":"wip","provenance":"PV-1","notes":"x"}]})"},
        {"owned without source",
         R"({"schema":"doom3-classic-programs/1","programs":[{"key":"a","stages":["vs"],"status":"owned","provenance":"PV-1","entry":{"vs":"m"}}]})"},
        {"owned without entry",
         R"({"schema":"doom3-classic-programs/1","programs":[{"key":"a","stages":["vs"],"status":"owned","provenance":"PV-1","source":"s.hlsl"}]})"},
        {"owned entry misses declared stage",
         R"({"schema":"doom3-classic-programs/1","programs":[{"key":"a","stages":["vs","ps"],"status":"owned","provenance":"PV-1","source":"s.hlsl","entry":{"vs":"m"}}]})"},
        {"entry names undeclared stage",
         R"({"schema":"doom3-classic-programs/1","programs":[{"key":"a","stages":["vs"],"status":"owned","provenance":"PV-1","source":"s.hlsl","entry":{"vs":"m","ps":"n"}}]})"},
        {"stub without notes",
         R"({"schema":"doom3-classic-programs/1","programs":[{"key":"a","stages":["vs"],"status":"stub","provenance":"PV-1"}]})"},
        {"stub with empty notes",
         R"({"schema":"doom3-classic-programs/1","programs":[{"key":"a","stages":["vs"],"status":"stub","provenance":"PV-1","notes":""}]})"},
        {"absent without notes",
         R"({"schema":"doom3-classic-programs/1","programs":[{"key":"a","stages":["vs"],"status":"absent","provenance":"PV-1"}]})"},
        {"absent with source",
         R"({"schema":"doom3-classic-programs/1","programs":[{"key":"a","stages":["vs"],"status":"absent","provenance":"PV-1","source":"s.hlsl","notes":"x"}]})"},
        {"unsupported with source",
         R"({"schema":"doom3-classic-programs/1","programs":[{"key":"a","stages":["vs"],"status":"unsupported","provenance":"PV-1","source":"s.hlsl","notes":"x"}]})"},
        {"entry is not an object",
         R"({"schema":"doom3-classic-programs/1","programs":[42]})"},
    };

    std::size_t negativeCount = 0;
    for (const Negative& negative : negatives) {
        ++negativeCount;
        Manifest parsed;
        std::string error;
        const bool accepted = Manifest::Parse(negative.json, parsed, error);
        if (accepted) {
            std::cout << "FAIL: manifest_invalid: accepted '" << negative.name << "'\n";
            ++g_failures;
        } else if (error.empty()) {
            std::cout << "FAIL: manifest_invalid: '" << negative.name
                      << "' rejected with an empty error\n";
            ++g_failures;
        }
    }
    Expect(negativeCount >= 8, "manifest_invalid: at least 8 negative manifests");

    return Finish();
}

// ---------------------------------------------------------------------------

int RunMaterialAtomic() {
    Manifest manifest;
    std::string message;
    const LoadResult loaded = LoadVectorsManifest(manifest, message);
    if (loaded == LoadResult::Absent) {
        std::cout << "SKIP: " << message << "\n";
        return 77;
    }
    if (loaded == LoadResult::Failed) {
        std::cout << "FAIL: " << message << "\n";
        return 1;
    }

    Resolver resolver(manifest);
    resolver.SetMode(Mode::Strict);
    resolver.SetMaterial("materials/one_sided");

    Record vs;
    Record ps;
    const bool installed = resolver.ResolveProgramPair("one_sided.vfp", 3, vs, ps);

    Expect(!installed, "material_atomic: one-sided pair is not installed");
    Expect(vs.code != Status::OK, "material_atomic: vs stage reports non-OK");
    Expect(ps.code != Status::OK, "material_atomic: ps stage reports non-OK");
    Expect(vs.code == Status::StageMissing && ps.code == Status::StageMissing,
           "material_atomic: the missing stage is the reason for both");
    Expect(resolver.Records().size() == 2,
           "material_atomic: both stages are recorded, got " +
               std::to_string(resolver.Records().size()));
    Expect(resolver.RequestCount() == 2, "material_atomic: pair counts two stage requests");

    const StageGate gate = resolver.PairGate(vs, ps);
    Expect(!gate.ok, "material_atomic: pair gate refuses the pair");
    Expect(gate.phase1 != Status::OK, "material_atomic: pair gate reports the phase-1 failure");
    Expect(!resolver.MayDraw(vs.key, vs.target), "material_atomic: vs is not installable");
    Expect(!resolver.MayDraw(ps.key, ps.target), "material_atomic: ps is not installable");

    Record goodVs;
    Record goodPs;
    const bool goodPair = resolver.ResolveProgramPair("environment.vfp", 4, goodVs, goodPs);
    Expect(goodPair, "material_atomic: a two-sided pair installs");
    Expect(goodVs.code == Status::OK && goodPs.code == Status::OK,
           "material_atomic: both stages of a complete pair are OK");

    return Finish();
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "usage: shader_resolver_test <vectors|manifest_shipping|phase2|"
                     "diagnostics|manifest_invalid|material_atomic> "
                     "[--repo-root <path>] [--vectors <path>]\n";
        return 1;
    }

    for (int i = 2; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--repo-root" && i + 1 < argc) {
            g_repoRoot = argv[++i];
        } else if (argument == "--vectors" && i + 1 < argc) {
            g_vectorsPath = argv[++i];
        } else if (argument.rfind("--repo-root=", 0) == 0) {
            g_repoRoot = argument.substr(std::string("--repo-root=").size());
        } else if (argument.rfind("--vectors=", 0) == 0) {
            g_vectorsPath = argument.substr(std::string("--vectors=").size());
        }
    }

    const std::string command = argv[1];
    if (command == "vectors") {
        return RunVectors();
    }
    if (command == "manifest_shipping") {
        return RunManifestShipping();
    }
    if (command == "phase2") {
        return RunPhase2();
    }
    if (command == "diagnostics") {
        return RunDiagnostics();
    }
    if (command == "manifest_invalid") {
        return RunManifestInvalid();
    }
    if (command == "material_atomic") {
        return RunMaterialAtomic();
    }

    std::cout << "unknown subcommand: " << command << "\n";
    return 1;
}
