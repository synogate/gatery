#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/frontend.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>
#include <hcl/simulation/waveformFormats/VCDSink.h>

#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>

using namespace boost::unit_test;

const auto optimizationLevels = data::make({0, 1, 2, 3});

using UnitTestSimulationFixture = hcl::core::frontend::UnitTestSimulationFixture;

BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, TestOperators, optimizationLevels * data::xrange(8) * data::xrange(8) * data::xrange(1, 8), optimization, x, y, bitsize)
{
    using namespace hcl::core::frontend;

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

    design.getCircuit().optimize(optimization);

    eval();
}





BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, TestSlicing, optimizationLevels * data::xrange(8) * data::xrange(3, 8), optimization, x, bitsize)
{
    using namespace hcl::core::frontend;

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
        res = 0;
        sim_assert(a == ConstBVec(x, bitsize)) << "Modifying copy of slice of a changes a to " << a << ", should be: " << x;
    }

    design.getCircuit().optimize(optimization);

    eval();
}



BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, TestSlicingModifications, data::xrange(8) * data::xrange(3, 8), x, bitsize)
{
    using namespace hcl::core::frontend;

    BVec a = ConstBVec(x, bitsize);

    {
        BVec b = a;
        b(1, 2) = 0;

        auto groundTruth = ConstBVec(unsigned(x) & ~0b110, bitsize);
        sim_assert(b == groundTruth) << "Clearing two bits out of " << a << " should be " << groundTruth << " but is " << b;
    }

    eval();
}


BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, TestSlicingAddition, optimizationLevels * data::xrange(8) * data::xrange(3, 8), optimization, x, bitsize)
{
    using namespace hcl::core::frontend;

    BVec a = ConstBVec(x, bitsize);

    {
        BVec b = a;
        b(1, 2) = b(1, 2) + 1;

        auto groundTruth = ConstBVec((unsigned(x) & ~0b110) | (unsigned(x+2) & 0b110), bitsize);
        sim_assert(b == groundTruth) << "Incrementing two bits out of " << a << " should be " << groundTruth << " but is " << b;
    }

    design.getCircuit().optimize(optimization);

    eval();
}




BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, SimpleAdditionNetwork, optimizationLevels * data::xrange(8) * data::xrange(8) * data::xrange(1, 8), optimization, x, y, bitsize)
{
    using namespace hcl::core::frontend;

    BVec a = ConstBVec(x, bitsize);
    sim_debug() << "Signal a is " << a;

    BVec b = ConstBVec(y, bitsize);
    sim_debug() << "Signal b is " << b;

    BVec c = a + b;
    sim_debug() << "Signal c (= a + b) is " << c;

    sim_assert(c == ConstBVec(x+y, bitsize)) << "The signal c should be " << x+y << " (with overflow in " << bitsize << "bits) but is " << c;

    design.getCircuit().optimize(optimization);
    eval();
}

BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, BitFromBool, data::xrange(2) * data::xrange(2), l, r)
{
    using namespace hcl::core::frontend;

    Bit a = l != 0;
    Bit b;
    b = r != 0;

    sim_assert((a == b) == Bit{ l == r })  << "test 0: " << a << "," << b;
    sim_assert((a != b) == Bit{ l != r })  << "test 1: " << a << "," << b;
    sim_assert((a == true) == Bit(l != 0)) << "test 2: " << a << "," << b;
    sim_assert((true == a) == Bit(l != 0)) << "test 3: " << a << "," << b;
    sim_assert((a != true) == Bit(l == 0)) << "test 4: " << a << "," << b;
    sim_assert((true != a) == Bit(l == 0)) << "test 5: " << a << "," << b;

    eval();
}


BOOST_FIXTURE_TEST_CASE(SimpleCounterNewSyntax, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clockScope(clock);

    {
        Register<BVec> counter(8_b);
        counter.setReset("8b0");
        counter += 1;
        sim_debug() << "Counter value is " << counter.delay(1) << " and next counter value is " << counter;

        BVec refCount(8_b);
        simpleSignalGenerator(clock, [](SimpleSignalGeneratorContext &context){
            context.set(0, context.getTick());
        }, refCount);

        sim_assert(counter.delay(1) == refCount) << "The counter should be " << refCount << " but is " << counter.delay(1);
    }

    runTicks(clock.getClk(), 10);
}

BOOST_FIXTURE_TEST_CASE(SignalMoveAssignment, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    {
        Bit a;
        Bit b = a;
        Bit c = std::move(a);
        c = '1';
        sim_assert(b == '1');
    }
    {
        BVec a = 4_b;
        BVec b = a;
        BVec c = std::move(a);
        c = 1;
        sim_assert(b == 1);
    }

    eval();
}

