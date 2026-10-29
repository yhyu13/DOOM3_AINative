// Portable frame-graph fixture: declared accesses, pass order and invalid-use
// rejection. No device is required.

#include "q1b_test_support.h"

#include "rhi/frame_graph.h"

namespace {

using doom3::rhi::Access;
using doom3::rhi::GraphResource;
using doom3::rhi::PassDeclaration;
using doom3::rhi::PassUse;
using doom3::rhi::Schedule;
using doom3::rhi::SchedulePasses;
using doom3::rhi::Status;

bool Rejected(const Schedule& schedule) {
    return schedule.status == Status::InvalidArgument && !schedule.error.empty();
}

int RunFrameGraph() {
    const std::vector<GraphResource> resources = {
        GraphResource{0, false},
        GraphResource{1, false},
    };
    std::vector<PassDeclaration> passes;
    PassDeclaration first;
    first.name = "depth-write";
    first.uses = {PassUse{0, Access::Write}, PassUse{1, Access::Write}};
    passes.push_back(first);
    PassDeclaration second;
    second.name = "depth-read";
    second.uses = {PassUse{1, Access::Read}};
    passes.push_back(second);
    const Schedule schedule = SchedulePasses(resources, passes);
    if (schedule.status != Status::Ok || schedule.passOrder.size() != 2 ||
        schedule.passOrder[0] != 0 || schedule.passOrder[1] != 1) {
        q1b_test::Require(false, "a valid declared graph did not keep the pass order");
        return 1;
    }
    q1b_test::Log("GRAPH valid order 0,1 reuse=%u", schedule.readAfterWriteCount);

    std::vector<PassDeclaration> readFirst;
    PassDeclaration reader;
    reader.name = "early-read";
    reader.uses = {PassUse{0, Access::Read}};
    readFirst.push_back(reader);
    if (!Rejected(SchedulePasses({GraphResource{0, false}}, readFirst))) {
        q1b_test::Require(false, "read before write was accepted for a transient resource");
        return 1;
    }
    q1b_test::Log("GRAPH read-before-write rejected");

    const Schedule imported = SchedulePasses({GraphResource{0, true}}, readFirst);
    if (imported.status != Status::Ok) {
        q1b_test::Require(false, "an imported resource could not be read first");
        return 1;
    }
    q1b_test::Log("GRAPH imported resource may be read first");

    std::vector<PassDeclaration> unknown = readFirst;
    unknown[0].uses[0].resource = 9;
    if (!Rejected(SchedulePasses({GraphResource{0, false}}, unknown))) {
        q1b_test::Require(false, "an undeclared resource was accepted");
        return 1;
    }
    q1b_test::Log("GRAPH undeclared resource rejected");

    if (!Rejected(SchedulePasses({GraphResource{0, false}, GraphResource{0, true}},
                                 readFirst))) {
        q1b_test::Require(false, "a duplicate resource declaration was accepted");
        return 1;
    }
    q1b_test::Log("GRAPH duplicate resource declaration rejected");

    std::vector<PassDeclaration> duplicated = readFirst;
    duplicated[0].uses.push_back(PassUse{0, Access::Write});
    if (!Rejected(SchedulePasses({GraphResource{0, true}}, duplicated))) {
        q1b_test::Require(false, "a duplicate use in one pass was accepted");
        return 1;
    }
    q1b_test::Log("GRAPH duplicate use in one pass rejected");

    std::vector<PassDeclaration> unnamed = readFirst;
    unnamed[0].name.clear();
    if (!Rejected(SchedulePasses({GraphResource{0, true}}, unnamed))) {
        q1b_test::Require(false, "an unnamed pass was accepted");
        return 1;
    }
    q1b_test::Log("GRAPH unnamed pass rejected");

    std::vector<PassDeclaration> three(3);
    three[0].name = "a";
    three[0].uses = {PassUse{0, Access::Write}};
    three[1].name = "b";
    three[1].uses = {PassUse{0, Access::Read}};
    three[2].name = "c";
    three[2].uses = {PassUse{0, Access::Read}};
    const Schedule ordered = SchedulePasses({GraphResource{0, false}}, three);
    if (ordered.status != Status::Ok || ordered.passOrder.size() != 3 ||
        ordered.passOrder[2] != 2) {
        q1b_test::Require(false, "declared order was not preserved for three passes");
        return 1;
    }
    q1b_test::Log("GRAPH declaration order preserved for three passes");
    return 0;
}

}  // namespace

int main() {
    const int result = RunFrameGraph();
    q1b_test::Log("GRAPH result=%s", result == 0 ? "PASS" : "FAIL");
    return result;
}
