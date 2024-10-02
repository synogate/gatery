@page gtry_scl_intelQuartusTestFixture_page Intel Quartus Test Fixture

Gatery supports a test fixtures for unit tests that check processing with Intel Quartus.
After synthesis/place/fit, various results such as resource consumption and fmax can be querried and checked as part of the unit test.

## Enabling and Locating Quartus

The global fixture `gtry::IntelQuartusGlobalFixture` for boost tests can be used to query if quartus is available. 
This can be used as a predicate for unit tests and test suits:
```cpp
boost::test_tools::assertion_result canRunQuartus(boost::unit_test::test_unit_id)
{
	return gtry::scl::IntelQuartusGlobalFixture::hasIntelQuartus();
}

BOOST_AUTO_TEST_SUITE(MyTestSuite, * precondition(canRunQuartus))

// ...

BOOST_AUTO_TEST_SUITE_END()
```

To make quartus available, the location of the quartus binary folder must be specified to the test executable, either through command line arguments:
```bash
./myTest -- --intelQuartus /opt/intelFPGA_pro/23.2/quartus/bin64/
```
or through an environmental variable
```bash
export IntelQuartus_BIN_PATH=/opt/intelFPGA_pro/23.2/quartus/bin64/
./myTest
```
Note that you need to point to the bin folder within the quartus folder.


## Building Intel Quartus Unit Tests

Unit tests are build analogus to the GHDL unit tests, but with the `gtry::IntelQuartusTestFixture`.
By default, the fixture will build the design for a Cyclone10 device.

> Note that there is a bug in Quartus where synthesis for Cyclone 10 fails with internal errors if the Arria 10 device is not installed!

```cpp
BOOST_FIXTURE_TEST_CASE(myUnitTest, gtry::IntelQuartusTestFixture)
{
	using namespace gtry;

	// build circuit

	testCompilation();

	// Query various results
}
```

The device can be changed with a simple helper fixture:

```cpp 
template<class Fixture>
struct TestWithAgilex : public Fixture
{
	TestWithAgilex() {
		auto device = std::make_unique<gtry::scl::IntelDevice>();
		device->setupAgilex();
		Fixture::design.setTargetTechnology(std::move(device));
	}
};

BOOST_FIXTURE_TEST_CASE(myUnitTest, TestWithAgilex<gtry::IntelQuartusTestFixture>)
{
	// ...
}
```

## Quering and Checking Results

After compilation, just as with the GHDL test fixture, the exported vhdl files can be searched for the presence or absence of various regular expressions:

```cpp
BOOST_FIXTURE_TEST_CASE(fifoLutram, gtry::IntelQuartusTestFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = {{500'000'000, 1}} });
	ClockScope clockScope(clock);

	scl::RvStream<BVec> inputStream = { 8_b };
	pinIn(inputStream, "inputStream");

	auto outputStream = move(inputStream) | scl::strm::regDecouple() | scl::strm::fifo(8) | scl::strm::regDecouple();
	pinOut(outputStream, "outputStream");

	testCompilation();

	BOOST_TEST(exportContains(std::regex{"altera_mf.altera_mf_components.altdpram"}));
	BOOST_TEST(exportContains(std::regex{"ram_block_type => \"MLAB\""}));
	BOOST_TEST(!exportContains(std::regex{"out_conflict_bypass_mux"}));
	BOOST_TEST(exportContains(std::regex{"read_during_write_mode_mixed_ports => \"DONT_CARE\""}));
}
```

In addition, we can query the resource consumption of entities after fitting (see `gtry::IntelQuartusTestFixture::FitterResourceUtilization` for a full list of fields):

```cpp
BOOST_FIXTURE_TEST_CASE(fifoLutram, gtry::IntelQuartusTestFixture)
{
	using namespace gtry;

	// ...

	testCompilation();

	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").ALMsForMemory.inclChildren == 10.0);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").M20Ks == 0);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").ALMsNeeded.inclChildren < 20.0);
	BOOST_TEST(getFitterResourceUtilization("scl_fifo0").dedicatedLogicRegisters.inclChildren < 20.0);
}
```
The function `gtry::IntelQuartusTestFixture::getFitterResourceUtilization` takes an instance path (e.g. "my0|subinstance1|thing0"), and the path to the top entity is "|".
Note that if you use input/output registers in the top entity to constrain the timing, those will obviously turn up the in the resource consumption of the top entity.

We can also query the fmax of clocks:
```cpp
BOOST_FIXTURE_TEST_CASE(fifoLutram, gtry::IntelQuartusTestFixture)
{
	using namespace gtry;

	Clock clock({ .absoluteFrequency = {{500'000'000, 1}} });

	// ...

	testCompilation();

	BOOST_TEST(getTimingAnalysisFmax(clock).fmaxRestricted > 500.0);
	// Since usually one would want to check against the frequency of the clock, there is a shorthand for this as well:
	BOOST_TEST(timingMet(clock));
}
```

> As of now, failing timing is not a failure of the unit test by itself and needs to be checked for explicitely.