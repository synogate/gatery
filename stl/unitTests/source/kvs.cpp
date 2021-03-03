#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/simulation/waveformFormats/VCDSink.h>

#include <hcl/frontend.h>
#include <hcl/utils.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>
#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>

#include <random>

#include <hcl/stl/kvs/TinyCuckoo.h>
#include <hcl/stl/kvs/TinyCuckooCore.h>

using namespace boost::unit_test;
using namespace hcl;


BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, TinyCookuTableLookup, data::xrange(2, 4), numTables)
{
    DesignScope design;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000));
    ClockScope clockScope(clock);

    const BitWidth keySize{ size_t(numTables * 4) };
    const BitWidth tableIdxWidth{ hcl::utils::Log2C(size_t(numTables)) };

    InputPins lookupKey = pinIn(keySize).setName("key");
    InputPin update = pinIn().setName("update");
    InputPins updateTableIdx = pinIn(tableIdxWidth).setName("updateTableIdx");
    InputPins updateItemIdx = pinIn(4_b).setName("updateItemIdx");
    InputPin updateItemValid = pinIn().setName("updateItemValid");
    InputPins updateItemKey = pinIn(keySize).setName("updateItemKey");
    InputPins updateItemValue = pinIn(8_b).setName("updateItemValue");
    
    stl::TinyCuckooIn params = {
        .key = lookupKey,
        .hash = lookupKey,
        .userData = 0,
        .update = {
            .valid = update,
            .tableIdx = updateTableIdx,
            .itemIdx = updateItemIdx,
            .item = {
                .valid = updateItemValid,
                .key = updateItemKey,
                .value = updateItemValue
            }
        },
        .numTables = size_t(numTables),
    };
    HCL_NAMED(params);
    stl::TinyCuckooOut result = stl::tinyCuckoo(params);

    OutputPin outFound = pinOut(result.found).setName("found");
    OutputPins outValue = pinOut(result.value).setName("value");

    const size_t invalid = std::numeric_limits<size_t>::max();
    std::vector<std::vector<std::pair<size_t, size_t>>> state{
        params.numTables,
        std::vector<std::pair<size_t, size_t>>{
            1ull << params.tableWidth().value,
            std::make_pair(invalid, invalid)
        }
    };

    addSimulationProcess([&]()->SimProcess {
        std::mt19937 rng{ 1337 };
        sim(update) = '0';

        while(true)
        {
            size_t value = rng() & 0xFF;
            size_t key = hcl::utils::bitfieldExtract(value * 23, 0, keySize.value);

            if (rng() % 3 == 0)
            {
                const size_t tableIdx = key % params.numTables;
                const size_t itemIdx = (key >> (tableIdx * 4)) & 0xF;

                sim(update) = '1';
                sim(updateItemKey) = key;
                sim(updateItemValue) = value;
                sim(updateTableIdx) = tableIdx;
                sim(updateItemIdx) = itemIdx;

                if (rng() % 5 == 0)
                {
                    sim(updateItemValid) = '0';
                    state[tableIdx][itemIdx] = std::make_pair(invalid, invalid);
                }
                else
                {
                    sim(updateItemValid) = '1';
                    state[tableIdx][itemIdx] = std::make_pair(key, value);
                }
            }

            co_await WaitClk(clock);
            sim(update) = '0';
        }
    });

    addSimulationProcess([&]()->SimProcess {
        std::mt19937 rng{ 1338 };
        while (true)
        {
            sim(lookupKey) = hcl::utils::bitfieldExtract(rng(), 0, keySize.value);
            co_await WaitClk(clock);
        }
    });

    addSimulationProcess([&]()->SimProcess {

        std::deque<size_t> lookupQueue;

        while (true)
        {
            if (lookupQueue.size() == params.latency)
            {
                if (sim(outFound))
                {
                    BOOST_TEST(lookupQueue.back() == sim(outValue));
                }
                else
                    BOOST_TEST(lookupQueue.back() == invalid);
                lookupQueue.pop_back();
            }
            else
            {
                BOOST_TEST(sim(outFound) == 0);
            }

            const size_t key = sim(lookupKey);
            size_t tableKey = key;
            size_t value = invalid;
            for (const auto& t : state)
            {
                const auto& item = t[tableKey & 0xF];
                tableKey >>= 4;
                if (item.first == key)
                    value = item.second;
            }
            lookupQueue.push_front(value);
            co_await WaitClk(clock);
        }
    });


    design.getCircuit().optimize(3);
    //design.visualize("TinyCookuTableLookup");
    //core::sim::VCDSink vcd(design.getCircuit(), getSimulator(), "TinyCookuTableLookup.vcd");
    //vcd.addAllNamedSignals();

    runTicks(design.getCircuit(), clock.getClk(), 4096);
}
#if 0
#include <hcl/export/vhdl/VHDLExport.h>

BOOST_FIXTURE_TEST_CASE(TinyCuckooCoreTest, hcl::core::sim::UnitTestSimulationFixture)
{
    DesignScope design;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000));
    ClockScope clockScope(clock);

    stl::TinyCuckooCore generator(1024, 16_b, 16_b);
    stl::TinyCuckooCoreInterface cuckoo = generator();
    stl::AvalonMMSlave ctrl(4_b, 32_b);
    generator.connectMemoryMap(cuckoo, ctrl);
    pinIn(ctrl, "ctrl_");

    cuckoo.lookupOut.ready = '1';
    cuckoo.lookupIn.valid = '1';

    InputPins lookupKey = pinIn(cuckoo.lookupIn.value().getWidth()).setName("key");
    cuckoo.lookupIn.value() = lookupKey;
    HCL_NAMED(cuckoo);

    OutputPin outFound = pinOut(cuckoo.lookupOut.value().found).setName("found");
    OutputPins outValue = pinOut(cuckoo.lookupOut.value().value).setName("value");

    addSimulationProcess([&]()->SimProcess {
        std::mt19937 rng{ 1337 };
        sim(ctrl.write) = '0';

        while (true)
        {
            size_t value = rng() & 0xFF;
            size_t key = hcl::utils::bitfieldExtract(value * 23, 0, 10);


            co_await WaitClk(clock);
            sim(ctrl.write) = '0';
        }
        });

    addSimulationProcess([&]()->SimProcess {
        std::mt19937 rng{ 1338 };
        while (true)
        {
            sim(lookupKey) = hcl::utils::bitfieldExtract(rng(), 0, 10);
            co_await WaitClk(clock);
        }
        });


    design.getCircuit().optimize(3);
    design.visualize("cuckoo");
    core::sim::VCDSink vcd(design.getCircuit(), getSimulator(), "cuckoo.vcd");
    vcd.addAllNamedSignals();

    hcl::core::vhdl::VHDLExport exp{ "cuckoo/" };
    exp(design.getCircuit());

    runTicks(design.getCircuit(), clock.getClk(), 4096);
}
#endif
