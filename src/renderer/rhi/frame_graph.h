#pragma once

// Portable frame graph: declared resource accesses and the pass order derived
// from them. It contains no native state operation and performs no aliasing.
//
// Every pass declares which resources it reads and writes. A transient
// resource must be written by an earlier pass before any pass reads it; an
// imported resource may be read first. The schedule keeps the declared pass
// order and rejects an unsatisfied access instead of guessing an order.

#include "rhi/rhi.h"

#include <cstdint>
#include <string>
#include <vector>

namespace doom3 {
namespace rhi {

enum class Access {
    Read,
    Write,
};

struct GraphResource {
    uint32_t id = 0;
    // True when the resource already holds defined contents at frame start.
    bool imported = false;
};

struct PassUse {
    uint32_t resource = 0;
    Access access = Access::Read;
};

struct PassDeclaration {
    std::string name;
    std::vector<PassUse> uses;
};

struct Schedule {
    Status status = Status::Ok;
    std::string error;
    // Indices into the submitted pass declaration list, in execution order.
    std::vector<uint32_t> passOrder;
    // Number of declared uses that required no change of ownership.
    uint32_t readAfterWriteCount = 0;
};

// Pure, deterministic validation. It does not allocate device resources and
// does not infer an order other than the declared one.
Schedule SchedulePasses(const std::vector<GraphResource>& resources,
                        const std::vector<PassDeclaration>& passes);

const char* ToString(Access value);

}  // namespace rhi
}  // namespace doom3
