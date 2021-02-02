#include "UnitTestSimulationFixture.h"

#include "ReferenceSimulator.h"

#include <boost/test/unit_test.hpp>

#include "../hlim/Circuit.h"

namespace hcl::core::sim {

UnitTestSimulationFixture::UnitTestSimulationFixture()
{
    m_simulator.reset(new ReferenceSimulator());
    m_simulator->addCallbacks(this);
}

UnitTestSimulationFixture::~UnitTestSimulationFixture()
{
}


void UnitTestSimulationFixture::eval(hlim::Circuit &circuit)
{
    circuit.removeFalseLoops();

    m_simulator->compileProgram(circuit);
    m_simulator->reset();
    //m_simulator->reevaluate();
}

void UnitTestSimulationFixture::runTicks(hlim::Circuit &circuit, const hlim::Clock *clock, unsigned numTicks)
{
    circuit.removeFalseLoops();
    
    m_runLimClock = 0;
    m_runLimClock = clock;
    
    m_simulator->compileProgram(circuit);
    m_simulator->reset();
    m_simulator->reevaluate();
    while (m_runLimTicks < numTicks)
        m_simulator->advanceAnyTick();
}


void UnitTestSimulationFixture::onNewTick(const hlim::ClockRational &simulationTime)
{
}

void UnitTestSimulationFixture::onClock(const hlim::Clock *clock, bool risingEdge)
{
    if (clock == m_runLimClock && risingEdge)
        m_runLimTicks++;
}

void UnitTestSimulationFixture::onDebugMessage(const hlim::BaseNode *src, std::string msg)
{
    BOOST_TEST_MESSAGE(msg);
}

void UnitTestSimulationFixture::onWarning(const hlim::BaseNode *src, std::string msg)
{
    BOOST_ERROR(msg);
}

void UnitTestSimulationFixture::onAssert(const hlim::BaseNode *src, std::string msg)
{
    BOOST_FAIL(msg);
}
    
}
