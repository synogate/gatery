#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/frontend.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>

#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>

using namespace boost::unit_test;

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, TestOperators, data::xrange(8) * data::xrange(8) * data::xrange(1, 8), x, y, bitsize)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    
    UnsignedInteger a = ConstUnsignedInteger(x, bitsize);
    UnsignedInteger b = ConstUnsignedInteger(y, bitsize);

#define op2str(op) #op
#define buildOperatorTest(op)                                                                               \
    {                                                                                                       \
        UnsignedInteger c = a op b;                                                                         \
        UnsignedInteger groundTruth = ConstUnsignedInteger(std::uint64_t(x) op std::uint64_t(y), bitsize);  \
        sim_assert(c == groundTruth) << "The result of " << a << " " << op2str(op) << " " << b              \
            << " should be " << groundTruth << " (with overflow in " << bitsize << "bits) but is " << c;    \
    }
    
    buildOperatorTest(+)
    buildOperatorTest(-)
    buildOperatorTest(*)
    //buildOperatorTest(/)
    
    buildOperatorTest(&)
    buildOperatorTest(|)
    buildOperatorTest(^)
    
#undef op2str
#undef buildOperatorTest

    
#define op2str(op) #op
#define buildOperatorTest(op)                                                                               \
    {                                                                                                       \
        UnsignedInteger c = a;                                                                              \
        c op b;                                                                                             \
        std::uint64_t gt = x;                                                                               \
        gt op std::uint64_t(y);                                                                             \
        UnsignedInteger groundTruth = ConstUnsignedInteger(gt, bitsize);                                    \
        sim_assert(c == groundTruth) << "The result of (" << c << "="<<a << ") " << op2str(op) << " " << b  \
            << " should be " << groundTruth << " (with overflow in " << bitsize << "bits) but is " << c;    \
    }
    
    buildOperatorTest(+=)
    buildOperatorTest(-=)
    buildOperatorTest(*=)
    //buildOperatorTest(/=)
    
    buildOperatorTest(&=)
    buildOperatorTest(|=)
    buildOperatorTest(^=)
    
#undef op2str
#undef buildOperatorTest
    
    
    eval(design.getCircuit());
}



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


