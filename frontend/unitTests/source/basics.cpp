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

    
    BVec a = ConstBVec(x, bitsize);
    BVec b = ConstBVec(y, bitsize);

#define op2str(op) #op
#define buildOperatorTest(op)                                                                               \
    {                                                                                                       \
        BVec c = a op b;                                                                         \
        BVec groundTruth = ConstBVec(std::uint64_t(x) op std::uint64_t(y), bitsize);  \
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
        BVec c = a;                                                                              \
        c op b;                                                                                             \
        std::uint64_t gt = x;                                                                               \
        gt op std::uint64_t(y);                                                                             \
        BVec groundTruth = ConstBVec(gt, bitsize);                                    \
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





BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, TestSlicing, data::xrange(8) * data::xrange(3, 8), x, bitsize)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;
    
    BVec a = ConstBVec(x, bitsize);
    
    {
        BVec res = a(0, 1);
        sim_assert(res == ConstBVec(x & 1, 1)) << "Slicing first bit of " << a << " failed: " << res;
    }

    {
        BVec res = a(1, 2);
        sim_assert(res == ConstBVec((x >> 1) & 3, 2)) << "Slicing second and third bit of " << a << " failed: " << res;
    }

    {
        BVec res = a(1, 2);
        res = 0b00_bvec;
        sim_assert(a == ConstBVec(x, bitsize)) << "Modifying copy of slice of a changes a to " << a << ", should be: " << x;
    }
    
    eval(design.getCircuit());
}



BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, TestSlicingModifications, data::xrange(8) * data::xrange(3, 8), x, bitsize)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;
    
    BVec a = ConstBVec(x, bitsize);
    
    {
        BVec b = a;
        b(1, 2) = 0b00_bvec;
        
        auto groundTruth = ConstBVec(unsigned(x) & ~0b110, bitsize);
        sim_assert(b == groundTruth) << "Clearing two bits out of " << a << " should be " << groundTruth << " but is " << b;
    }
    
    eval(design.getCircuit());
}


BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, TestSlicingAddition, data::xrange(8) * data::xrange(3, 8), x, bitsize)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;
    
    BVec a = ConstBVec(x, bitsize);
    
    {
        BVec b = a;
        //b(1, 2) = b(1, 2) + 1_bvec;
        auto slice = b(1, 2);
        b(1, 2) += 1_bvec;
        
        auto groundTruth = ConstBVec((unsigned(x) & ~0b110) | (unsigned(x+2) & 0b110), bitsize);
        sim_assert(b == groundTruth) << "Incrementing two bits out of " << a << " should be " << groundTruth << " but is " << b;
    }
    
    eval(design.getCircuit());
}




BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, SimpleAdditionNetwork, data::xrange(8) * data::xrange(8) * data::xrange(1, 8), x, y, bitsize)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    BVec a = ConstBVec(x, bitsize);
    sim_debug() << "Signal a is " << a;
    
    BVec b = ConstBVec(y, bitsize);
    sim_debug() << "Signal b is " << b;
    
    BVec c = a + b;
    sim_debug() << "Signal c (= a + b) is " << c;
    
    sim_assert(c == ConstBVec(x+y, bitsize)) << "The signal c should be " << x+y << " (with overflow in " << bitsize << "bits) but is " << c;
    
    eval(design.getCircuit());
}

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, BitFromBool, data::xrange(2) * data::xrange(2), l, r)
{
    using namespace hcl::core::frontend;

    DesignScope design;
    Bit a = l != 0;
    Bit b;
    b = r != 0;

    sim_assert((a == b) == Bit{ l == r });
    sim_assert((a != b) == Bit{ l != r });
    sim_assert((a == true) == Bit(l != 0));
    sim_assert((true == a) == Bit(l != 0));
    sim_assert((a != true) == Bit(l == 0));
    sim_assert((true != a) == Bit(l == 0));

    eval(design.getCircuit());
}

