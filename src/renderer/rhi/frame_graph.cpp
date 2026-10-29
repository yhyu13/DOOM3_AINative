#include "rhi/frame_graph.h"

#include <algorithm>
#include <map>
#include <set>

namespace doom3 {
namespace rhi {

namespace {

Schedule Reject(const std::string& message) {
    Schedule schedule;
    schedule.status = Status::InvalidArgument;
    schedule.error = message;
    return schedule;
}

}  // namespace

const char* ToString(Access value) {
    switch (value) {
    case Access::Read:
        return "read";
    case Access::Write:
        return "write";
    }
    return "unknown";
}

Schedule SchedulePasses(const std::vector<GraphResource>& resources,
                        const std::vector<PassDeclaration>& passes) {
    std::map<uint32_t, bool> imported;
    for (const GraphResource& resource : resources) {
        if (imported.find(resource.id) != imported.end()) {
            return Reject("resource id " + std::to_string(resource.id) +
                          " is declared more than once");
        }
        imported.emplace(resource.id, resource.imported);
    }

    std::set<uint32_t> written;
    std::set<uint32_t> read;
    Schedule schedule;
    schedule.passOrder.reserve(passes.size());
    for (uint32_t passIndex = 0; passIndex < passes.size(); ++passIndex) {
        const PassDeclaration& pass = passes[passIndex];
        if (pass.name.empty()) {
            return Reject("pass " + std::to_string(passIndex) + " has no name");
        }
        std::set<uint32_t> declaredHere;
        for (const PassUse& use : pass.uses) {
            const auto resource = imported.find(use.resource);
            if (resource == imported.end()) {
                return Reject("pass '" + pass.name + "' names undeclared resource " +
                              std::to_string(use.resource));
            }
            if (!declaredHere.insert(use.resource).second) {
                return Reject("pass '" + pass.name + "' declares resource " +
                              std::to_string(use.resource) + " more than once");
            }
            if (use.access == Access::Read) {
                if (written.find(use.resource) == written.end() && !resource->second) {
                    return Reject("pass '" + pass.name + "' reads resource " +
                                  std::to_string(use.resource) +
                                  " before any declared write and it is not imported");
                }
                if (written.find(use.resource) != written.end()) {
                    ++schedule.readAfterWriteCount;
                }
                read.insert(use.resource);
            } else {
                written.insert(use.resource);
            }
        }
        schedule.passOrder.push_back(passIndex);
    }
    schedule.status = Status::Ok;
    return schedule;
}

}  // namespace rhi
}  // namespace doom3
