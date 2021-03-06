/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Michael Offel, Andreas Ley

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "frontend/pch.h"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

using namespace boost::unit_test;

const auto optimizationLevels = data::make({0, 1, 2, 3});

using UnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;

BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, TestOperators, optimizationLevels * data::xrange(1, 8), optimization, bitsize)
{
    using namespace gtry;
    using namespace gtry::sim;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clockScope(clock);

    BVec a = pinIn(BitWidth{(unsigned)bitsize});
    BVec b = pinIn(BitWidth{(unsigned)bitsize});

    size_t x, y;
    addSimulationProcess([=, this, &clock, &x, &y]()->SimProcess {

        for (x = 0; x < 8; x++)
            for (y = 0; y < 8; y++) {
                simu(a) = x;
                simu(b) = y;

                co_await WaitClk(clock);
            }

        stopTest();
    });



#define op2str(op) #op
#define buildOperatorTest(op)                                                                               \
    {                                                                                                       \
        BVec c = a op b;                                                                                    \
        auto pinC = pinOut(c);                                                                              \
                                                                                                            \
        addSimulationProcess([=, this, &clock, &x, &y]()->SimProcess {                                      \
            while (true) {                                                                                  \
                DefaultBitVectorState state = simu(pinC);                                                   \
                                                                                                            \
                BOOST_TEST(allDefinedNonStraddling(state, 0, bitsize));                                     \
                auto v = state.extractNonStraddling(DefaultConfig::VALUE, 0, bitsize);                      \
                std::uint64_t x_ = x & (~0ull >> (64 - bitsize));                                           \
                std::uint64_t y_ = y & (~0ull >> (64 - bitsize));                                           \
                std::uint64_t gt = x_ op y_;                                                                \
                gt &= (~0ull >> (64 - bitsize));                                                            \
                BOOST_TEST(v == gt);                                                                        \
                co_await WaitClk(clock);                                                                    \
            }                                                                                               \
        });                                                                                                 \
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
        BVec c = a;                                                                                         \
        c op b;                                                                                             \
        auto pinC = pinOut(c);                                                                              \
        addSimulationProcess([=, this, &clock, &x, &y]()->SimProcess {                                      \
            while (true) {                                                                                  \
                DefaultBitVectorState state = simu(pinC);                                                   \
                                                                                                            \
                BOOST_TEST(allDefinedNonStraddling(state, 0, bitsize));                                     \
                auto v = state.extractNonStraddling(DefaultConfig::VALUE, 0, bitsize);                      \
                                                                                                            \
                std::uint64_t x_ = x & (~0ull >> (64 - bitsize));                                           \
                std::uint64_t y_ = y & (~0ull >> (64 - bitsize));                                           \
                std::uint64_t gt = x_;                                                                      \
                gt op std::uint64_t(y_);                                                                    \
                gt &= (~0ull >> (64 - bitsize));                                                            \
                                                                                                            \
                BOOST_TEST(v == gt);                                                                        \
                co_await WaitClk(clock);                                                                    \
            }                                                                                               \
        });                                                                                                 \
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

    // @TODO: Choose optimization postprocessor accordingly
    //design.getCircuit().postprocess(optimization);
    design.getCircuit().postprocess(DefaultPostprocessing{});

    runTest(gtry::hlim::ClockRational(100'000, 10'000));
}





BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, TestSlicing, optimizationLevels, optimization)
{
    using namespace gtry;

    for (auto bitsize : gtry::utils::Range(3, 8))
        for (auto x : gtry::utils::Range(8)) {
            BVec a = ConstBVec(x, BitWidth{ uint64_t(bitsize) });

            {
                BVec res = a(0, 1);
                sim_assert(res == ConstBVec(x & 1, 1_b)) << "Slicing first bit of " << a << " failed: " << res;
            }

            {
                BVec res = a(1, 2);
                sim_assert(res == ConstBVec((x >> 1) & 3, 2_b)) << "Slicing second and third bit of " << a << " failed: " << res;
            }

            {
                BVec res = a(1, 2);
                res = 0;
                sim_assert(a == ConstBVec(x, BitWidth{ uint64_t(bitsize) })) << "Modifying copy of slice of a changes a to " << a << ", should be: " << x;
            }
        }

    // @TODO: Choose optimization postprocessor accordingly
    //design.getCircuit().postprocess(optimization);
    design.getCircuit().postprocess(DefaultPostprocessing{});


    runEvalOnlyTest();
}



