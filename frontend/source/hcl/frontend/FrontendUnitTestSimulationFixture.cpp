#include "FrontendUnitTestSimulationFixture.h"

#include <hcl/simulation/Simulator.h>
#include <hcl/simulation/waveformFormats/VCDSink.h>
#include <hcl/export/vhdl/VHDLExport.h>

#include <boost/test/unit_test.hpp>

namespace hcl::core::frontend {

UnitTestSimulationFixture::~UnitTestSimulationFixture()
{
    // Force destruct of simulator (and all frontend signals held inside coroutines) before destruction of DesignScope in base class.
    m_simulator.reset(nullptr);
}

void UnitTestSimulationFixture::eval()
{
    sim::UnitTestSimulationFixture::eval(design.getCircuit());
}

void UnitTestSimulationFixture::runTicks(const hlim::Clock* clock, unsigned numTicks)
{
    sim::UnitTestSimulationFixture::runTicks(design.getCircuit(), clock, numTicks);
}


void UnitTestSimulationFixture::recordVCD(const std::filesystem::path &destination)
{
    m_vcdSink.emplace(design.getCircuit(), *m_simulator, destination.string().c_str());
    m_vcdSink->addAllOutPins();
    m_vcdSink->addAllWatchSignalTaps();
    m_vcdSink->addAllSignals();
}

void UnitTestSimulationFixture::outputVHDL(const std::filesystem::path &destination, bool includeTest)
{
    m_vhdlExport.emplace(destination);
    (*m_vhdlExport)(design.getCircuit());

    if (includeTest) {
        m_vhdlExport->recordTestbench(*m_simulator, "testbench");
        m_vhdlExport->writeGHDLScript("runGHDL.sh");
    }
}

void UnitTestSimulationFixture::stopTest()
{
    m_simulator->abort();
    m_stopTestCalled = true;
}

bool UnitTestSimulationFixture::runHitsTimeout(const hlim::ClockRational &timeoutSeconds)
{
    m_stopTestCalled = false;
    m_simulator->compileProgram(design.getCircuit());
    m_simulator->powerOn();
    m_simulator->advance(timeoutSeconds);
    return !m_stopTestCalled;
}


void BoostUnitTestGlobalFixture::setup()
{
    auto &testSuite = boost::unit_test::framework::master_test_suite();
    for (unsigned i = 1; i < testSuite.argc; i++) {
        std::string arg(testSuite.argv[i]);
        if (arg == "--vcd") {
            if (i+1 < testSuite.argc)
                vcdPath = testSuite.argv[++i];
            else
                BOOST_CHECK_MESSAGE(false, "Missing command line argument");
        } else
        if (arg == "--vhdl") {
            if (i+1 < testSuite.argc)
                vhdlPath = testSuite.argv[++i];
            else
                BOOST_CHECK_MESSAGE(false, "Missing command line argument");
        } else
        if (arg == "--graph-vis") {
            if (i+1 < testSuite.argc)
                graphVisPath = testSuite.argv[++i];
            else
                BOOST_CHECK_MESSAGE(false, "Missing command line argument");
        } else
            BOOST_CHECK_MESSAGE(false, std::string("Unknown argument: ") + arg);
    }
}


std::filesystem::path BoostUnitTestGlobalFixture::graphVisPath;
std::filesystem::path BoostUnitTestGlobalFixture::vcdPath;
std::filesystem::path BoostUnitTestGlobalFixture::vhdlPath;


void BoostUnitTestSimulationFixture::runFixedLengthTest(const hlim::ClockRational &seconds)
{
    prepRun();
    runHitsTimeout(seconds);
}

void BoostUnitTestSimulationFixture::runEvalOnlyTest()
{
    prepRun();
    eval();
}

void BoostUnitTestSimulationFixture::runTest(const hlim::ClockRational &timeoutSeconds)
{
    prepRun();
    BOOST_CHECK_MESSAGE(!runHitsTimeout(timeoutSeconds), "Simulation timed out without being called to a stop by any simulation process!");
}


void BoostUnitTestSimulationFixture::prepRun()
{
    if (!BoostUnitTestGlobalFixture::graphVisPath.empty())
        design.visualize(BoostUnitTestGlobalFixture::graphVisPath.string());

    if (!BoostUnitTestGlobalFixture::vhdlPath.empty())
        outputVHDL(BoostUnitTestGlobalFixture::vhdlPath);

    if (!BoostUnitTestGlobalFixture::vcdPath.empty())
        recordVCD(BoostUnitTestGlobalFixture::vcdPath);
}



}
