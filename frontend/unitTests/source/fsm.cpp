#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/frontend.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>

#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>

using namespace boost::unit_test;

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, TestGCD, data::make({1, 2, 3, 4, 5, 10, 42}) * data::make({1, 2, 3, 4, 5, 23, 56, 126}), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;
    
    auto clk = design.createClock<hcl::core::hlim::RootClock>("clk", hcl::core::hlim::ClockRational(10'000));
    RegisterConfig regConf = {.clk = clk, .resetName = "rst"};
    RegisterFactory reg(regConf);
    
    auto gcd_ref = [](unsigned a, unsigned b) -> unsigned {
        while (a != b) {
            if (a > b)
                a -= b;
            else
                b -= a;
        }
        return a;
    };
    
    
    unsigned maxTicks = 200;
    
    {
        BVec x_vec = ConstBVec(x, 8);
        BVec y_vec = ConstBVec(y, 8);

        Bit start;
        simpleSignalGenerator(clk, [](SimpleSignalGeneratorContext &context){
            context.set(0, context.getTick() == 0);
        }, start);
        
        BVec result;
        Bit done;
        
        {
            HCL_NAMED(x_vec);
            HCL_NAMED(y_vec);
            
            GroupScope entity(hcl::core::hlim::NodeGroup::GRP_ENTITY);
            entity
                .setName("gcd")
                .setComment("Statemachine to compute the GCD of two 8-bit integers.");
            
            
            fsm::ImmediateState idle;
            HCL_NAMED(idle);
            fsm::DelayedState running;
            HCL_NAMED(running);

            Register<BVec> a(regConf, 0b00000000_bvec);
            a = a.delay(1);
            Register<BVec> b(regConf, 0b00000000_bvec);
            b = b.delay(1);

#if 0
            // Euclid's gcd
            idle.onActive([&]{
                IF (start) {
                    a = x_vec;
                    b = y_vec;
                    fsm::delayedSwitch(running);
                }
            });
            running.onActive([&]{
                IF (a.delay(1) == b.delay(1)) {
                    fsm::immediateSwitch(idle);
                } ELSE {
                    IF (a.delay(1) > b.delay(1)) {
                        a = a.delay(1) - b.delay(1);
                    } ELSE {
                        b = b.delay(1) - a.delay(1);
                    }
                }
            });
#else
            // Binary GCD
            fsm::ImmediateState shifting;
            HCL_NAMED(shifting);

            Register<BVec> d(regConf, 0b0000_bvec);
            d = d.delay(1);
            
            idle.onActive([&]{
                IF (start) {
                    a = x_vec;
                    b = y_vec;
                    d = (0_bvec).zext(4);
                    fsm::delayedSwitch(running);
                }
            });
            running.onActive([&]{
                IF (a == b) {
                    fsm::immediateSwitch(shifting);
                } ELSE {
                    Bit a_odd = a[0];
                    Bit b_odd = b[0];
                    IF (!a_odd && !b_odd) {
                        a >>= 1;
                        b >>= 1;
                        d += 1_bvec;
                    } 
                    IF (!a_odd && b_odd) {
                        a >>= 1;
                    }
                    IF (a_odd && !b_odd) {
                        b >>= 1;
                    }
                    IF (a_odd && b_odd) {
                        IF (a < b) {
                            BVec help = a;
                            a = b;
                            b = help;
                        } ELSE {
                            BVec difference = a - b;
                            a = difference >> 1;
                        }
                    }
                }
            });
            shifting.onActive([&]{
                IF (d == (0_bvec).zext(4)) {
                    fsm::immediateSwitch(idle);
                } ELSE {
                    a <<= 1;
                    d -= 1_bvec;
                }
            });            
#endif
            fsm::FSM stateMachine(regConf, idle);
            result = a.delay(1);
            HCL_NAMED(result);
            done = stateMachine.isInState(idle);
            HCL_NAMED(done);

            //sim_debug() << "a is " << a.delay(1) << " and b is " << b.delay(1);
        }
        
        BVec ticks(8);
        simpleSignalGenerator(clk, [](SimpleSignalGeneratorContext &context){
            context.set(0, context.getTick());
        }, ticks);
        
        sim_assert((ticks < ConstBVec(maxTicks-1, 8)) || done) << "The state machine should be idle after " << maxTicks << " cycles";
        BVec gtruth = ConstBVec(gcd_ref(x, y), 8);
        sim_assert((ticks < ConstBVec(maxTicks-1, 8)) || (result == gtruth)) << "The state machine computed " << result << " but the correct answer is " << gtruth;
    }

    runTicks(design.getCircuit(), clk, maxTicks);
}
