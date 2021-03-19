#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/simulation/waveformFormats/VCDSink.h>

#include <hcl/frontend.h>
#include <hcl/utils.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>
#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>

#include <hcl/export/vhdl/VHDLExport.h>

#include <random>

#include <hcl/stl/kvs/TinyCuckoo.h>

extern "C"
{
#include <hcl/stl/kvs/TinyCuckooDriver.h>
}

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
                    BOOST_TEST(lookupQueue.back() == sim(outValue));
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


BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, TinyCuckooTableLookup, data::xrange(3, 4), numTables)
{
    DesignScope design;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000));
    ClockScope clockScope(clock);

    const BitWidth keySize{ size_t(numTables * 10) };
    InputPins lookupKey = pinIn(keySize).setName("key");

    stl::TinyCuckoo<BVec, BVec> tc{ size_t(numTables * 1024), keySize, 4_b, size_t(numTables) };
    BOOST_TEST(keySize.value == tc.hashWidth().value);

    auto cuckooPoo = tc(lookupKey, lookupKey);
    cuckooPoo = reg(cuckooPoo);
    pinOut(cuckooPoo.found).setName("out_found");
    pinOut(cuckooPoo.value).setName("out_value");

    stl::AvalonNetworkSection net;
    tc.addCpuInterface(net);
    net.assignPins();
    

    //design.visualize("TinyCookuTableLookup_before");
    //design.getCircuit().optimize(3);
    //design.visualize("TinyCookuTableLookup");
    //core::sim::VCDSink vcd(design.getCircuit(), getSimulator(), "TinyCookuTableLookup.vcd");
    //vcd.addAllNamedSignals();

    //hcl::core::vhdl::VHDLExport vhdl("vhdl/");
    //vhdl(design.getCircuit());

    runTicks(design.getCircuit(), clock.getClk(), 4096);
}

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, TinyCuckooTableLookupDemuxed, data::xrange(3, 4), numTables)
{
    DesignScope design;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(100'000'000));
    ClockScope clockScope(clock);

    const BitWidth keySize{ size_t(numTables * 10) };
    InputPins lookupKey = pinIn(keySize).setName("key");

    stl::TinyCuckoo<BVec, BVec> tc{ size_t(numTables * 1024), keySize, 4_b, size_t(numTables) };
    BOOST_TEST(keySize.value == tc.hashWidth().value);

    auto cuckooPoo = tc(lookupKey, lookupKey);
    cuckooPoo = reg(cuckooPoo);
    pinOut(cuckooPoo.found).setName("out_found");
    pinOut(cuckooPoo.value).setName("out_value");

    stl::AvalonNetworkSection net;
    tc.addCpuInterface(net);
    stl::AvalonMM ctrl = net.demux();
    net.clear();

    ctrl.pinIn("ctrl");


    design.visualize("TinyCuckooTableLookupDemuxed_before");
    design.getCircuit().optimize(3);
    design.visualize("TinyCuckooTableLookupDemuxed");
    core::sim::VCDSink vcd(design.getCircuit(), getSimulator(), "TinyCuckooTableLookupDemuxed.vcd");
    vcd.addAllNamedSignals();

    //hcl::core::vhdl::VHDLExport vhdl("vhdl/");
    //vhdl(design.getCircuit());

    runTicks(design.getCircuit(), clock.getClk(), 4096);
}

extern "C"
{
    static void super_free(void* ptr, size_t)
    {
        free(ptr);
    }

    static void super_hash(void* ctx, uint32_t* key, uint32_t* hash)
    {
        const uint32_t k = *key;
        hash[0] = k * 609598081u;
        hash[1] = k * 1067102063u;
        hash[2] = k * 190989923u;
        hash[3] = k * 905010023u;
        hash[4] = k * 2370688493u;
        hash[5] = k * 3059132147u;
        hash[6] = k * 1500458227u;
        hash[7] = k * 1781057147u;
    }
}



BOOST_AUTO_TEST_CASE(TinyCuckooDriverBaseTest)
{
    TinyCuckooContext* ctx = tiny_cuckoo_init(32 * 1024, 4, 32, 32, malloc, super_free);
    BOOST_TEST(ctx);

    tiny_cuckoo_set_hash(ctx, super_hash, NULL);

    uint32_t testKey = 128;
    uint32_t testVal = 1337;
    BOOST_TEST(!tiny_cuckoo_lookup(ctx, &testKey));

    BOOST_TEST(tiny_cuckoo_update(ctx, &testKey, &testVal));

    uint32_t* lookupVal = tiny_cuckoo_lookup(ctx, &testKey);
    BOOST_TEST(lookupVal);
    BOOST_TEST(*lookupVal == testVal);

    testVal = ~testVal;
    BOOST_TEST(tiny_cuckoo_update(ctx, &testKey, &testVal));

    lookupVal = tiny_cuckoo_lookup(ctx, &testKey);
    BOOST_TEST(lookupVal);
    BOOST_TEST(*lookupVal == testVal);

    BOOST_TEST(tiny_cuckoo_remove(ctx, &testKey));
    BOOST_TEST(!tiny_cuckoo_remove(ctx, &testKey));
    BOOST_TEST(!tiny_cuckoo_lookup(ctx, &testKey));

    tiny_cuckoo_destroy(ctx);
}

BOOST_DATA_TEST_CASE(TinyCuckooDriverFuzzTest, data::xrange(0, 3), tableShift)
{
    const uint32_t numTables = 2 << tableShift;
    TinyCuckooContext* ctx = tiny_cuckoo_init(64 * 1024, numTables, 32, 32, malloc, super_free);
    BOOST_TEST(ctx);
    tiny_cuckoo_set_hash(ctx, super_hash, NULL);

    std::map<uint32_t, uint32_t> ref;
    const auto seed = std::random_device{}();
    std::mt19937 rng{ seed };

    for (size_t i = 0;; ++i)
    {
        uint32_t key = rng() & 0xFFFFF;
        uint32_t val = rng();

        uint32_t* uut = tiny_cuckoo_lookup(ctx, &key);
        auto it_ref = ref.find(key);
        if (it_ref == ref.end())
        {
            BOOST_TEST(uut == nullptr, "seed: " << seed);
        }
        else
        {
            BOOST_TEST(uut, "seed: " << seed);
            BOOST_TEST(*uut = it_ref->second, "seed: " << seed);
        }

        if (!tiny_cuckoo_update(ctx, &key, &val))
            break;
        ref[key] = val;
    }

    BOOST_TEST(ref.size() > ctx->capacity / 3, "reached only " << ref.size() / double(ctx->capacity) << " of capacity using seed: " << seed);

    const uint32_t* item_end = ctx->items + ctx->capacity * ctx->itemWords;
    for (uint32_t* item = ctx->items; item != item_end; item += ctx->itemWords)
    {
        if (item[0])
        {
            auto it_ref = ref.find(item[1]);
            bool is_end = it_ref == ref.end();
            BOOST_TEST(!is_end);
            if (!is_end)
            {
                BOOST_TEST(it_ref->second == item[2]);
                ref.erase(it_ref);
            }
        }
    }
    BOOST_TEST(ref.empty());

    tiny_cuckoo_destroy(ctx);
}