BOOST_FIXTURE_TEST_CASE(BVecBitAliasConditionCheck, UnitTestSimulationFixture)
{
    using namespace hcl;

    BVec a = "xFF";
    Bit c = '0';

    IF(c)
    {
        a.msb() = '0';
        a.lsb() = '0';
        a[1] = '0';
    }
    sim_assert(a == 255);

    eval();
}

BOOST_FIXTURE_TEST_CASE(SwapMoveAssignment, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clockScope(clock);

    {
        hcl::BVec a = "xa";
        hcl::BVec b = "xb";
        HCL_NAMED(a);
        HCL_NAMED(b);
        std::swap(a, b);

        sim_assert(a == "xb");
        sim_assert(b == "xa");
    }

    {
        hcl::Bit x = '0';
        hcl::Bit y = '1';
        HCL_NAMED(x);
        HCL_NAMED(y);
        std::swap(x, y);

        sim_assert(x == '1');
        sim_assert(y == '0');
    }

    {
        hcl::BVec c = 0xC;
        hcl::BVec d = 0xD;
        HCL_NAMED(c);
        HCL_NAMED(d);
        hcl::Bit x = '0';
        hcl::Bit y = '1';
        HCL_NAMED(x);
        HCL_NAMED(y);

        InputPin pinConditionIn = pinIn(); // TODO default value for input pins (simulation and vhdl export)
        hcl::Bit condition = pinConditionIn;
        HCL_NAMED(condition);

        IF(condition)
        {
            std::swap(c, d);
            std::swap(x, y);
        }

        auto pinC = pinOut(c);
        auto pinD = pinOut(d);
        auto pinX = pinOut(x);
        auto pinY = pinOut(y);

        addSimulationProcess([=, &clock]()->SimProcess {

            sim(pinConditionIn) = 0;
            BOOST_TEST(sim(pinC) == 0xC);
            BOOST_TEST(sim(pinD) == 0xD);
            BOOST_TEST(sim(pinX) == 0);
            BOOST_TEST(sim(pinY) == 1);
            co_await WaitClk(clock);

            sim(pinConditionIn) = 1;
            BOOST_TEST(sim(pinC) == 0xD);
            BOOST_TEST(sim(pinD) == 0xC);
            BOOST_TEST(sim(pinX) == 1);
            BOOST_TEST(sim(pinY) == 0);
            co_await WaitClk(clock);

        });

    }

    design.getCircuit().optimize(3);
    runTicks(clock.getClk(), 100);
}

BOOST_FIXTURE_TEST_CASE(RotateMoveAssignment, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clockScope(clock);

    {
        hcl::Vector<BVec> listA(4);
        for (size_t i = 0; i < listA.size(); ++i)
            listA[i] = ConstBVec(i, 2);
        HCL_NAMED(listA);
        std::rotate(listA.begin(), listA.begin() + 1, listA.end());

        sim_assert(listA[0] == 1);
        sim_assert(listA[1] == 2);
        sim_assert(listA[2] == 3);
        sim_assert(listA[3] == 0);
    }

    {
        std::vector<InputPins> in;
        hcl::Vector<BVec> listB;
        for (size_t i = 0; i < 4; ++i)
        {
            in.emplace_back(2_b);
            listB.emplace_back(in.back());
        }
        HCL_NAMED(listB);

        InputPin pinConditionIn = pinIn();
        hcl::Bit condition = pinConditionIn;
        HCL_NAMED(condition);

        IF(condition)
            std::rotate(listB.begin(), listB.begin() + 1, listB.end());

        std::vector<OutputPins> out;
        for (BVec& i : listB)
            out.emplace_back(i);

        addSimulationProcess([=, &clock]()->SimProcess {

            for (size_t i = 0; i < in.size(); ++i)
                sim(in[i]) = i;
            sim(pinConditionIn) = 0;

            for (size_t i = 0; i < in.size(); ++i)
                BOOST_TEST(sim(out[i]) == i);
            co_await WaitClk(clock);

            sim(pinConditionIn) = 1;
            for (size_t i = 0; i < in.size(); ++i)
                BOOST_TEST(sim(out[i]) == (i + 1) % 4);
            co_await WaitClk(clock);

        });

    }

    design.getCircuit().optimize(3);
    runTicks(clock.getClk(), 100);
}

BOOST_FIXTURE_TEST_CASE(ConditionalLoopAssignment, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clockScope(clock);

    hcl::Bit condition = '1';
    hcl::BVec counter = 4_b;
    HCL_NAMED(condition);
    HCL_NAMED(counter);

    IF(condition)
        counter += 1;
    counter = reg(counter);

    runTicks(clock.getClk(), 100);
}