BOOST_FIXTURE_TEST_CASE(SimpleCounterNewSyntax, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;
    
    auto clk = design.createClock<hcl::core::hlim::RootClock>("clk", hcl::core::hlim::ClockRational(10'000));
    RegisterConfig regConf{.clk = clk, .resetName = "rst"};
    
    {
        Register<UnsignedInteger> counter(regConf, 0x00_uvec);
        counter = counter.delay(1) + 1_uvec;
        sim_debug() << "Counter value is " << counter.delay(1) << " and next counter value is " << counter;

        UnsignedInteger refCount(8);
        simpleSignalGenerator(clk, [](SimpleSignalGeneratorContext &context){
            context.set(0, context.getTick());
        }, refCount);
        
        sim_assert(counter.delay(1) == refCount) << "The counter should be " << refCount << " but is " << counter.delay(1);    
    }
    
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

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, ConditionalAssignmentMultipleStatements, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    UnsignedInteger a = ConstUnsignedInteger(x, 8);
    UnsignedInteger b = ConstUnsignedInteger(y, 8);

    UnsignedInteger c;
    IF (a[1] == 1_bit) {
        c = a + b;
        c += a;
        c += b;
    } ELSE {
        c = a - b;
    }
    
    unsigned groundTruth;
    if (unsigned(x) & 2) {
        groundTruth = unsigned(x)+unsigned(y);
        groundTruth += x;
        groundTruth += y;
    } else {
        groundTruth = unsigned(x)-unsigned(y);        
    }

    sim_assert(c == ConstUnsignedInteger(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;
    
    eval(design.getCircuit());
}

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, ConditionalAssignmentMultipleElseStatements, data::xrange(8) * data::xrange(8), x, y)
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
        c = c - b;
        c = c - b;
    }
    
    unsigned groundTruth;
    if (unsigned(x) & 2) 
        groundTruth = unsigned(x)+unsigned(y);
    else {
        groundTruth = unsigned(x)-unsigned(y);        
        groundTruth = groundTruth-unsigned(y);        
        groundTruth = groundTruth-unsigned(y);        
    }

    sim_assert(c == ConstUnsignedInteger(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;
    
    eval(design.getCircuit());
}



BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, MultiLevelConditionalAssignment, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    UnsignedInteger a = ConstUnsignedInteger(x, 8);
    UnsignedInteger b = ConstUnsignedInteger(y, 8);

    UnsignedInteger c;
    IF (a[2] == 1_bit) {
        IF (a[1] == 1_bit)
            c = a + b;
        ELSE {
            c = a - b;
        }
    } ELSE {
        IF (a[1] == 1_bit)
            c = a;
        ELSE {
            c = b;
        }
    }
    
    unsigned groundTruth;
    if (unsigned(x) & 4) {
        if (unsigned(x) & 2) 
            groundTruth = x+y;
        else
            groundTruth = x-y;
    } else {
        if (unsigned(x) & 2) 
            groundTruth = x;
        else
            groundTruth = y;
    }

    sim_assert(c == ConstUnsignedInteger(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;
    
    eval(design.getCircuit());
}


BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, MultiLevelConditionalAssignmentMultipleStatements, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    UnsignedInteger a = ConstUnsignedInteger(x, 8);
    UnsignedInteger b = ConstUnsignedInteger(y, 8);

    UnsignedInteger c;
    IF (a[2] == 1_bit) {
        IF (a[1] == 1_bit) {
            c = a + b;
            c += b;
            c += a;
        } ELSE {
            c = a - b;
        }
    } ELSE {
        IF (a[1] == 1_bit)
            c = a;
        ELSE {
            c = b;
        }
    }
    
    unsigned groundTruth;
    if (unsigned(x) & 4) {
        if (unsigned(x) & 2) {
            groundTruth = x+y;
            groundTruth += y;
            groundTruth += x;
        } else
            groundTruth = x-y;
    } else {
        if (unsigned(x) & 2) 
            groundTruth = x;
        else
            groundTruth = y;
    }

    sim_assert(c == ConstUnsignedInteger(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;
    
    eval(design.getCircuit());
}

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, MultiLevelConditionalAssignmentWithPreviousAssignmentNoElse, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    UnsignedInteger a = ConstUnsignedInteger(x, 8);
    UnsignedInteger b = ConstUnsignedInteger(y, 8);

    UnsignedInteger c = a;
    IF (a[2] == 1_bit) {
        IF (a[1] == 1_bit)
            c = a + b;
        ELSE {
            c = a - b;
        }
    } 
    
    unsigned groundTruth = x;
    if (unsigned(x) & 4) {
        if (unsigned(x) & 2) 
            groundTruth = x+y;
        else
            groundTruth = x-y;
    } 
    
    sim_assert(c == ConstUnsignedInteger(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;
    
    eval(design.getCircuit());
}



BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, MultiLevelConditionalAssignmentWithPreviousAssignmentNoIf, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    UnsignedInteger a = ConstUnsignedInteger(x, 8);
    UnsignedInteger b = ConstUnsignedInteger(y, 8);

    UnsignedInteger c = a;
    IF (a[2] == 1_bit) {
    } ELSE {
        IF (a[1] == 1_bit)
            c = b;
    }
    
    unsigned groundTruth = x;
    if (unsigned(x) & 4) {
    } else {
        if (unsigned(x) & 2) 
            groundTruth = y;
    }

    sim_assert(c == ConstUnsignedInteger(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;
    
    eval(design.getCircuit());
}


BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, MultiLevelConditionalAssignmentWithPreviousAssignment, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    UnsignedInteger a = ConstUnsignedInteger(x, 8);
    UnsignedInteger b = ConstUnsignedInteger(y, 8);

    UnsignedInteger c = a;
    IF (a[2] == 1_bit) {
        IF (a[1] == 1_bit)
            c = a + b;
        ELSE
            c = a - b;
    } ELSE {
        IF (a[1] == 1_bit)
            c = b;
    }
    
    unsigned groundTruth = x;
    if (unsigned(x) & 4) {
        if (unsigned(x) & 2) 
            groundTruth = x+y;
        else
            groundTruth = x-y;
    } else {
        if (unsigned(x) & 2) 
            groundTruth = y;
    }

    sim_assert(c == ConstUnsignedInteger(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;
    
    eval(design.getCircuit());
}

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, MultiLevelConditionalAssignmentIfElseIf, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    UnsignedInteger a = ConstUnsignedInteger(x, 8);
    UnsignedInteger b = ConstUnsignedInteger(y, 8);

    UnsignedInteger c = a;
    IF (a[2] == 1_bit) {
        c = a + b;
    } ELSE {
        IF (a[1] == 1_bit)
            c = b;
    }
    
    unsigned groundTruth = x;
    if (unsigned(x) & 4) {
        groundTruth = x+y;
    } else {
        if (unsigned(x) & 2) 
            groundTruth = y;
    }

    sim_assert(c == ConstUnsignedInteger(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;
    
    eval(design.getCircuit());
}