BOOST_FIXTURE_TEST_CASE(TestSlicingModifications, UnitTestSimulationFixture)
{
    using namespace gtry;

    for (auto bitsize : gtry::utils::Range(3, 8))
        for (auto x : gtry::utils::Range(8)) {

            BVec a = ConstBVec(x, BitWidth{ uint64_t(bitsize) });

            {
                BVec b = a;
                b(1, 2) = 0;

                auto groundTruth = ConstBVec(unsigned(x) & ~0b110, BitWidth{ uint64_t(bitsize) });
                sim_assert(b == groundTruth) << "Clearing two bits out of " << a << " should be " << groundTruth << " but is " << b;
            }
        }

    runEvalOnlyTest();
}


BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, TestSlicingAddition, optimizationLevels, optimization)
{
    using namespace gtry;

    for (auto bitsize : gtry::utils::Range(3, 8))
        for (auto x : gtry::utils::Range(8)) {
            BVec a = ConstBVec(x, BitWidth{ uint64_t(bitsize) });

            {
                BVec b = a;
                b(1, 2) = b(1, 2) + 1;

                auto groundTruth = ConstBVec((unsigned(x) & ~0b110) | (unsigned(x+2) & 0b110), BitWidth{ uint64_t(bitsize) });
                sim_assert(b == groundTruth) << "Incrementing two bits out of " << a << " should be " << groundTruth << " but is " << b;
            }
        }

    // @TODO: Choose optimization postprocessor accordingly
    //design.getCircuit().postprocess(optimization);
    design.getCircuit().postprocess(DefaultPostprocessing{});


    runEvalOnlyTest();
}




BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, SimpleAdditionNetwork, optimizationLevels, optimization)
{
    using namespace gtry;

    for (auto bitsize : gtry::utils::Range(1, 8))
        for (auto x : gtry::utils::Range(8))
            for (auto y : gtry::utils::Range(8)) {
                BVec a = ConstBVec(x, BitWidth{ uint64_t(bitsize) });
                sim_debug() << "Signal a is " << a;

                BVec b = ConstBVec(y, BitWidth{ uint64_t(bitsize) });
                sim_debug() << "Signal b is " << b;

                BVec c = a + b;
                sim_debug() << "Signal c (= a + b) is " << c;

                sim_assert(c == ConstBVec(x+y, BitWidth{ uint64_t(bitsize) })) << "The signal c should be " << x+y << " (with overflow in " << bitsize << "bits) but is " << c;
            }
    // @TODO: Choose optimization postprocessor accordingly
    //design.getCircuit().postprocess(optimization);
    design.getCircuit().postprocess(DefaultPostprocessing{});


    runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(BitFromBool, UnitTestSimulationFixture)
{
    using namespace gtry;

    for (auto l : gtry::utils::Range(2))
        for (auto r : gtry::utils::Range(2)) {

            Bit a = l != 0;
            Bit b;
            b = r != 0;

            sim_assert((a == b) == Bit{ l == r })  << "test 0: " << a << "," << b;
            sim_assert((a != b) == Bit{ l != r })  << "test 1: " << a << "," << b;
            sim_assert((a == true) == Bit(l != 0)) << "test 2: " << a << "," << b;
            sim_assert((true == a) == Bit(l != 0)) << "test 3: " << a << "," << b;
            sim_assert((a != true) == Bit(l == 0)) << "test 4: " << a << "," << b;
            sim_assert((true != a) == Bit(l == 0)) << "test 5: " << a << "," << b;
        }

    runEvalOnlyTest();
}


BOOST_FIXTURE_TEST_CASE(SimpleCounterNewSyntax, UnitTestSimulationFixture)
{
    using namespace gtry;

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

    runFixedLengthTest(10u / clock.getClk()->getAbsoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(SignalMoveAssignment, UnitTestSimulationFixture)
{
    using namespace gtry;

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

    runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(BVecBitAliasConditionCheck, UnitTestSimulationFixture)
{
    using namespace gtry;

    BVec a = "xFF";
    Bit c = '0';

    IF(c)
    {
        a.msb() = '0';
        a.lsb() = '0';
        a[1] = '0';
    }
    sim_assert(a == 255);

    runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(SwapMoveAssignment, UnitTestSimulationFixture)
{
    using namespace gtry;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clockScope(clock);

    {
        gtry::BVec a = "xa";
        gtry::BVec b = "xb";
        HCL_NAMED(a);
        HCL_NAMED(b);
        std::swap(a, b);

        sim_assert(a == "xb");
        sim_assert(b == "xa");
    }

    {
        gtry::Bit x = '0';
        gtry::Bit y = '1';
        HCL_NAMED(x);
        HCL_NAMED(y);
        std::swap(x, y);

        sim_assert(x == '1');
        sim_assert(y == '0');
    }

    {
        gtry::BVec c = 0xC;
        gtry::BVec d = 0xD;
        HCL_NAMED(c);
        HCL_NAMED(d);
        gtry::Bit x = '0';
        gtry::Bit y = '1';
        HCL_NAMED(x);
        HCL_NAMED(y);

        InputPin pinConditionIn = pinIn(); // TODO default value for input pins (simulation and vhdl export)
        gtry::Bit condition = pinConditionIn;
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

        addSimulationProcess([=, this, &clock]()->SimProcess {

            simu(pinConditionIn) = 0;
            BOOST_TEST(simu(pinC) == 0xC);
            BOOST_TEST(simu(pinD) == 0xD);
            BOOST_TEST(simu(pinX) == 0);
            BOOST_TEST(simu(pinY) == 1);
            co_await WaitClk(clock);

            simu(pinConditionIn) = 1;
            BOOST_TEST(simu(pinC) == 0xD);
            BOOST_TEST(simu(pinD) == 0xC);
            BOOST_TEST(simu(pinX) == 1);
            BOOST_TEST(simu(pinY) == 0);
            co_await WaitClk(clock);

            stopTest();
        });

    }

    design.getCircuit().postprocess(gtry::DefaultPostprocessing{});

    runTest(100u / clock.getClk()->getAbsoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(RotateMoveAssignment, UnitTestSimulationFixture)
{
    using namespace gtry;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clockScope(clock);

    {
        gtry::Vector<BVec> listA(4);
        for (size_t i = 0; i < listA.size(); ++i)
            listA[i] = ConstBVec(i, 2_b);
        HCL_NAMED(listA);
        std::rotate(listA.begin(), listA.begin() + 1, listA.end());

        sim_assert(listA[0] == 1);
        sim_assert(listA[1] == 2);
        sim_assert(listA[2] == 3);
        sim_assert(listA[3] == 0);
    }

    {
        std::vector<InputPins> in;
        gtry::Vector<BVec> listB;
        for (size_t i = 0; i < 4; ++i)
        {
            in.emplace_back(2_b);
            listB.emplace_back(in.back());
        }
        HCL_NAMED(listB);

        InputPin pinConditionIn = pinIn();
        gtry::Bit condition = pinConditionIn;
        HCL_NAMED(condition);

        IF(condition)
            std::rotate(listB.begin(), listB.begin() + 1, listB.end());

        std::vector<OutputPins> out;
        for (BVec& i : listB)
            out.emplace_back(i);

        addSimulationProcess([=, this, &clock]()->SimProcess {

            for (size_t i = 0; i < in.size(); ++i)
                simu(in[i]) = i;
            simu(pinConditionIn) = 0;

            for (size_t i = 0; i < in.size(); ++i)
                BOOST_TEST(simu(out[i]) == i);
            co_await WaitClk(clock);

            simu(pinConditionIn) = 1;
            for (size_t i = 0; i < in.size(); ++i)
                BOOST_TEST(simu(out[i]) == (i + 1) % 4);
            co_await WaitClk(clock);

            stopTest();
        });

    }

    design.getCircuit().postprocess(gtry::DefaultPostprocessing{});
    runTest(100u / clock.getClk()->getAbsoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(ConditionalLoopAssignment, UnitTestSimulationFixture)
{
    using namespace gtry;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clockScope(clock);

    gtry::Bit condition = '1';
    gtry::BVec counter = 4_b;
    HCL_NAMED(condition);
    HCL_NAMED(counter);

    IF(condition)
        counter += 1;
    counter = reg(counter);

    runFixedLengthTest(100u / clock.getClk()->getAbsoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(SimpleCounterClockSyntax, UnitTestSimulationFixture)
{
    using namespace gtry;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clockScope(clock);

    {
        BVec counter(8_b);
        counter = reg(counter, "8b0");

        addSimulationProcess([=, this, &clock]()->SimProcess{
            for (unsigned refCount = 0; refCount < 10; refCount++) {
                BOOST_TEST(refCount == simu(counter));
                BOOST_TEST(simu(counter).defined() == 0xFF);

                co_await WaitClk(clock);
            }
            stopTest();
        });

        counter += 1;
    }

    runTest(100u / clock.getClk()->getAbsoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(ClockRegisterReset, UnitTestSimulationFixture)
{
    using namespace gtry;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clockScope(clock);

    {
        BVec vec1 = reg(BVec{ "b01" });
        BVec vec2 = reg(BVec{ "b01" }, "2b");
        Bit bit1 = reg(Bit{ '1' });
        Bit bit2 = reg(Bit{ '1' }, '0');

        BVec ref(2_b);
        simpleSignalGenerator(clock, [](SimpleSignalGeneratorContext& context) {
            context.set(0, context.getTick() ? 1 : 0);
            }, ref);

        sim_assert((ref == 0) | (vec1 == ref)) << "should be " << ref << " but is " << vec1;
        sim_assert((ref == 0) | (bit1 == ref[0])) << "should be " << ref[0] << " but is " << bit1;
        sim_assert(vec2 == ref) << "should be " << ref << " but is " << vec2;
        sim_assert(bit2 == ref[0]) << "should be " << ref[0] << " but is " << bit2;
    }

    runFixedLengthTest(3u / clock.getClk()->getAbsoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(DoubleCounterNewSyntax, UnitTestSimulationFixture)
{
    using namespace gtry;

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

    runFixedLengthTest(10u / clock.getClk()->getAbsoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(ShifterNewSyntax, UnitTestSimulationFixture)
{
    using namespace gtry;



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

    runFixedLengthTest(6u / clock.getClk()->getAbsoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(RegisterConditionalAssignment, UnitTestSimulationFixture)
{
    using namespace gtry;



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

    runFixedLengthTest(10u / clock.getClk()->getAbsoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(StringLiteralParsing, UnitTestSimulationFixture)
{
    using namespace gtry;



    BVec a = "d7";
    BOOST_TEST(a.size() == 3);

    BVec b = "4d7";
    BOOST_TEST(b.size() == 4);
    sim_assert(b == "x7");
    sim_assert(b == 7);
    sim_assert(b == "b0111");
    sim_assert(b == "4o7");

    runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(ShiftOp, UnitTestSimulationFixture)
{
    using namespace gtry;



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

    runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(ConditionalAssignment, UnitTestSimulationFixture)
{
    using namespace gtry;

    for (auto x : gtry::utils::Range(8))
        for (auto y : gtry::utils::Range(8)) {
            BVec a = ConstBVec(x, 8_b);
            BVec b = ConstBVec(y, 8_b);

            BVec c = ConstBVec(8_b);
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

            sim_assert(c == ConstBVec(groundTruth, 8_b)) << "The signal should be " << groundTruth << " but is " << c;
        }

    runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(ConditionalAssignmentMultipleStatements, UnitTestSimulationFixture)
{
    using namespace gtry;

    for (auto x : gtry::utils::Range(8))
        for (auto y : gtry::utils::Range(8)) {

            BVec a = ConstBVec(x, 8_b);
            BVec b = ConstBVec(y, 8_b);

            BVec c = ConstBVec(8_b);
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

            sim_assert(c == ConstBVec(groundTruth, 8_b)) << "The signal should be " << groundTruth << " but is " << c;
        }

    runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(ConditionalAssignmentMultipleElseStatements, UnitTestSimulationFixture)
{
    using namespace gtry;

    for (auto x : gtry::utils::Range(8))
        for (auto y : gtry::utils::Range(8)) {

            BVec a = ConstBVec(x, 8_b);
            BVec b = ConstBVec(y, 8_b);

            BVec c = ConstBVec(8_b);
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

            sim_assert(c == ConstBVec(groundTruth, 8_b)) << "The signal should be " << groundTruth << " but is " << c;
        }

    runEvalOnlyTest();
}



BOOST_FIXTURE_TEST_CASE(MultiLevelConditionalAssignment, UnitTestSimulationFixture)
{
    using namespace gtry;


    for (auto x : gtry::utils::Range(8))
        for (auto y : gtry::utils::Range(8)) {

            BVec a = ConstBVec(x, 8_b);
            BVec b = ConstBVec(y, 8_b);

            BVec c = ConstBVec(8_b);
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

            sim_assert(c == ConstBVec(groundTruth, 8_b)) << "The signal should be " << groundTruth << " but is " << c;
        }

    runEvalOnlyTest();
}


BOOST_FIXTURE_TEST_CASE(MultiLevelConditionalAssignmentMultipleStatements, UnitTestSimulationFixture)
{
    using namespace gtry;

    for (auto x : gtry::utils::Range(8))
        for (auto y : gtry::utils::Range(8)) {

            BVec a = ConstBVec(x, 8_b);
            BVec b = ConstBVec(y, 8_b);

            BVec c = ConstBVec(8_b);
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

            sim_assert(c == ConstBVec(groundTruth, 8_b)) << "The signal should be " << groundTruth << " but is " << c;
        }

    runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(MultiElseConditionalAssignment, UnitTestSimulationFixture)
{
    using namespace gtry;

    for (auto x : gtry::utils::Range(8))
        for (auto y : gtry::utils::Range(8)) {

            BVec a = ConstBVec(x, 8_b);
            BVec b = ConstBVec(y, 8_b);

            BVec c = ConstBVec(8_b);
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


            sim_assert(c == ConstBVec(groundTruth, 8_b)) << "The signal should be " << groundTruth << " but is " << c;
        }

    runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(MultiLevelConditionalAssignmentWithPreviousAssignmentNoElse, UnitTestSimulationFixture)
{
    using namespace gtry;

    for (auto x : gtry::utils::Range(8))
        for (auto y : gtry::utils::Range(8)) {

            BVec a = ConstBVec(x, 8_b);
            BVec b = ConstBVec(y, 8_b);

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

            sim_assert(c == ConstBVec(groundTruth, 8_b)) << "The signal should be " << groundTruth << " but is " << c;
        }

    runEvalOnlyTest();
}



BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, MultiLevelConditionalAssignmentWithPreviousAssignmentNoIf, optimizationLevels, optimization)
{
    using namespace gtry;

    for (auto x : gtry::utils::Range(8))
        for (auto y : gtry::utils::Range(8)) {
            BVec a = ConstBVec(x, 8_b);
            BVec b = ConstBVec(y, 8_b);

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

            sim_assert(c == ConstBVec(groundTruth, 8_b)) << "The signal should be " << groundTruth << " but is " << c;
        }

    // @TODO: Choose optimization postprocessor accordingly
    //design.getCircuit().postprocess(optimization);
    design.getCircuit().postprocess(DefaultPostprocessing{});

    runEvalOnlyTest();
}


BOOST_DATA_TEST_CASE_F(UnitTestSimulationFixture, MultiLevelConditionalAssignmentWithPreviousAssignment, optimizationLevels, optimization)
{
    using namespace gtry;

    for (auto x : gtry::utils::Range(8))
        for (auto y : gtry::utils::Range(8)) {
            BVec a = ConstBVec(x, 8_b);
            BVec b = ConstBVec(y, 8_b);

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

            sim_assert(c == ConstBVec(groundTruth, 8_b)) << "The signal should be " << groundTruth << " but is " << c;
        }

    // @TODO: Choose optimization postprocessor accordingly
    //design.getCircuit().postprocess(optimization);
    design.getCircuit().postprocess(DefaultPostprocessing{});

    runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(MultiLevelConditionalAssignmentIfElseIf, UnitTestSimulationFixture)
{
    using namespace gtry;


    for (auto x : gtry::utils::Range(8))
        for (auto y : gtry::utils::Range(8)) {

            BVec a = ConstBVec(x, 8_b);
            BVec b = ConstBVec(y, 8_b);

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

            sim_assert(c == ConstBVec(groundTruth, 8_b)) << "The signal should be " << groundTruth << " but is " << c;
        }

    runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(UnsignedCompare, UnitTestSimulationFixture)
{
    using namespace gtry;


    for (auto x : gtry::utils::Range(8))
        for (auto y : gtry::utils::Range(8)) {

            BVec a = ConstBVec(x, 8_b);
            BVec b = ConstBVec(y, 8_b);

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
        }

    runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(BVecArithmeticOpSyntax, UnitTestSimulationFixture)
{
    using namespace gtry;



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
    using namespace gtry;



    BVec in = 5;

    '1' & in;
    in & '1';

}

BOOST_FIXTURE_TEST_CASE(SimpleCat, UnitTestSimulationFixture)
{
    using namespace gtry;

    BVec vec = 42u;
    BVec vec_2 = pack('1', vec, '0');
    BOOST_TEST(vec_2.size() == 8);
    sim_assert(vec_2 == 42u * 2 + 128) << "result is " << vec_2;

    runEvalOnlyTest();
}

BOOST_FIXTURE_TEST_CASE(msbBroadcast, UnitTestSimulationFixture)
{
    using namespace gtry;



    BVec vec = "4b0000";
    BVec vec_2 = "4b1000";
    vec ^= vec_2.msb();

    sim_assert(vec == "4b1111") << "result is " << vec << " but should be 1111";

    runEvalOnlyTest();
}