BOOST_FIXTURE_TEST_CASE(SimpleCounterClockSyntax, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clockScope(clock);

    {
        BVec counter(8_b);
        counter = reg(counter, "8b0");

        BVec refCount(8_b);
        simpleSignalGenerator(clock, [](SimpleSignalGeneratorContext& context) {
            context.set(0, context.getTick());
            }, refCount);

        sim_assert(counter == refCount) << "The counter should be " << refCount << " but is " << counter;

        counter += 1;
    }

    runTicks(clock.getClk(), 18);
}

BOOST_FIXTURE_TEST_CASE(ClockRegisterReset, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clockScope(clock);

    {
        BVec vec1 = reg("b01");
        BVec vec2 = reg("b01", "2b");
        Bit bit1 = reg('1');
        Bit bit2 = reg('1', '0');

        BVec ref(2_b);
        simpleSignalGenerator(clock, [](SimpleSignalGeneratorContext& context) {
            context.set(0, context.getTick() ? 1 : 0);
            }, ref);

        sim_assert((ref == 0) | (vec1 == ref)) << "should be " << ref << " but is " << vec1;
        sim_assert((ref == 0) | (bit1 == ref[0])) << "should be " << ref[0] << " but is " << bit1;
        sim_assert(vec2 == ref) << "should be " << ref << " but is " << vec2;
        sim_assert(bit2 == ref[0]) << "should be " << ref[0] << " but is " << bit2;
    }

    runTicks(clock.getClk(), 3);
}

BOOST_FIXTURE_TEST_CASE(DoubleCounterNewSyntax, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clockScope(clock);

    {
        Register<BVec> counter(8_b);
        counter.setReset("8b0");

        counter += 1;
        counter += 1;
        sim_debug() << "Counter value is " << counter.delay(1) << " and next counter value is " << counter;

        BVec refCount(8_b);
        simpleSignalGenerator(clock, [](SimpleSignalGeneratorContext &context){
            context.set(0, context.getTick()*2);
        }, refCount);

        sim_assert(counter.delay(1) == refCount) << "The counter should be " << refCount << " but is " << counter.delay(1);
    }

    runTicks(clock.getClk(), 10);
}

BOOST_FIXTURE_TEST_CASE(ShifterNewSyntax, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;



    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clockScope(clock);

    {
        Register<BVec> counter(8_b);
        counter.setReset("8b1");

        counter <<= 1;
        sim_debug() << "Counter value is " << counter.delay(1) << " and next counter value is " << counter;

        BVec refCount(8_b);
        simpleSignalGenerator(clock, [](SimpleSignalGeneratorContext &context){
            context.set(0, 1ull << context.getTick());
        }, refCount);

        sim_assert(counter.delay(1) == refCount) << "The counter should be " << refCount << " but is " << counter.delay(1);
    }

    runTicks(clock.getClk(), 6);
}

BOOST_FIXTURE_TEST_CASE(RegisterConditionalAssignment, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;



    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clockScope(clock);
    {
        Bit condition;
        simpleSignalGenerator(clock, [](SimpleSignalGeneratorContext &context){
            context.set(0, context.getTick() % 2);
        }, condition);


        Register<BVec> counter(8_b);
        counter.setReset("8b0");

        IF (condition)
            counter += 1;

        sim_debug() << "Counter value is " << counter.delay(1) << " and next counter value is " << counter;

        BVec refCount(8_b);
        simpleSignalGenerator(clock, [](SimpleSignalGeneratorContext &context){
            context.set(0, context.getTick()/2);
        }, refCount);

        sim_assert(counter.delay(1) == refCount) << "The counter should be " << refCount << " but is " << counter.delay(1);
    }

    runTicks(clock.getClk(), 10);
}

BOOST_FIXTURE_TEST_CASE(StringLiteralParsing, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;



    BVec a = "d7";
    BOOST_TEST(a.size() == 3);

    BVec b = "4d7";
    BOOST_TEST(b.size() == 4);
    sim_assert(b == "x7");
    sim_assert(b == 7);
    sim_assert(b == "b0111");
    sim_assert(b == "4o7");

    eval();
}

BOOST_FIXTURE_TEST_CASE(ShiftOp, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;



    sim_assert(zshr("xA0", "x4") == "x0A") << "zshr failed";
    sim_assert(oshr("xA0", "x4") == "xFA") << "oshr failed";
    sim_assert(sshr("xA0", "x4") == "xFA") << "sshr failed";
    sim_assert(sshr("x70", "x4") == "x07") << "sshr failed";
    sim_assert(rotr("xA4", "x4") == "x4A") << "rotr failed";

    sim_assert(zshl("x0A", "x4") == "xA0") << "zshl failed";
    sim_assert(oshl("x0B", "x4") == "xBF") << "oshl failed";
    sim_assert(sshl("x0B", "x4") == "xBF") << "sshl failed";
    sim_assert(sshl("x0A", "x4") == "xA0") << "sshl failed";
    sim_assert(rotl("x4A", "x4") == "xA4") << "rotl failed";

    eval();
}

BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, ConditionalAssignment, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;



    BVec a = ConstBVec(x, 8);
    BVec b = ConstBVec(y, 8);

    BVec c = ConstBVec(8);
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

    eval();
}

BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, ConditionalAssignmentMultipleStatements, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;



    BVec a = ConstBVec(x, 8);
    BVec b = ConstBVec(y, 8);

    BVec c = ConstBVec(8);
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

    eval();
}

BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, ConditionalAssignmentMultipleElseStatements, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;



    BVec a = ConstBVec(x, 8);
    BVec b = ConstBVec(y, 8);

    BVec c = ConstBVec(8);
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

    eval();
}



BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, MultiLevelConditionalAssignment, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;



    BVec a = ConstBVec(x, 8);
    BVec b = ConstBVec(y, 8);

    BVec c = ConstBVec(8);
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

    eval();
}


BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, MultiLevelConditionalAssignmentMultipleStatements, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;



    BVec a = ConstBVec(x, 8);
    BVec b = ConstBVec(y, 8);

    BVec c = ConstBVec(8);
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

    eval();
}

BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, MultiElseConditionalAssignment, data::xrange(8)* data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;



    BVec a = ConstBVec(x, 8);
    BVec b = ConstBVec(y, 8);

    BVec c = ConstBVec(8);
    IF(a[2]) {
        IF(a[1]) {
            c = a + b;
            c += b;
            c += a;
        } ELSE{
            c = a - b;
        }
    } ELSE IF(a[1])
        c = a;
    ELSE
        c = b;

    unsigned groundTruth;
    if (unsigned(x) & 4) {
        if (unsigned(x) & 2) {
            groundTruth = x + y;
            groundTruth += y;
            groundTruth += x;
        }
        else
            groundTruth = x - y;
    }
    else
        if (unsigned(x) & 2)
            groundTruth = x;
        else
            groundTruth = y;


    sim_assert(c == ConstBVec(groundTruth, 8)) << "The signal should be " << groundTruth << " but is " << c;

    eval();
}

BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, MultiLevelConditionalAssignmentWithPreviousAssignmentNoElse, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;



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

    eval();
}



BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, MultiLevelConditionalAssignmentWithPreviousAssignmentNoIf, optimizationLevels * data::xrange(8) * data::xrange(8), optimization, x, y)
{
    using namespace hcl::core::frontend;



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

    design.getCircuit().optimize(optimization);
    eval();
}


BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, MultiLevelConditionalAssignmentWithPreviousAssignment, optimizationLevels * data::xrange(8) * data::xrange(8), optimization, x, y)
{
    using namespace hcl::core::frontend;



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

    design.getCircuit().optimize(optimization);
    eval();
}

BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, MultiLevelConditionalAssignmentIfElseIf, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;



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

    eval();
}

BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, UnsignedCompare, data::xrange(8) * data::xrange(8), x, y)
{
    using namespace hcl::core::frontend;



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

    eval();
}

BOOST_FIXTURE_TEST_CASE(BVecArithmeticOpSyntax, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;



    BVec in = 5;
    BVec res = in + 5u;
    in - 5u;
    in * 5u;
    in / 5u;
    in % 5u;

    in += 2u;
    in -= 1u;
    in *= 2u;
    in /= 2u;
    in %= 3u;

    in + '1';
    in - true;
    in += '0';
    in -= false;

}

BOOST_FIXTURE_TEST_CASE(LogicOpSyntax, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;



    BVec in = 5;

    '1' & in;
    in & '1';

}

BOOST_FIXTURE_TEST_CASE(SimpleCat, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;

    BVec vec = 42u;
    BVec vec_2 = pack('1', vec, '0');
    BOOST_TEST(vec_2.size() == 8);
    sim_assert(vec_2 == 42u * 2 + 128) << "result is " << vec_2;

    eval();
}

BOOST_FIXTURE_TEST_CASE(msbBroadcast, UnitTestSimulationFixture)
{
    using namespace hcl::core::frontend;



    BVec vec = "4b0000";
    BVec vec_2 = "4b1000";
    vec ^= vec_2.msb();

    sim_assert(vec == "4b1111") << "result is " << vec << " but should be 1111";

    eval();
}
