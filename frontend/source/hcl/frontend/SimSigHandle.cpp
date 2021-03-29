#include "SimSigHandle.h"

#include <hcl/simulation/SimulationContext.h>
#include <hcl/simulation/Simulator.h>
#include <hcl/hlim/ClockRational.h>

#include "Clock.h"

namespace hcl::core::frontend {


hlim::Node_Pin* findInputPin(hlim::NodePort driver)
{
    hlim::Node_Pin* res;
    if (res = dynamic_cast<hlim::Node_Pin*>(driver.node))
        return res;

    HCL_DESIGNCHECK(driver.node != nullptr);
    if (dynamic_cast<hlim::Node_Signal*>(driver.node) == nullptr)
        return nullptr;

    for (auto nh : driver.node->exploreInput(0)) {
        if (res = dynamic_cast<hlim::Node_Pin*>(nh.node()))
            return res;
        else
            if (!nh.isSignal())
                nh.backtrack();
    }
    return nullptr;
}


hlim::Node_Pin *findOutputPin(hlim::NodePort driver)
{
    hlim::Node_Pin *res;
    if (res = dynamic_cast<hlim::Node_Pin*>(driver.node))
        return res;

    HCL_DESIGNCHECK(driver.node != nullptr);

    for (auto nh : driver.node->exploreOutput(driver.port)) {
        if (res = dynamic_cast<hlim::Node_Pin*>(nh.node()))
            return res;
        else
            if (!nh.isSignal())
                nh.backtrack();
    }
    
    return nullptr;
}

sim::SigHandle sim(hlim::NodePort output)
{
    return sim::SigHandle(output);
}

sim::SigHandle sim(const Bit &bit)
{
    auto driver = bit.getReadPort();
    HCL_DESIGNCHECK(driver.node != nullptr);

    hlim::Node_Pin* pin = findInputPin(driver);
    if (pin)
        return sim({ .node = pin, .port = 0ull });

    pin = findOutputPin(driver);
    if (pin)
        return sim(pin->getDriver(0));

    HCL_DESIGNCHECK_HINT(false, "Found neither input nor output pin associated with signal");
}

sim::SigHandle sim(const BVec &signal)
{
    auto driver = signal.getReadPort();
    HCL_DESIGNCHECK(driver.node != nullptr);
        
    hlim::Node_Pin* pin = findInputPin(driver);
    if (pin)
        return sim({ .node = pin, .port = 0ull });

    pin = findOutputPin(driver);
    if (pin)
        return sim(pin->getDriver(0));

    HCL_DESIGNCHECK_HINT(false, "Found neither input nor output pin associated with signal");
}

sim::SigHandle sim(const InputPin &pin)
{
    return sim({.node=pin.getNode(), .port=0ull});
}

sim::SigHandle sim(const InputPins &pins)
{
    return sim({.node=pins.getNode(), .port=0ull});
}

sim::SigHandle sim(const OutputPin &pin)
{
    auto driver = pin.getNode()->getDriver(0);
    HCL_DESIGNCHECK_HINT(driver.node != nullptr, "Can't read unbound output pin!");
    return sim(driver);
}

sim::SigHandle sim(const OutputPins &pins)
{
    auto driver = pins.getNode()->getDriver(0);
    HCL_DESIGNCHECK_HINT(driver.node != nullptr, "Can't read unbound output pin!");
    return sim(driver);
}

sim::WaitClock WaitClk(const Clock &clk)
{
    return sim::WaitClock(clk.getClk());
}

void simAnnotationStart(const std::string &id, const std::string &desc)
{
    auto *sim = sim::SimulationContext::current()->getSimulator();
    HCL_DESIGNCHECK_HINT(sim, "Can only annotate if running an actual simulation!");
    sim->annotationStart(sim->getCurrentSimulationTime(), id, desc);
}

void simAnnotationStartDelayed(const std::string &id, const std::string &desc, const Clock &clk, int cycles)
{
    auto *sim = sim::SimulationContext::current()->getSimulator();
    HCL_DESIGNCHECK_HINT(sim, "Can only annotate if running an actual simulation!");

    auto shift = (unsigned) std::abs(cycles) / clk.getAbsoluteFrequency();

    if (cycles > 0)
        sim->annotationStart(sim->getCurrentSimulationTime() + shift, id, desc);
    else
        sim->annotationStart(sim->getCurrentSimulationTime() - shift, id, desc);
}

void simAnnotationEnd(const std::string &id)
{
    auto *sim = sim::SimulationContext::current()->getSimulator();
    HCL_DESIGNCHECK_HINT(sim, "Can only annotate if running an actual simulation!");
    sim->annotationEnd(sim->getCurrentSimulationTime(), id);
}

void simAnnotationEndDelayed(const std::string &id, const Clock &clk, int cycles)
{
    auto *sim = sim::SimulationContext::current()->getSimulator();
    HCL_DESIGNCHECK_HINT(sim, "Can only annotate if running an actual simulation!");

    auto shift = (unsigned) std::abs(cycles) / clk.getAbsoluteFrequency();

    if (cycles > 0)
        sim->annotationEnd(sim->getCurrentSimulationTime() + shift, id);
    else
        sim->annotationEnd(sim->getCurrentSimulationTime() - shift, id);
}



}
