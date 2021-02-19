#pragma once

#include "Scope.h"

#include <hcl/simulation/UnitTestSimulationFixture.h>

#include <memory>
#include <functional>


namespace hcl::core::frontend {

/**
 * @todo write docs
 */
    class UnitTestSimulationFixture : public sim::UnitTestSimulationFixture
    {
    public:
        ~UnitTestSimulationFixture();

        void eval();
        void runTicks(const hlim::Clock* clock, unsigned numTicks);

        DesignScope design;
    protected:
};


}
