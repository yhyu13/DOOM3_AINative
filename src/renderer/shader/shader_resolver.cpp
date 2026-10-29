#include "shader_resolver.h"

#include "shader_json.h"

#include <cstddef>
#include <limits>
#include <sstream>

namespace d3::shader {
namespace {

const char* const kManifestSchema = "doom3-classic-programs/1";
const std::size_t kMaxProgramNameBytes = 63;

bool ParseStatusToken(const std::string& token, Status& out) {
    if (token == "owned") {
        out = Status::OK;
        return true;
    }
    if (token == "stub") {
        out = Status::Stub;
        return true;
    }
    if (token == "unsupported") {
        out = Status::Unsupported;
        return true;
    }
    if (token == "absent") {
        out = Status::Absent;
        return true;
    }
    return false;
}

bool ParseArityValue(const json::Value& value, int& out) {
    if (!value.IsNumber() || !value.integer) {
        return false;
    }
    if (value.number < static_cast<double>(std::numeric_limits<int>::min()) ||
        value.number > static_cast<double>(std::numeric_limits<int>::max())) {
        return false;
    }
    out = static_cast<int>(value.number);
    return true;
}

bool ParseEntry(const json::Value& value, std::size_t index, ManifestEntry& entry, std::string& error) {
    const std::string where = "manifest programs[" + std::to_string(index) + "]";
    if (!value.IsObject()) {
        error = where + ": entry must be an object";
        return false;
    }

    const json::Value* key = value.Find("key");
    if (key == nullptr || !key->IsString() || key->string.empty()) {
        error = where + ": missing or empty 'key'";
        return false;
    }
    entry.key = key->string;

    const json::Value* stages = value.Find("stages");
    if (stages == nullptr) {
        error = where + " ('" + entry.key + "'): missing 'stages'";
        return false;
    }
    if (!stages->IsArray()) {
        error = where + " ('" + entry.key + "'): 'stages' must be an array";
        return false;
    }
    if (stages->array.empty()) {
        error = where + " ('" + entry.key + "'): 'stages' must not be empty";
        return false;
    }
    for (const json::Value& stage : stages->array) {
        if (!stage.IsString()) {
            error = where + " ('" + entry.key + "'): stage token must be a string";
            return false;
        }
        if (stage.string == "vs") {
            entry.hasVs = true;
        } else if (stage.string == "ps") {
            entry.hasPs = true;
        } else {
            error = where + " ('" + entry.key + "'): unknown stage token '" + stage.string + "'";
            return false;
        }
    }

    const json::Value* status = value.Find("status");
    if (status == nullptr) {
        error = where + " ('" + entry.key + "'): missing 'status'";
        return false;
    }
    if (!status->IsString()) {
        error = where + " ('" + entry.key + "'): 'status' must be a string";
        return false;
    }
    if (!ParseStatusToken(status->string, entry.status)) {
        error = where + " ('" + entry.key + "'): unknown status '" + status->string + "'";
        return false;
    }

    const json::Value* provenance = value.Find("provenance");
    if (provenance == nullptr || !provenance->IsString() || provenance->string.empty()) {
        error = where + " ('" + entry.key + "'): missing or empty 'provenance'";
        return false;
    }
    entry.provenance = provenance->string;

    const json::Value* notes = value.Find("notes");
    if (notes != nullptr && !notes->IsString()) {
        error = where + " ('" + entry.key + "'): 'notes' must be a string";
        return false;
    }
    if (notes != nullptr) {
        entry.notes = notes->string;
    }

    const json::Value* source = value.Find("source");
    if (source != nullptr && !source->IsString()) {
        error = where + " ('" + entry.key + "'): 'source' must be a string";
        return false;
    }
    if (source != nullptr) {
        entry.source = source->string;
    }

    const json::Value* entryPoints = value.Find("entry");
    if (entryPoints != nullptr) {
        if (!entryPoints->IsObject()) {
            error = where + " ('" + entry.key + "'): 'entry' must be an object";
            return false;
        }
        for (const std::pair<std::string, json::Value>& member : entryPoints->object) {
            const bool declared =
                (member.first == "vs" && entry.hasVs) || (member.first == "ps" && entry.hasPs);
            if (!declared) {
                error = where + " ('" + entry.key + "'): 'entry' names stage '" + member.first +
                        "' that 'stages' does not declare";
                return false;
            }
            if (!member.second.IsString() || member.second.string.empty()) {
                error = where + " ('" + entry.key + "'): entry point for '" + member.first +
                        "' must be a non-empty string";
                return false;
            }
            if (member.first == "vs") {
                entry.entryVs = member.second.string;
            } else {
                entry.entryPs = member.second.string;
            }
        }
    }

    if (entry.status == Status::OK) {
        if (entry.source.empty()) {
            error = where + " ('" + entry.key + "'): owned entry requires 'source'";
            return false;
        }
        if (entryPoints == nullptr) {
            error = where + " ('" + entry.key + "'): owned entry requires 'entry'";
            return false;
        }
        if (entry.hasVs && entry.entryVs.empty()) {
            error = where + " ('" + entry.key + "'): owned entry requires an 'entry' name for stage 'vs'";
            return false;
        }
        if (entry.hasPs && entry.entryPs.empty()) {
            error = where + " ('" + entry.key + "'): owned entry requires an 'entry' name for stage 'ps'";
            return false;
        }
    } else {
        if (entry.notes.empty()) {
            error = where + " ('" + entry.key + "'): non-owned entry requires 'notes'";
            return false;
        }
        // resolver.md 4.1 requires `source` only for owned entries and never
        // forbids it elsewhere. The shipping manifest deliberately keeps an
        // "owned-shaped" source on stubs (the authored file exists but the
        // ownership criteria are not met), so source is allowed on stubs. It
        // is still rejected on entries that must never carry implementation
        // provenance.
        if (source != nullptr && (entry.status == Status::Unsupported || entry.status == Status::Absent)) {
            error = where + " ('" + entry.key +
                    "'): an " + (entry.status == Status::Absent ? "absent" : "unsupported") +
                    " entry must not declare 'source'";
            return false;
        }
    }

    const json::Value* arity = value.Find("arity");
    if (arity != nullptr) {
        if (!arity->IsObject()) {
            error = where + " ('" + entry.key + "'): 'arity' must be an object";
            return false;
        }
        const json::Value* vertexParms = arity->Find("vertexParms");
        if (vertexParms != nullptr) {
            if (!ParseArityValue(*vertexParms, entry.vertexParms)) {
                error = where + " ('" + entry.key + "'): 'arity.vertexParms' must be an integer";
                return false;
            }
        }
        const json::Value* fragmentMaps = arity->Find("fragmentMaps");
        if (fragmentMaps != nullptr) {
            if (!ParseArityValue(*fragmentMaps, entry.fragmentMaps)) {
                error = where + " ('" + entry.key + "'): 'arity.fragmentMaps' must be an integer";
                return false;
            }
        }
    }

    return true;
}

} // namespace

const char* ToString(Status status) {
    switch (status) {
    case Status::OK:
        return "OK";
    case Status::Unresolved:
        return "UNRESOLVED";
    case Status::StageMissing:
        return "STAGE_MISSING";
    case Status::Stub:
        return "STUB";
    case Status::Unsupported:
        return "UNSUPPORTED";
    case Status::Absent:
        return "ABSENT";
    case Status::Ambiguous:
        return "AMBIGUOUS";
    case Status::NameTooLong:
        return "NAME_TOO_LONG";
    case Status::ManifestInvalid:
        return "MANIFEST_INVALID";
    case Status::CompileFailed:
        return "COMPILE_FAILED";
    case Status::NotCompiled:
        return "NOT_COMPILED";
    }
    return "MANIFEST_INVALID";
}

const char* ToString(Target target) {
    return target == Target::Vertex ? "vs" : "ps";
}

const char* ToString(Mode mode) {
    switch (mode) {
    case Mode::Off:
        return "off";
    case Mode::Tolerant:
        return "tolerant";
    case Mode::Strict:
        return "strict";
    case Mode::Fatal:
        return "fatal";
    }
    return "strict";
}

const char* ToString(Decision decision) {
    switch (decision) {
    case Decision::Draw:
        return "draw";
    case Decision::RejectMaterial:
        return "reject-material";
    case Decision::ErrorMaterial:
        return "error-material";
    case Decision::Fatal:
        return "fatal";
    }
    return "reject-material";
}

Manifest::Manifest(const Manifest& other) : entries_(other.entries_), index_() {
    RebuildIndex();
}

Manifest& Manifest::operator=(const Manifest& other) {
    if (this != &other) {
        entries_ = other.entries_;
        RebuildIndex();
    }
    return *this;
}

Manifest::Manifest(Manifest&& other) noexcept : entries_(std::move(other.entries_)), index_() {
    RebuildIndex();
    other.index_.clear();
}

Manifest& Manifest::operator=(Manifest&& other) noexcept {
    if (this != &other) {
        entries_ = std::move(other.entries_);
        RebuildIndex();
        other.index_.clear();
    }
    return *this;
}

bool Manifest::Parse(const std::string& text, Manifest& out, std::string& error) {
    error.clear();
    out = Manifest();

    json::Value root;
    if (!json::Parse(text, root, error)) {
        return false;
    }
    if (!root.IsObject()) {
        error = "manifest: top-level value must be an object";
        return false;
    }

    const json::Value* schema = root.Find("schema");
    if (schema == nullptr) {
        error = "manifest: missing 'schema'";
        return false;
    }
    if (!schema->IsString()) {
        error = "manifest: 'schema' must be a string";
        return false;
    }
    if (schema->string != kManifestSchema) {
        error = "manifest: unknown schema '" + schema->string + "' (expected '" +
                std::string(kManifestSchema) + "')";
        return false;
    }

    const json::Value* programs = root.Find("programs");
    if (programs == nullptr) {
        error = "manifest: missing 'programs'";
        return false;
    }
    if (!programs->IsArray()) {
        error = "manifest: 'programs' must be an array";
        return false;
    }

    Manifest parsed;
    parsed.entries_.reserve(programs->array.size());
    for (std::size_t i = 0; i < programs->array.size(); ++i) {
        ManifestEntry entry;
        if (!ParseEntry(programs->array[i], i, entry, error)) {
            error = "manifest: " + error;
            return false;
        }
        parsed.entries_.push_back(std::move(entry));
    }
    parsed.RebuildIndex();
    out = std::move(parsed);
    return true;
}

std::size_t Manifest::CountOf(Status status) const {
    std::size_t count = 0;
    for (const ManifestEntry& entry : entries_) {
        if (entry.status == status) {
            ++count;
        }
    }
    return count;
}

const std::vector<const ManifestEntry*>* Manifest::Find(const std::string& key) const {
    const auto it = index_.find(key);
    if (it == index_.end()) {
        return nullptr;
    }
    return &it->second;
}

void Manifest::RebuildIndex() {
    index_.clear();
    for (const ManifestEntry& entry : entries_) {
        index_[entry.key].push_back(&entry);
    }
}

Resolver::Resolver(const Manifest& manifest) : manifest_(manifest) {}

void Resolver::SetMode(Mode mode) {
    mode_ = mode;
}

Mode Resolver::GetMode() const {
    return mode_;
}

void Resolver::SetMaterial(const std::string& name) {
    material_ = name;
}

std::string Resolver::NormalizeName(const std::string& name) {
    const std::size_t lastDot = name.rfind('.');
    std::string key = (lastDot == std::string::npos) ? name : name.substr(0, lastDot);
    for (char& c : key) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return key;
}

Decision Resolver::Decide(Status code) const {
    if (code == Status::OK) {
        return Decision::Draw;
    }
    if (code == Status::ManifestInvalid) {
        return Decision::Fatal;
    }
    switch (mode_) {
    case Mode::Off:
        return Decision::Draw;
    case Mode::Tolerant:
        return Decision::ErrorMaterial;
    case Mode::Fatal:
        return Decision::Fatal;
    case Mode::Strict:
        break;
    }
    return Decision::RejectMaterial;
}

Record Resolver::Evaluate(const Request& request, int stageIndex, const std::string& material) const {
    Record record;
    record.material = material;
    record.stageIndex = stageIndex;
    record.keyword = request.keyword;
    record.program = request.name;
    record.target = request.target;
    record.mode = mode_;
    record.key = NormalizeName(request.name);

    if (request.name.size() > kMaxProgramNameBytes) {
        record.code = Status::NameTooLong;
        record.detail = "program name is " + std::to_string(request.name.size()) +
                        " bytes; the engine limit is " + std::to_string(kMaxProgramNameBytes);
    } else {
        const std::vector<const ManifestEntry*>* entries = manifest_.Find(record.key);
        if (entries == nullptr || entries->empty()) {
            record.code = Status::Unresolved;
            record.detail = "no manifest entry for '" + record.key + "'";
        } else if (entries->size() > 1) {
            record.code = Status::Ambiguous;
            record.detail = std::to_string(entries->size()) + " manifest entries for '" +
                            record.key + "'; duplicate keys are ambiguous";
        } else {
            const ManifestEntry& entry = *entries->front();
            const bool stageDeclared =
                (request.target == Target::Vertex) ? entry.hasVs : entry.hasPs;
            if (!stageDeclared) {
                record.code = Status::StageMissing;
                record.detail = "manifest entry '" + record.key + "' does not declare stage '" +
                                ToString(request.target) + "'";
            } else {
                const auto withNotes = [&record](const char* state) {
                    std::string detail =
                        std::string("manifest entry '") + record.key + "' is " + state;
                    return detail;
                };
                switch (entry.status) {
                case Status::OK:
                    record.code = Status::OK;
                    record.detail = "owned program '" + record.key + "'";
                    break;
                case Status::Stub:
                    record.code = Status::Stub;
                    record.detail = withNotes("a stub");
                    break;
                case Status::Unsupported:
                    record.code = Status::Unsupported;
                    record.detail = withNotes("unsupported");
                    break;
                case Status::Absent:
                    record.code = Status::Absent;
                    record.detail = withNotes("absent");
                    break;
                default:
                    record.code = Status::ManifestInvalid;
                    record.detail = "manifest entry '" + record.key + "' has an invalid status";
                    break;
                }
                if (record.code != Status::OK && !entry.notes.empty()) {
                    record.detail += ": " + entry.notes;
                }
            }
        }
    }

    record.decision = Decide(record.code);
    record.draw = (record.decision == Decision::Draw);
    switch (record.decision) {
    case Decision::Draw:
        record.action = (record.code == Status::OK)
                            ? "stage installed; draw proceeds"
                            : "mode off: draw proceeds with a non-OK program; not evidence";
        break;
    case Decision::RejectMaterial:
        record.action = "material rejected; stage not drawn";
        break;
    case Decision::ErrorMaterial:
        record.action = "error material substituted; stage not drawn";
        break;
    case Decision::Fatal:
        record.action = "fatal: no draw; process stops";
        break;
    }
    return record;
}

void Resolver::RecordResolution(const Record& record) {
    ++requests_;
    phase1_[PhaseKey(record.key, record.target)] = record.code;

    switch (record.code) {
    case Status::OK:
        ++okCount_;
        return;
    case Status::Unresolved:
        ++unresolvedCount_;
        break;
    case Status::StageMissing:
        ++stageMissingCount_;
        break;
    case Status::Ambiguous:
        ++ambiguousCount_;
        break;
    case Status::NameTooLong:
        ++nameTooLongCount_;
        break;
    case Status::Stub:
        ++stubCount_;
        break;
    case Status::Unsupported:
        ++unsupportedCount_;
        break;
    case Status::Absent:
        ++absentCount_;
        break;
    case Status::ManifestInvalid:
        break;
    case Status::CompileFailed:
        ++compileFailedCount_;
        break;
    case Status::NotCompiled:
        ++notCompiledCount_;
        break;
    }

    ++nonOkCount_;
    if (record.decision == Decision::RejectMaterial) {
        rejectedMaterials_.insert(record.material);
    } else if (record.decision == Decision::ErrorMaterial) {
        errorMaterials_.insert(record.material);
    }

    const auto identity =
        std::make_tuple(record.material, record.stageIndex, record.program,
                        static_cast<int>(record.code), static_cast<int>(record.target));
    if (!seen_.insert(identity).second) {
        ++suppressedRepeats_;
        return;
    }
    records_.push_back(record);
}

Record Resolver::Resolve(const Request& request, int stageIndex) {
    const Record record = Evaluate(request, stageIndex, material_);
    RecordResolution(record);

    LoggedRequest logged;
    logged.pair = false;
    logged.request = request;
    logged.stageIndex = stageIndex;
    logged.material = material_;
    log_.push_back(logged);

    return record;
}

bool Resolver::ResolvePairInternal(const std::string& name, int stageIndex,
                                   const std::string& material, Record& outVs, Record& outPs) {
    Request vsRequest;
    vsRequest.keyword = "program";
    vsRequest.name = name;
    vsRequest.target = Target::Vertex;
    Request psRequest = vsRequest;
    psRequest.target = Target::Fragment;

    outVs = Evaluate(vsRequest, stageIndex, material);
    outPs = Evaluate(psRequest, stageIndex, material);

    const bool bothOk = (outVs.code == Status::OK && outPs.code == Status::OK);
    if (!bothOk) {
        const Status failure = (outVs.code != Status::OK) ? outVs.code : outPs.code;
        const char* failureStage = (outVs.code != Status::OK) ? "vs" : "ps";
        const std::string detail = std::string("atomic 'program' pair: stage '") + failureStage +
                                   "' failed (" + ToString(failure) +
                                   "); this stage is not installed";
        if (outVs.code == Status::OK) {
            outVs.code = failure;
            outVs.detail = detail;
            outVs.decision = Decide(outVs.code);
            outVs.draw = (outVs.decision == Decision::Draw);
        }
        if (outPs.code == Status::OK) {
            outPs.code = failure;
            outPs.detail = detail;
            outPs.decision = Decide(outPs.code);
            outPs.draw = (outPs.decision == Decision::Draw);
        }
    }

    RecordResolution(outVs);
    RecordResolution(outPs);
    return bothOk;
}

bool Resolver::ResolveProgramPair(const std::string& name, int stageIndex, Record& outVs,
                                  Record& outPs) {
    const std::string material = material_;
    const bool ok = ResolvePairInternal(name, stageIndex, material, outVs, outPs);

    LoggedRequest logged;
    logged.pair = true;
    logged.request.keyword = "program";
    logged.request.name = name;
    logged.request.target = Target::Vertex;
    logged.stageIndex = stageIndex;
    logged.material = material;
    log_.push_back(logged);

    return ok;
}

std::pair<std::string, int> Resolver::PhaseKey(const std::string& key, Target target) {
    return std::make_pair(key, static_cast<int>(target));
}

Status Resolver::BlobStatus(const std::string& key, Target target) const {
    const auto it = blobs_.find(PhaseKey(key, target));
    if (it == blobs_.end()) {
        return Status::NotCompiled;
    }
    return it->second;
}

StageGate Resolver::PairGate(const Record& vs, const Record& ps) const {
    StageGate gate;
    gate.key = !vs.key.empty() ? vs.key : ps.key;
    if (vs.code == Status::OK && ps.code == Status::OK) {
        gate.phase1 = Status::OK;
    } else {
        gate.phase1 = (vs.code != Status::OK) ? vs.code : ps.code;
    }
    const Status vsBlob = BlobStatus(vs.key, vs.target);
    const Status psBlob = BlobStatus(ps.key, ps.target);
    if (vsBlob == Status::OK && psBlob == Status::OK) {
        gate.phase2 = Status::OK;
    } else {
        gate.phase2 = (vsBlob != Status::OK) ? vsBlob : psBlob;
    }
    gate.ok = (gate.phase1 == Status::OK && gate.phase2 == Status::OK);
    return gate;
}

void Resolver::BeginBlobPhase() {
    blobs_.clear();
    blobPhaseActive_ = true;
}

void Resolver::SetBlob(Status phase2, const std::string& key, Target target) {
    if (phase2 != Status::OK && phase2 != Status::CompileFailed && phase2 != Status::NotCompiled) {
        return;
    }
    blobs_[PhaseKey(key, target)] = phase2;
    blobPhaseActive_ = true;
}

bool Resolver::MayDraw(const std::string& key, Target target) const {
    if (!blobPhaseActive_) {
        return false;
    }
    const auto phase1 = phase1_.find(PhaseKey(key, target));
    if (phase1 == phase1_.end() || phase1->second != Status::OK) {
        return false;
    }
    const auto blob = blobs_.find(PhaseKey(key, target));
    return blob != blobs_.end() && blob->second == Status::OK;
}

Record Resolver::GateForDraw(const std::string& key, Target target) const {
    Record record;
    record.material = material_;
    record.keyword = "draw";
    record.program = key;
    record.key = key;
    record.target = target;
    record.mode = mode_;

    const auto phase1 = phase1_.find(PhaseKey(key, target));
    if (phase1 == phase1_.end()) {
        record.code = Status::Unresolved;
        record.detail = "no phase-1 resolution recorded for '" + key + "'";
    } else if (phase1->second != Status::OK) {
        record.code = phase1->second;
        record.detail = "phase 1 failed for '" + key + "'";
        for (const Record& stored : records_) {
            if (stored.key == key && stored.target == target && stored.code == record.code) {
                return stored;
            }
        }
    } else {
        const Status blob = BlobStatus(key, target);
        if (blob != Status::OK) {
            record.code = blob;
            record.detail = (blob == Status::NotCompiled)
                                ? "no blob bound for the active backend"
                                : "blob failed validation";
        } else {
            record.code = Status::OK;
            record.detail = "phase 1 and phase 2 are OK";
            record.action = "draw proceeds";
            record.decision = Decision::Draw;
            record.draw = true;
            return record;
        }
    }

    record.decision = Decide(record.code);
    record.draw = false;
    switch (record.decision) {
    case Decision::Draw:
        record.action = "mode off: draw proceeds with a non-OK program; not evidence";
        break;
    case Decision::RejectMaterial:
        record.action = "material rejected; stage not drawn";
        break;
    case Decision::ErrorMaterial:
        record.action = "error material substituted; stage not drawn";
        break;
    case Decision::Fatal:
        record.action = "fatal: no draw; process stops";
        break;
    }
    return record;
}

void Resolver::ResetCounters() {
    requests_ = 0;
    okCount_ = 0;
    unresolvedCount_ = 0;
    stageMissingCount_ = 0;
    ambiguousCount_ = 0;
    nameTooLongCount_ = 0;
    stubCount_ = 0;
    unsupportedCount_ = 0;
    absentCount_ = 0;
    compileFailedCount_ = 0;
    notCompiledCount_ = 0;
    nonOkCount_ = 0;
    suppressedRepeats_ = 0;
    rejectedMaterials_.clear();
    errorMaterials_.clear();
    records_.clear();
    seen_.clear();
    phase1_.clear();
    blobs_.clear();
    blobPhaseActive_ = false;
    lastReloadReport_.clear();
}

void Resolver::Reload() {
    const std::string before = StatusLine();
    const std::size_t beforeNonOk = nonOkCount_;
    const std::vector<LoggedRequest> replay = log_;

    ResetCounters();

    for (const LoggedRequest& item : replay) {
        if (item.pair) {
            Record vs;
            Record ps;
            ResolvePairInternal(item.request.name, item.stageIndex, item.material, vs, ps);
        } else {
            const Record record = Evaluate(item.request, item.stageIndex, item.material);
            RecordResolution(record);
        }
    }

    const std::string after = StatusLine();
    std::string report = before;
    report += "\n";
    report += after;
    report += "\nshader_reload: non_ok before=" + std::to_string(beforeNonOk) +
              " after=" + std::to_string(nonOkCount_);
    report += (beforeNonOk == nonOkCount_) ? " (unchanged)" : " (changed)";
    lastReloadReport_ = report;
}

std::string Resolver::StatusLine() const {
    std::ostringstream line;
    line << "shader_status: manifest=" << manifest_.Size()
         << " owned=" << manifest_.CountOf(Status::OK)
         << " stub=" << manifest_.CountOf(Status::Stub)
         << " unsupported=" << manifest_.CountOf(Status::Unsupported)
         << " absent=" << manifest_.CountOf(Status::Absent)
         << " requests=" << requests_
         << " ok=" << okCount_
         << " unresolved=" << unresolvedCount_
         << " stage_missing=" << stageMissingCount_
         << " ambiguous=" << ambiguousCount_
         << " materials_rejected=" << rejectedMaterials_.size()
         << " materials_error=" << errorMaterials_.size()
         << " mode=" << ToString(mode_);
    return line.str();
}

std::string Resolver::Describe(const Record& record) const {
    std::ostringstream out;
    out << "SHADER ERROR: material=\"" << record.material << "\" stage=" << record.stageIndex
        << " keyword=" << record.keyword << " target=" << ToString(record.target) << "\n"
        << "  program=\"" << record.program << "\" key=\"" << record.key
        << "\" code=" << ToString(record.code) << " mode=" << ToString(record.mode) << "\n"
        << "  detail=\"" << record.detail << "\"\n"
        << "  action=\"" << record.action << "\"";
    return out.str();
}

} // namespace d3::shader
