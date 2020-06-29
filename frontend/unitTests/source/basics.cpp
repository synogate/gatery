#include <boost/test/unit_test.hpp>

#include <hcl/frontend.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>

#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>


BOOST_FIXTURE_TEST_CASE(SimpleAdditionNetwork, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    UnsignedInteger a = 0b0010_uvec;
    sim_debug() << "Signal a is " << a;
    
    UnsignedInteger b = 0b0110_uvec;
    sim_debug() << "Signal b is " << b;
    
    UnsignedInteger c = a + b;
    sim_debug() << "Signal c (= a + b) is " << c;
    
    sim_assert(c == 0b1000_uvec) << "The signal c should be 0b1000_uvec but is " << c;
    
    eval(design.getCircuit());    
}


class CounterRefImpl : public hcl::core::hlim::Node_SignalGenerator
{
    public:
        CounterRefImpl(hcl::core::hlim::BaseClock *clk) : hcl::core::hlim::Node_SignalGenerator(clk) {
            setOutputs({{ .interpretation = hcl::core::hlim::ConnectionType::UNSIGNED, .width = 8}});
        }
        
        virtual std::string getOutputName(size_t idx) const override { return "counter"; }
    protected:
        virtual void produceSignals(hcl::core::sim::DefaultBitVectorState &state, const size_t *outputOffsets, size_t clockTick) const override {
            state.insertNonStraddling(hcl::core::sim::DefaultConfig::DEFINED, outputOffsets[0], 8, 0xFF);
            state.insertNonStraddling(hcl::core::sim::DefaultConfig::VALUE, outputOffsets[0], 8, clockTick);
        }
};

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


    auto sigGen = design.createNode<CounterRefImpl>(clk);
    UnsignedInteger refCount({.node = sigGen, .port = 0ull});
    
    sim_assert(counter == refCount) << "The counter should be " << refCount << " but is " << counter;
    
    runTicks(design.getCircuit(), clk, 10);
}
