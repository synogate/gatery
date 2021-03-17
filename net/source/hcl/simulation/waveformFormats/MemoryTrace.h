#pragma once

#include "../BitVectorState.h"

#include "../../hlim/NodePort.h"
#include "../../hlim/ClockRational.h"

namespace hcl::core::hlim {

class Clock;

}

namespace hcl::core::sim {

struct MemoryTrace {
    struct Signal {
        hlim::NodePort driver;
        const hlim::Clock *clock = nullptr;
        std::string name;
        size_t width;
        bool isBool;
    };

    struct SignalChange {
        size_t sigIdx;
        size_t dataOffset;
    };

    struct Event {
        hlim::ClockRational timestamp;
        std::vector<SignalChange> changes;
    };

    DefaultBitVectorState data;
    std::vector<Signal> signals;
    std::vector<Event> events;
};

}
