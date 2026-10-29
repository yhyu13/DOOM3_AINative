// Fail-loud shader resolver core (spec 008).
//
// Port-owned C++17. No engine headers, no idLib types, no RHI or native graphics API
// dependency: the resolver can be built and unit tested on its own.
#pragma once

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace d3::shader {

enum class Target { Vertex, Fragment };
enum class Status {
    OK,
    Unresolved,
    StageMissing,
    Stub,
    Unsupported,
    Absent,
    Ambiguous,
    NameTooLong,
    ManifestInvalid,
    CompileFailed,
    NotCompiled
};

const char* ToString(Status status);
const char* ToString(Target target);

enum class Mode { Off, Tolerant, Strict, Fatal };
enum class Decision { Draw, RejectMaterial, ErrorMaterial, Fatal };

const char* ToString(Mode mode);
const char* ToString(Decision decision);

struct ManifestEntry {
    std::string key;
    std::string provenance;
    std::string source;
    std::string notes;
    bool hasVs = false;
    bool hasPs = false;
    // One of OK (owned) / Stub / Unsupported / Absent.
    Status status = Status::OK;
    std::string entryVs;
    std::string entryPs;
    int vertexParms = -1;
    int fragmentMaps = -1;
};

class Manifest {
public:
    Manifest() = default;
    Manifest(const Manifest& other);
    Manifest& operator=(const Manifest& other);
    Manifest(Manifest&& other) noexcept;
    Manifest& operator=(Manifest&& other) noexcept;
    ~Manifest() = default;

    // Strict per resolver.md 4.1. Returns false and fills `error` on any defect.
    static bool Parse(const std::string& json, Manifest& out, std::string& error);

    std::size_t Size() const { return entries_.size(); }
    std::size_t CountOf(Status status) const;

    // nullptr means "no entry"; a non-null vector with more than one element is
    // the resolve-time AMBIGUOUS case.
    const std::vector<const ManifestEntry*>* Find(const std::string& key) const;

private:
    void RebuildIndex();

    std::vector<ManifestEntry> entries_;
    std::unordered_map<std::string, std::vector<const ManifestEntry*>> index_;
};

struct Request {
    std::string keyword; // vertexProgram | fragmentProgram | program
    std::string name;
    Target target = Target::Vertex;
};

struct Record {
    std::string material;
    int stageIndex = -1;
    std::string keyword;
    std::string program; // the name as requested
    std::string key;     // normalized
    Target target = Target::Vertex;
    Status code = Status::OK;
    Mode mode = Mode::Strict;
    Decision decision = Decision::Draw;
    std::string detail;
    std::string action;
    bool draw = false;
};

struct StageGate {
    bool ok = false;
    std::string key;
    Status phase1 = Status::OK;
    Status phase2 = Status::OK;
};

class Resolver {
public:
    explicit Resolver(const Manifest& manifest);

    void SetMode(Mode mode);
    Mode GetMode() const;
    void SetMaterial(const std::string& name);

    // Phase 1: name resolution. No device, no blob. Counts one request.
    Record Resolve(const Request& request, int stageIndex);

    // Atomic pair for the `program` keyword. If either stage is non-OK, neither
    // stage is installable and both records carry a non-OK code.
    bool ResolveProgramPair(const std::string& name, int stageIndex, Record& outVs, Record& outPs);
    StageGate PairGate(const Record& vs, const Record& ps) const;

    // Phase 2: blob binding for the active backend.
    void BeginBlobPhase();
    void SetBlob(Status phase2, const std::string& key, Target target);

    // Draw gate: both phases must be OK.
    bool MayDraw(const std::string& key, Target target) const;
    Record GateForDraw(const std::string& key, Target target) const;

    // Re-resolves every logged request, re-validates, resets counters and
    // records a before/after report (LastReloadReport()).
    void Reload();

    std::string StatusLine() const;
    const std::vector<Record>& Records() const { return records_; }
    std::size_t SuppressedRepeats() const { return suppressedRepeats_; }

    // Helpers for console commands and tests.
    std::string Describe(const Record& record) const;
    const std::string& LastReloadReport() const { return lastReloadReport_; }
    std::size_t RequestCount() const { return requests_; }

    static std::string NormalizeName(const std::string& name);

private:
    struct LoggedRequest {
        bool pair = false;
        Request request;
        int stageIndex = -1;
        std::string material;
    };

    Record Evaluate(const Request& request, int stageIndex, const std::string& material) const;
    bool ResolvePairInternal(const std::string& name, int stageIndex, const std::string& material,
                             Record& outVs, Record& outPs);
    void RecordResolution(const Record& record);
    void ResetCounters();
    Decision Decide(Status code) const;
    Status BlobStatus(const std::string& key, Target target) const;
    static std::pair<std::string, int> PhaseKey(const std::string& key, Target target);

    const Manifest& manifest_;
    Mode mode_ = Mode::Strict;
    std::string material_;

    std::size_t requests_ = 0;
    std::size_t okCount_ = 0;
    std::size_t unresolvedCount_ = 0;
    std::size_t stageMissingCount_ = 0;
    std::size_t ambiguousCount_ = 0;
    std::size_t nameTooLongCount_ = 0;
    std::size_t stubCount_ = 0;
    std::size_t unsupportedCount_ = 0;
    std::size_t absentCount_ = 0;
    std::size_t compileFailedCount_ = 0;
    std::size_t notCompiledCount_ = 0;
    std::size_t nonOkCount_ = 0;
    std::size_t suppressedRepeats_ = 0;

    std::set<std::string> rejectedMaterials_;
    std::set<std::string> errorMaterials_;
    std::vector<Record> records_;
    // Identity for the "log once" rule. The target is part of the identity:
    // a `program` request resolves two stages under one stage index, and the
    // two stages are genuinely distinct failures.
    std::set<std::tuple<std::string, int, std::string, int, int>> seen_;
    std::map<std::pair<std::string, int>, Status> phase1_;
    std::map<std::pair<std::string, int>, Status> blobs_;
    std::vector<LoggedRequest> log_;
    bool blobPhaseActive_ = false;
    std::string lastReloadReport_;
};

} // namespace d3::shader
