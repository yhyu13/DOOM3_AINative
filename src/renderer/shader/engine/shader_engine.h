// Engine-facing facade over the port-owned resolver core (spec 008, tasks T-021/T-029).
//
// This header is included by engine translation units whose precompiled header poisons standard
// library names (for example `snprintf`), so it must not include any standard header. Everything
// crosses the boundary as plain C strings and integers; the resolver itself is C++17 and is compiled
// separately by the target build.
//
// Ownership: shaders/classic/** and src/renderer/shader/** are port code. The engine call sites are
// patched through targets/<name>/patches/ (see targets/README.md).
#pragma once

namespace d3 {
namespace shaderport {

// Mirrors d3::shader::Status. Kept as plain ints so the engine can store them in POD structs.
enum Code {
    CODE_OK = 0,
    CODE_UNRESOLVED = 1,
    CODE_STAGE_MISSING = 2,
    CODE_STUB = 3,
    CODE_UNSUPPORTED = 4,
    CODE_ABSENT = 5,
    CODE_AMBIGUOUS = 6,
    CODE_NAME_TOO_LONG = 7,
    CODE_MANIFEST_INVALID = 8,
    CODE_COMPILE_FAILED = 9,
    CODE_NOT_COMPILED = 10
};

enum Target { TARGET_VERTEX = 0, TARGET_FRAGMENT = 1 };

// Loads the manifest text into a fresh resolver. mode is one of off|tolerant|strict|fatal.
// Returns false and copies the reason into error on a malformed manifest.
bool Init(const char* manifestJson, const char* mode, char* error, int errorSize);

// Loads the manifest embedded at build time from shaders/manifest/classic-programs.json, so the
// engine has one source of truth and no runtime file dependency. Same return contract as Init().
bool InitEmbedded(const char* mode, char* error, int errorSize);

// Drops the resolver. Safe to call when not initialised.
void Shutdown();

// True when a manifest is loaded. `off` is a loaded mode (it draws, but it is not evidence).
bool Active();

// Coverage probe used by the engine table check. Returns -1 when the key is not in the manifest,
// otherwise the entry's manifest Code (CODE_OK/CODE_STUB/CODE_UNSUPPORTED/CODE_ABSENT).
// Does not count a request and does not change resolver state.
int ManifestStatus(const char* name);

void SetMode(const char* mode);
const char* ModeName();

// True when the active mode is `tolerant`, i.e. a non-OK stage must draw the error material
// (resolver.md I-5). False for off/strict/fatal.
bool Tolerant();

// Phase 1 (material parse time): no GL, no blob. Returns the Code and, on success, the key.
int Resolve(const char* keyword, const char* name, int target, char* outKey, int outKeySize);

// The material a subsequent Resolve() belongs to; used only by the diagnostic text.
void SetMaterial(const char* name);

// The stage index a subsequent Resolve() belongs to; used only by the diagnostic text.
void SetStage(int stageIndex);

// Structured text for the last resolution when it produced a new non-OK record (nullptr otherwise).
// The pointer stays valid until the next resolver call.
const char* TakeDiagnostic();

// Atomic `program` pair: either both stages resolve OK or neither is installable.
// Returns true only when both are OK; the Code outputs are always filled.
bool ResolvePair(const char* name, char* vsKey, int vsKeySize, int* vsCode,
                 char* psKey, int psKeySize, int* psCode);

// Phase 2 (after GL init): the engine asks for each pending (program, target), loads it, and reports
// the blob status back. `program` is the name as requested (extension included), `key` normalised.
void BeginBlobPhase();
bool NextBlob(int* cursor, char* program, int programSize, char* key, int keySize, int* target,
              int* code);
void SetBlob(const char* key, int target, int code, int programId);

// Draw gate. ProgramId returns 0 when the stage must not be bound; MayDraw is the boolean form.
int ProgramId(const char* key, int target);
bool MayDraw(const char* key, int target);

// Diagnostics. Both return pointers that stay valid until the next call of the same function.
const char* StatusLine();
const char* Reload();

// In-engine test of the resolver contract (T-024): missing, one-sided, compile-failed, tolerant and
// reload, against local resolver instances. Never touches the shipping resolver. Returns the number
// of failed cases (0 = all passed) and writes a one-line-per-case report.
int SelfTest(char* report, int reportSize);

} // namespace shaderport
} // namespace d3
