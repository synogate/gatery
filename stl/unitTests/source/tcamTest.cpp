#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/stl/hardCores/AsyncRam.h>
#include <hcl/stl/hardCores/BlockRam.h>
#include <hcl/stl/hardCores/blockRam/XilinxSimpleDualPortBlockRam.h>
#include <hcl/stl/kvs/TCAM.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>
#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>
#include <hcl/export/vhdl/VHDLExport.h>

using namespace boost::unit_test;

BOOST_FIXTURE_TEST_CASE(ramSimpleDualPortAsyncTest, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::stl;
    using namespace hcl::core::frontend;
    DesignScope design;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope scope(clock);

    BVec counter = 6_b;
    counter += 1;
    counter = reg(counter, "6b0");

    BVec address = counter(0, -1);
    BVec data = ~address;
    Bit update = !counter.msb();

    Ram<BVec> ram(32, 5_b);

    IF(update)
        ram[address] = data;

    sim_assert(update | ram[address] == data) << ram[address].read() << " should be " << data << " phase " << update;

    runTicks(design.getCircuit(), clock.getClk(), 64);
}

BOOST_FIXTURE_TEST_CASE(ramDualPortAccessOrderTest, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::stl;
    using namespace hcl::core::frontend;
    DesignScope design;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope scope(clock);

    BVec counter = 8_b;
    counter += 1;
    counter = reg(counter, "8b0");

    BVec addr0 = counter(0, -1);
    BVec addr1 = addr0;

    Ram<BVec> ram(2, 8_b);

    BVec read00 = ram[addr0]; // async read of old ram content
    BVec read10 = ram[addr1];

    ram[reg(addr0)] = counter;
    BVec read01 = ram[addr0]; // should read counter
    BVec read11 = ram[addr1];

    sim_debug() << read00 << ", " << read10 << ", " << read01 << ", " << read11;
    
    runTicks(design.getCircuit(), clock.getClk(), 8);
}


BOOST_FIXTURE_TEST_CASE(asyncMemoryTest, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::stl;
    using namespace hcl::core::frontend;
    DesignScope design;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope scope(clock);

    BVec counter = 6_b;
    counter = reg(counter, "6b0");
    
    AvalonMM ram(5, 20);

    ram.address = counter(0, 5);
    ram.read = '1';
    ram.write = !counter[5];
    ram.writeData = zext(counter);
    asyncRam(ram);

    sim_assert(!counter[5] | ram.readData == zext(counter(0, 5))) << ram.readData << " should be " << counter(0, 5) << " phase " << counter[5];
    sim_assert(!counter[5] | ram.readDataValid) << ram.readDataValid << " should be 1";

    counter += 1;
    runTicks(design.getCircuit(), clock.getClk(), 64);
}

BOOST_FIXTURE_TEST_CASE(TCAMCellTest, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::stl;
    using namespace hcl::core::frontend;
    DesignScope design;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope scope(clock);

    BVec counter = 6_b;
    counter = reg(counter, "6b0");

    BVec searchKey = 10_b;
    searchKey = zext(counter(0, 5));

    std::vector<BVec> updateData(20, 2_b);
    for (size_t i = 0; i < updateData.size(); ++i)
    { // bit 0-4 have to match its item index and 5-9 have to match 0
        updateData[i] = cat(counter == 0, counter == i);
    }
    
    IF(!counter[5]) // update mode
        searchKey(5, 5) = counter(0, 5);

    BVec match = constructTCAMCell(searchKey, !counter[5], updateData);

    BVec matchRef = ConstBVec(20);
    for (size_t i = 0; i < matchRef.size(); ++i)
        matchRef[i] = counter(0, 5) == i;
    BOOST_CHECK(match.size() == matchRef.size());

    counter += 1;
    runTicks(design.getCircuit(), clock.getClk(), 64);
}

BOOST_FIXTURE_TEST_CASE(xilinxBramTest, hcl::core::sim::UnitTestSimulationFixture)
{
    using namespace hcl::stl;
    using namespace hcl::core::frontend;
    DesignScope design;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope scope(clock);

    BVec counter = 6_b;
    counter = reg(counter, "6b0");
    HCL_NAMED(counter);

    Stream<WritePort> wr(5, 20);
    Stream<BVec> rd = 5_b;
    Stream<BVec> rdData = simpleDualPortRam(wr, rd, "test");

    rd.value() = counter(0, 5);
    rd.valid = '1';
    wr.address = counter(0, 5);
    wr.writeData = zext(counter);
    wr.valid = !counter[5];

    BVec lastCounter = reg(counter, "6b0");
    sim_assert(!lastCounter[5] | rdData == zext(lastCounter(0, 5))) << rdData << " should be " << lastCounter(0, 5) << " phase " << lastCounter[5];

    counter += 1;
    runTicks(design.getCircuit(), clock.getClk(), 64);

    //hcl::core::vhdl::VHDLExport vhdl("bram_test");

    //auto* formatter = dynamic_cast<hcl::core::vhdl::DefaultCodeFormatting*>(vhdl.getFormatting());
    //formatter->addExternalNodeHandler(blockram::XilinxSimpleDualPortBlockRam::writeIntelVHDL);

    //vhdl(design.getCircuit());
}
