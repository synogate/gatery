#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/frontend.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>

#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>

using namespace boost::unit_test;
using UnitTestSimulationFixture = hcl::core::frontend::UnitTestSimulationFixture;
/*
BOOST_FIXTURE_TEST_CASE(ROM, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;
    using namespace hcl::core::sim;
    using namespace hcl::utils;



    Memory<BVec> rom(16, 4_b);
    rom.setPowerOnState(createDefaultBitVectorState(16, 4, [](std::size_t i, std::size_t *words){
        words[DefaultConfig::VALUE] = i;
        words[DefaultConfig::DEFINED] = ~0ull;
    }));

    for (auto i : Range(16)) {
        BVec v = rom[i];
        sim_assert(v == i) << "rom["<<i<<"] is " << v << " but should be " << i;
    }

    eval(design.getCircuit());
}
*/