BOOST_FIXTURE_TEST_CASE(SimpleCounter, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;
    
    auto clk = design.createClock<hcl::core::hlim::RootClock>("clk", hcl::core::hlim::ClockRational(10'000));
    RegisterFactory reg({.clk = clk, .resetName = "rst"});
    
    BVec counter(8);
    BVec nextCounter = counter + 0b1_bvec;
    sim_debug() << "Counter value is " << counter << " and next counter value is " << nextCounter;

    driveWith(counter, reg(nextCounter, true, 0b00000000_bvec));


    BVec refCount(8);
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
        Register<BVec> counter(0x00_bvec, regConf);
        counter += 1_bvec;
        sim_debug() << "Counter value is " << counter.delay(1) << " and next counter value is " << counter;

        BVec refCount(8);
        simpleSignalGenerator(clk, [](SimpleSignalGeneratorContext &context){
            context.set(0, context.getTick());
        }, refCount);
        
        sim_assert(counter.delay(1) == refCount) << "The counter should be " << refCount << " but is " << counter.delay(1);    
    }
    
    runTicks(design.getCircuit(), clk, 10);
}

BOOST_FIXTURE_TEST_CASE(DoubleCounterNewSyntax, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;
    
    auto clk = design.createClock<hcl::core::hlim::RootClock>("clk", hcl::core::hlim::ClockRational(10'000));
    RegisterConfig regConf{.clk = clk, .resetName = "rst"};
    
    {
        Register<BVec> counter(0x00_bvec, regConf);
        counter += 1_bvec;
        counter += 1_bvec;
        sim_debug() << "Counter value is " << counter.delay(1) << " and next counter value is " << counter;

        BVec refCount(8);
        simpleSignalGenerator(clk, [](SimpleSignalGeneratorContext &context){
            context.set(0, context.getTick()*2);
        }, refCount);
        
        sim_assert(counter.delay(1) == refCount) << "The counter should be " << refCount << " but is " << counter.delay(1);    
    }
    
    runTicks(design.getCircuit(), clk, 10);
}

BOOST_FIXTURE_TEST_CASE(ShifterNewSyntax, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;
    
    auto clk = design.createClock<hcl::core::hlim::RootClock>("clk", hcl::core::hlim::ClockRational(10'000));
    RegisterConfig regConf{.clk = clk, .resetName = "rst"};
    
    {
        Register<BVec> counter(0x01_bvec, regConf);
        counter <<= 1;
        sim_debug() << "Counter value is " << counter.delay(1) << " and next counter value is " << counter;

        BVec refCount(8);
        simpleSignalGenerator(clk, [](SimpleSignalGeneratorContext &context){
            context.set(0, 1 << context.getTick());
        }, refCount);
        
        sim_assert(counter.delay(1) == refCount) << "The counter should be " << refCount << " but is " << counter.delay(1);    
    }
    
    runTicks(design.getCircuit(), clk, 6);
}

BOOST_FIXTURE_TEST_CASE(RegisterConditionalAssignment, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;
    
    auto clk = design.createClock<hcl::core::hlim::RootClock>("clk", hcl::core::hlim::ClockRational(10'000));
    RegisterConfig regConf{.clk = clk, .resetName = "rst"};
    
    {
        Bit condition;
        simpleSignalGenerator(clk, [](SimpleSignalGeneratorContext &context){
            context.set(0, context.getTick() % 2);
        }, condition);


        Register<BVec> counter(0x00_bvec, regConf);
        IF (condition)
            counter += 1_bvec;

        sim_debug() << "Counter value is " << counter.delay(1) << " and next counter value is " << counter;

        BVec refCount(8);
        simpleSignalGenerator(clk, [](SimpleSignalGeneratorContext &context){
            context.set(0, context.getTick()/2);
        }, refCount);
        
        sim_assert(counter.delay(1) == refCount) << "The counter should be " << refCount << " but is " << counter.delay(1);    
    }
    
    runTicks(design.getCircuit(), clk, 10);
}



BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, ConditionalAssignment, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    BVec a = ConstBVec(x, 8);
    BVec b = ConstBVec(y, 8);

    BVec c;
    IF (a[1])
        c = a + b;
    ELSE {
        c = a - b;
    }
    
    unsigned groundTruth;
    if (unsigned(x) & 2) 
        groundTruth = unsigned(x)+unsigned(y);
    else
        groundTruth = unsigned(x)-unsigned(y);        

    sim_assert(c == ConstBVec(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;
    
    eval(design.getCircuit());
}

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, ConditionalAssignmentMultipleStatements, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    BVec a = ConstBVec(x, 8);
    BVec b = ConstBVec(y, 8);

    BVec c;
    IF (a[1]) {
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

    sim_assert(c == ConstBVec(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;
    
    eval(design.getCircuit());
}

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, ConditionalAssignmentMultipleElseStatements, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    BVec a = ConstBVec(x, 8);
    BVec b = ConstBVec(y, 8);

    BVec c;
    IF (a[1])
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

    sim_assert(c == ConstBVec(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;
    
    eval(design.getCircuit());
}



BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, MultiLevelConditionalAssignment, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    BVec a = ConstBVec(x, 8);
    BVec b = ConstBVec(y, 8);

    BVec c;
    IF (a[2]) {
        IF (a[1])
            c = a + b;
        ELSE {
            c = a - b;
        }
    } ELSE {
        IF (a[1])
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

    sim_assert(c == ConstBVec(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;
    
    eval(design.getCircuit());
}


BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, MultiLevelConditionalAssignmentMultipleStatements, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    BVec a = ConstBVec(x, 8);
    BVec b = ConstBVec(y, 8);

    BVec c;
    IF (a[2]) {
        IF (a[1]) {
            c = a + b;
            c += b;
            c += a;
        } ELSE {
            c = a - b;
        }
    } ELSE {
        IF (a[1])
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

    sim_assert(c == ConstBVec(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;
    
    eval(design.getCircuit());
}

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, MultiLevelConditionalAssignmentWithPreviousAssignmentNoElse, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    BVec a = ConstBVec(x, 8);
    BVec b = ConstBVec(y, 8);

    BVec c = a;
    IF (a[2]) {
        IF (a[1])
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
    
    sim_assert(c == ConstBVec(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;
    
    eval(design.getCircuit());
}



BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, MultiLevelConditionalAssignmentWithPreviousAssignmentNoIf, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    BVec a = ConstBVec(x, 8);
    BVec b = ConstBVec(y, 8);

    BVec c = a;
    IF (a[2]) {
    } ELSE {
        IF (a[1])
            c = b;
    }
    
    unsigned groundTruth = x;
    if (unsigned(x) & 4) {
    } else {
        if (unsigned(x) & 2) 
            groundTruth = y;
    }

    sim_assert(c == ConstBVec(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;
    
    eval(design.getCircuit());
}


BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, MultiLevelConditionalAssignmentWithPreviousAssignment, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    BVec a = ConstBVec(x, 8);
    BVec b = ConstBVec(y, 8);

    BVec c = a;
    IF (a[2]) {
        IF (a[1])
            c = a + b;
        ELSE
            c = a - b;
    } ELSE {
        IF (a[1])
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

    sim_assert(c == ConstBVec(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;
    
    eval(design.getCircuit());
}

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, MultiLevelConditionalAssignmentIfElseIf, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;

    BVec a = ConstBVec(x, 8);
    BVec b = ConstBVec(y, 8);

    BVec c = a;
    IF (a[2]) {
        c = a + b;
    } ELSE {
        IF (a[1])
            c = b;
    }
    
    unsigned groundTruth = x;
    if (unsigned(x) & 4) {
        groundTruth = x+y;
    } else {
        if (unsigned(x) & 2) 
            groundTruth = y;
    }

    sim_assert(c == ConstBVec(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;
    
    eval(design.getCircuit());
}

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, UnsignedCompare, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;

    DesignScope design;

    BVec a = ConstBVec(x, 8);
    BVec b = ConstBVec(y, 8);

    if (x > y)
    {
        sim_assert(a > b);
        sim_assert(!(a <= b));
    }
    else
    {
        sim_assert(!(a > b));
        sim_assert(a <= b);
    }

    if (x < y)
    {
        sim_assert(a < b);
        sim_assert(!(a >= b));
    }
    else
    {
        sim_assert(!(a < b));
        sim_assert(a >= b);
    }

    if (x == y)
    {
        sim_assert(a == b);
        sim_assert(!(a != b));
    }
    else
    {
        sim_assert(a != b);
        sim_assert(!(a == b));
    }

    eval(design.getCircuit());
}
