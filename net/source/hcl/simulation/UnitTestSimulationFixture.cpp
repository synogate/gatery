#include "UnitTestSimulationFixture.h"

#include "ReferenceSimulator.h"

#include <boost/test/unit_test.hpp>

namespace hcl::core::sim {

UnitTestSimulationFixture::UnitTestSimulationFixture()
{
    m_simulator.reset(new ReferenceSimulator(*this));
}

UnitTestSimulationFixture::~UnitTestSimulationFixture()
{
}


void UnitTestSimulationFixture::eval(const hlim::Circuit &circuit)
{
    m_simulator->compileProgram(circuit);
    m_simulator->reset();
    //m_simulator->reevaluate();
}

void UnitTestSimulationFixture::runTicks(const hlim::Circuit &circuit, const hlim::BaseClock *clock, unsigned numTicks)
{
    m_runLimClock = 0;
    m_runLimClock = clock;
    
    m_simulator->compileProgram(circuit);
    m_simulator->reset();
    m_simulator->reevaluate();
    while (m_runLimTicks < numTicks)
        m_simulator->advanceAnyTick();
}


void UnitTestSimulationFixture::onNewTick(const hlim::BaseClock *clock)
{
    if (clock == m_runLimClock)
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
