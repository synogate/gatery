#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/frontend.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>

#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>

using namespace boost::unit_test;

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, SimpleAdditionNetwork, data::xrange(8) * data::xrange(8) * data::xrange(1, 8), x, y, bitsize)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    UnsignedInteger a = ConstUnsignedInteger(x, bitsize);
    sim_debug() << "Signal a is " << a;
    
    UnsignedInteger b = ConstUnsignedInteger(y, bitsize);
    sim_debug() << "Signal b is " << b;
    
    UnsignedInteger c = a + b;
    sim_debug() << "Signal c (= a + b) is " << c;
    
    sim_assert(c == ConstUnsignedInteger(x+y, bitsize)) << "The signal c should be " << x+y << " (with overflow in " << bitsize << "bits) but is " << c;
    
    eval(design.getCircuit());
}


BOOST_FIXTURE_TEST_CASE(SimpleCounter, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;
    
    auto clk = design.createClock<hcl::core::hlim::RootClock>("clk", hcl::core::hlim::ClockRational(10'000));
    RegisterFactory reg({.clk = clk, .resetName = "rst"});
    
    UnsignedInteger counter(8);
    UnsignedInteger nextCounter = counter + 0b1_uvec;
    sim_debug() << "Counter value is " << counter << " and next counter value is " << nextCounter;

    driveWith(counter, reg(nextCounter, 1_bit, 0b00000000_uvec));


    UnsignedInteger refCount(8);
    simpleSignalGenerator(clk, [](SimpleSignalGeneratorContext &context){
        context.set(0, context.getTick());
    }, refCount);
    
    sim_assert(counter == refCount) << "The counter should be " << refCount << " but is " << counter;
    
    runTicks(design.getCircuit(), clk, 10);
}



BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, ConditionalAssignment, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    UnsignedInteger a = ConstUnsignedInteger(x, 8);
    UnsignedInteger b = ConstUnsignedInteger(y, 8);

    UnsignedInteger c;
    IF (a[1] == 1_bit)
        c = a + b;
    ELSE {
        c = a - b;
    }
    
    unsigned groundTruth;
    if (unsigned(x) & 2) 
        groundTruth = unsigned(x)+unsigned(y);
    else
        groundTruth = unsigned(x)-unsigned(y);        

    sim_assert(c == ConstUnsignedInteger(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;
    
    eval(design.getCircuit());
}



