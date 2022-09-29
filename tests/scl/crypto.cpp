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
#include "scl/pch.h"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

extern "C"
{
#include <gatery/scl/crypto/TabulationHashingDriver.h>
}

using namespace boost::unit_test;
using namespace gtry;

BOOST_FIXTURE_TEST_CASE(Sha1RoundA, gtry::BoostUnitTestSimulationFixture)
{
	// create padded empty input
	UInt msgBlock = "512x0";
	msgBlock.msb() = '1';

	scl::Sha1Generator<> sha1, sha1ref;
	sha1.init();
	sha1ref.init();

	sha1.beginBlock(msgBlock);
	sim_assert(sha1.w[0] == "x80000000") << "w0";

	sha1.round(0);

	uint32_t h1 = 0xEFCDAB89;
	uint32_t h2 = 0x98BADCFE;
	uint32_t h3 = 0x10325476;
	uint32_t f = (h1 & h2) | (~h1 & h3);
	uint32_t k = 0x5A827999;

	UInt checkA = rotl(sha1ref.a, 5) + sha1ref.e + 0x80000000u + k + f;
	sim_assert(sha1.a == checkA) << "a wrong " << sha1.a << " != " << checkA;
	sim_assert(sha1.b == sha1ref.a) << "b " << sha1.b << " != " << sha1ref.a;
	sim_assert(sha1.c == rotl(sha1ref.b, 30));
	sim_assert(sha1.d == sha1ref.c);
	sim_assert(sha1.e == sha1ref.d);

	eval();
}

BOOST_FIXTURE_TEST_CASE(Sha1RoundB, gtry::BoostUnitTestSimulationFixture)
{
	// create padded empty input
	UInt msgBlock = "512x0";
	msgBlock.msb() = '1';

	scl::Sha1Generator<> sha1, sha1ref;
	sha1.init();
	sha1ref.init();

	sha1.beginBlock(msgBlock);
	sim_assert(sha1.w[0] == "x80000000") << "w0";

	sha1.round(20);

	uint32_t h1 = 0xEFCDAB89;
	uint32_t h2 = 0x98BADCFE;
	uint32_t h3 = 0x10325476;
	uint32_t f = h1 ^ h2 ^ h3;
	uint32_t k = 0x6ED9EBA1;

	UInt checkA = rotl(sha1ref.a, 5) + sha1ref.e + 0x80000000u + k + f;
	sim_assert(sha1.a == checkA) << "a wrong " << sha1.a << " != " << checkA;
	sim_assert(sha1.b == sha1ref.a) << "b " << sha1.b << " != " << sha1ref.a;
	sim_assert(sha1.c == rotl(sha1ref.b, 30));
	sim_assert(sha1.d == sha1ref.c);
	sim_assert(sha1.e == sha1ref.d);

	eval();
}

BOOST_FIXTURE_TEST_CASE(Sha1RoundC, gtry::BoostUnitTestSimulationFixture)
{
	// create padded empty input
	UInt msgBlock = "512x0";
	msgBlock.msb() = '1';

	scl::Sha1Generator<> sha1, sha1ref;
	sha1.init();
	sha1ref.init();

	sha1.beginBlock(msgBlock);
	sim_assert(sha1.w[0] == "x80000000") << "w0";

	sha1.round(40);

	uint32_t b = 0xEFCDAB89;
	uint32_t c = 0x98BADCFE;
	uint32_t d = 0x10325476;
	uint32_t f = (b & c) | (b & d) | (c & d);
	uint32_t k = 0x8F1BBCDC;

	UInt checkA = rotl(sha1ref.a, 5) + sha1ref.e + 0x80000000u + k + f;
	sim_assert(sha1.a == checkA) << "a wrong " << sha1.a << " != " << checkA;
	sim_assert(sha1.b == sha1ref.a) << "b " << sha1.b << " != " << sha1ref.a;
	sim_assert(sha1.c == rotl(sha1ref.b, 30));
	sim_assert(sha1.d == sha1ref.c);
	sim_assert(sha1.e == sha1ref.d);

	eval();
}

BOOST_FIXTURE_TEST_CASE(Sha1RoundD, gtry::BoostUnitTestSimulationFixture)
{
	// create padded empty input
	UInt msgBlock = "512x0";
	msgBlock.msb() = '1';

	scl::Sha1Generator<> sha1, sha1ref;
	sha1.init();
	sha1ref.init();

	sha1.beginBlock(msgBlock);
	sim_assert(sha1.w[0] == "x80000000") << "w0";

	sha1.round(60);

	uint32_t h1 = 0xEFCDAB89;
	uint32_t h2 = 0x98BADCFE;
	uint32_t h3 = 0x10325476;
	uint32_t f = h1 ^ h2 ^ h3;
	uint32_t k = 0xCA62C1D6;

	UInt checkA = rotl(sha1ref.a, 5) + sha1ref.e + 0x80000000u + k + f;
	sim_assert(sha1.a == checkA) << "a wrong " << sha1.a << " != " << checkA;
	sim_assert(sha1.b == sha1ref.a) << "b " << sha1.b << " != " << sha1ref.a;
	sim_assert(sha1.c == rotl(sha1ref.b, 30));
	sim_assert(sha1.d == sha1ref.c);
	sim_assert(sha1.e == sha1ref.d);

	eval();
}

BOOST_FIXTURE_TEST_CASE(Sha1, gtry::BoostUnitTestSimulationFixture)
{
	// create padded empty input
	UInt msgBlock = "512x0";
	msgBlock.msb() = '1';

	scl::Sha1Generator<> sha1;
	sha1.init();

	sha1.beginBlock(msgBlock);
	scl::HashEngine<scl::Sha1Generator<>> sha1Engine(0, 0);
	sha1Engine.buildPipeline(sha1);
	sha1.endBlock();

	UInt hash = sha1.finalize();
	UInt ref = "xDA39A3EE5E6B4B0D3255BFEF95601890AFD80709";

	sim_assert(hash(0, 64_b) == ref(0, 64_b)); // TODO: implement large compare simulation
	sim_assert(hash(64, 64_b) == ref(64, 64_b));
	sim_assert(hash(128, 32_b) == ref(128, 32_b));

	eval();
}

BOOST_FIXTURE_TEST_CASE(Sha2_256, gtry::BoostUnitTestSimulationFixture)
{
	// create padded empty input
	UInt msgBlock = "512x0";
	msgBlock.msb() = '1';

	scl::Sha2_256<> sha2;
	sha2.init();

	sha2.beginBlock(msgBlock);
	scl::HashEngine<scl::Sha2_256<>> sha2Engine(0, 0);
	sha2Engine.buildPipeline(sha2);
	sha2.endBlock();

	UInt hash = sha2.finalize();
	UInt ref = "xE3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855";

	sim_assert(hash(0, 64_b) == ref(0, 64_b)); // TODO: implement large compare simulation
	sim_assert(hash(64, 64_b) == ref(64, 64_b));
	sim_assert(hash(128, 32_b) == ref(128, 32_b));
	sim_assert(hash(160, 32_b) == ref(160, 32_b));

	eval();
}

BOOST_FIXTURE_TEST_CASE(Md5, gtry::BoostUnitTestSimulationFixture)
{
	struct md5ref
	{
		uint32_t a = 0x67452301;
		uint32_t b = 0xefcdab89;
		uint32_t c = 0x98badcfe;
		uint32_t d = 0x10325476;

		uint32_t w[16] = { 0x80 };

		md5ref round(uint32_t idx) const
		{
			md5ref ret;

			const uint32_t K = uint32_t(pow(2., 32.) * abs(sin(idx + 1)));

			const uint32_t S_table[4][4] = { {7, 12, 17, 22}, {5, 9, 14, 20}, {4, 11, 16, 23}, {6, 10, 15, 21} };
			const uint32_t S = S_table[idx / 16][idx % 4];

			const uint32_t Wmul[4] = { 1, 5, 3, 7 };
			const uint32_t Wadd[4] = { 0, 1, 5, 0 };
			const uint32_t W = Wmul[idx / 16] * idx + Wadd[idx / 16];

			uint32_t F;
			switch (idx / 16) {
			case 0: F = (b & c) | (~b & d); break;
			case 1: F = (d & b) | (~d & c); break;
			case 2: F = b ^ c ^ d;		  break;
			default:F = c ^ (b | ~d);
			}

			uint32_t tmp = F + a + K + w[W % 16];
			tmp = (tmp << S) | (tmp >> (32 - S));

			ret.a = d;
			ret.b = tmp + b;
			ret.c = b;
			ret.d = c;
			return ret;
		}
	};

	// create padded empty input
	UInt msgBlock = "512x0";
	msgBlock.msb() = '1';
	scl::Md5Generator<> md5;
	md5.beginBlock(msgBlock);

	md5ref refImpl;
	for (size_t i = 0; i < 64; ++i)
	{
		md5.round(i);
		refImpl = refImpl.round(uint32_t(i));

		sim_assert(md5.a == refImpl.a) << "a in round " << i;
		sim_assert(md5.b == refImpl.b) << "b in round " << i;
		sim_assert(md5.c == refImpl.c) << "c in round " << i;
		sim_assert(md5.d == refImpl.d) << "d in round " << i;
	}

	BOOST_TEST(refImpl.a + 0x67452301 == 0xd98c1dd4);
	md5.endBlock();

	UInt hash = md5.finalize();
	UInt ref = "xD41D8CD98F00B204E9800998ECF8427E";

	sim_assert(hash(0, 64_b) == ref(0, 64_b)) << hash << " != " << ref; // TODO: implement large compare simulation
	sim_assert(hash(64, 64_b) == ref(64, 64_b)) << hash << " != " << ref;

	eval();
}

#if 0
#include <gatery/vis/MainWindowSimulate.h>
#include <QApplication>

#include <locale.h>

BOOST_AUTO_TEST_CASE(SipHash64TestVisual)
{
	setlocale(LC_ALL, "en_US.UTF-8");

	DesignScope design;


	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope clockScope(clock);

	scl::SipHash sip(2, 4);
	sip.enableRegister(true);

	scl::SipHashState state;

	UInt key = "x0F0E0D0C0B0A09080706050403020100";
	sip.initialize(state, key);

	InputPins msg = pinIn(64_b).setName("msg");
	sip.block(state, msg);
	OutputPins hash = pinOut(sip.finalize(state)).setName("hash");

	design.postprocess();

	char str[] = "datProgramm";
	char* bla[] = { str };
	int one = 1;
	QApplication a(one, bla);

	gtry::vis::MainWindowSimulate w(nullptr, design.getCircuit());

	w.getSimulator().addSimulationProcess([&]()->SimProcess {

		const uint64_t test_vector_sip64[] = {
			0x726fdb47dd0e0e31, 0x74f839c593dc67fd, 0x0d6c8009d9a94f5a, 0x85676696d7fb7e2d,
			0xcf2794e0277187b7, 0x18765564cd99a68d, 0xcbc9466e58fee3ce, 0xab0200f58b01d137,
			0x93f5f5799a932462, 0x9e0082df0ba9e4b0, 0x7a5dbbc594ddb9f3, 0xf4b32f46226bada7,
			0x751e8fbc860ee5fb, 0x14ea5627c0843d90, 0xf723ca908e7af2ee, 0xa129ca6149be45e5,
			0x3f2acc7f57c29bdb, 0x699ae9f52cbe4794, 0x4bc1b3f0968dd39c, 0xbb6dc91da77961bd,
			0xbed65cf21aa2ee98, 0xd0f2cbb02e3b67c7, 0x93536795e3a33e88, 0xa80c038ccd5ccec8,
			0xb8ad50c6f649af94, 0xbce192de8a85b8ea, 0x17d835b85bbb15f3, 0x2f2e6163076bcfad,
			0xde4daaaca71dc9a5, 0xa6a2506687956571, 0xad87a3535c49ef28, 0x32d892fad841c342,
			0x7127512f72f27cce, 0xa7f32346f95978e3, 0x12e0b01abb051238, 0x15e034d40fa197ae,
			0x314dffbe0815a3b4, 0x027990f029623981, 0xcadcd4e59ef40c4d, 0x9abfd8766a33735c,
			0x0e3ea96b5304a7d0, 0xad0c42d6fc585992, 0x187306c89bc215a9, 0xd4a60abcf3792b95,
			0xf935451de4f21df2, 0xa9538f0419755787, 0xdb9acddff56ca510, 0xd06c98cd5c0975eb,
			0xe612a3cb9ecba951, 0xc766e62cfcadaf96, 0xee64435a9752fe72, 0xa192d576b245165a,
			0x0a8787bf8ecb74b2, 0x81b3e73d20b49b6f, 0x7fa8220ba3b2ecea, 0x245731c13ca42499,
			0xb78dbfaf3a8d83bd, 0xea1ad565322a1a0b, 0x60e61c23a3795013, 0x6606d7e446282b93,
			0x6ca4ecb15c5f91e1, 0x9f626da15c9625f3, 0xe51b38608ef25f57, 0x958a324ceb064572
		};

		uint64_t blockVal = 0;
		for (size_t i = 0; i < 7; ++i)
		{
			blockVal |= i << (i * 8);
			simu(msg) = blockVal + (i + 1) * (1ull << 56); // add padding
			co_await AfterClk(clock);
			//BOOST_TEST(simu(hash) == test_vector_sip64[i + 1]);
		}
		simu(msg) = 0;

		for (size_t i = 7; i < sip.latency(1, 64); ++i)
			co_await AfterClk(clock);

		for (size_t i = 0; i < 7; ++i)
		{
			BOOST_TEST(simu(hash) == test_vector_sip64[i + 1]);
			co_await AfterClk(clock);
		}



		});

	w.powerOn();

	w.show();

	a.exec();
}
#endif

BOOST_FIXTURE_TEST_CASE(SipHash64Test, gtry::BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope clockScope(clock);

	scl::SipHash sip(2, 4);
	sip.enableRegister(true);

	scl::SipHashState state;

	UInt key = "x0F0E0D0C0B0A09080706050403020100";
	sip.initialize(state, key);

	InputPins msg = pinIn(64_b).setName("msg");
	sip.block(state, msg);
	OutputPins hash = pinOut(sip.finalize(state)).setName("hash");

	addSimulationProcess([&]()->SimProcess {

		const uint64_t test_vector_sip64[] = {
			0x726fdb47dd0e0e31, 0x74f839c593dc67fd, 0x0d6c8009d9a94f5a, 0x85676696d7fb7e2d,
			0xcf2794e0277187b7, 0x18765564cd99a68d, 0xcbc9466e58fee3ce, 0xab0200f58b01d137,
			0x93f5f5799a932462, 0x9e0082df0ba9e4b0, 0x7a5dbbc594ddb9f3, 0xf4b32f46226bada7,
			0x751e8fbc860ee5fb, 0x14ea5627c0843d90, 0xf723ca908e7af2ee, 0xa129ca6149be45e5,
			0x3f2acc7f57c29bdb, 0x699ae9f52cbe4794, 0x4bc1b3f0968dd39c, 0xbb6dc91da77961bd,
			0xbed65cf21aa2ee98, 0xd0f2cbb02e3b67c7, 0x93536795e3a33e88, 0xa80c038ccd5ccec8,
			0xb8ad50c6f649af94, 0xbce192de8a85b8ea, 0x17d835b85bbb15f3, 0x2f2e6163076bcfad,
			0xde4daaaca71dc9a5, 0xa6a2506687956571, 0xad87a3535c49ef28, 0x32d892fad841c342,
			0x7127512f72f27cce, 0xa7f32346f95978e3, 0x12e0b01abb051238, 0x15e034d40fa197ae,
			0x314dffbe0815a3b4, 0x027990f029623981, 0xcadcd4e59ef40c4d, 0x9abfd8766a33735c,
			0x0e3ea96b5304a7d0, 0xad0c42d6fc585992, 0x187306c89bc215a9, 0xd4a60abcf3792b95,
			0xf935451de4f21df2, 0xa9538f0419755787, 0xdb9acddff56ca510, 0xd06c98cd5c0975eb,
			0xe612a3cb9ecba951, 0xc766e62cfcadaf96, 0xee64435a9752fe72, 0xa192d576b245165a,
			0x0a8787bf8ecb74b2, 0x81b3e73d20b49b6f, 0x7fa8220ba3b2ecea, 0x245731c13ca42499,
			0xb78dbfaf3a8d83bd, 0xea1ad565322a1a0b, 0x60e61c23a3795013, 0x6606d7e446282b93,
			0x6ca4ecb15c5f91e1, 0x9f626da15c9625f3, 0xe51b38608ef25f57, 0x958a324ceb064572
		};

		uint64_t blockVal = 0;
		for (size_t i = 0; i < 7; ++i)
		{
			blockVal |= i << (i * 8);
			simu(msg) = blockVal + (i + 1) * (1ull << 56); // add padding
			co_await AfterClk(clock);
			//BOOST_TEST(simu(hash) == test_vector_sip64[i + 1]);
		}
		simu(msg) = 0;

		for (size_t i = 7; i < sip.latency(1, 64); ++i)
			co_await AfterClk(clock);

		for (size_t i = 0; i < 7; ++i)
		{
			BOOST_TEST(simu(hash) == test_vector_sip64[i + 1]);
			co_await AfterClk(clock);
		}



		});

	//design.postprocess();
	//design.visualize("siphash");
	//sim::VCDSink vcd(design.getCircuit(), getSimulator(), "siphash.vcd");
	//vcd.addAllSignals();

	design.postprocess();
	runTicks(clock.getClk(), 24);
}

BOOST_FIXTURE_TEST_CASE(SipHashPaddingTest, gtry::BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope clockScope(clock);

	scl::SipHash sip;

	uint64_t blockVal = 0;
	for (size_t i = 0; i < 7; ++i)
	{
		blockVal |= i << (i * 8);
		uint64_t ref = blockVal + (i + 1) * (1ull << 56); // add padding

		UInt paddedBlock = sip.pad(ConstUInt(blockVal, (i + 1) * 8_b), i + 1);
		HCL_NAMED(paddedBlock);
		sim_assert(paddedBlock == ref);
	}

	eval();
}

BOOST_FIXTURE_TEST_CASE(SipHash64HelperTest, gtry::BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 10'000 });
	ClockScope clockScope(clock);

	auto [hash, latency] = scl::sipHash("x0100", "x0F0E0D0C0B0A09080706050403020100", false);
	BOOST_TEST(latency == 0);
	sim_assert(hash == 0x0d6c8009d9a94f5a) << hash;

	eval();
}

BOOST_FIXTURE_TEST_CASE(TabulationHashingTest, gtry::BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clockScope(clock);

	scl::TabulationHashing gen{ 16_b };
	UInt data = pinIn(16_b).setName("data");
	HCL_NAMED(data);
	UInt hash = reg(gen(data), {.allowRetimingBackward=true});

	scl::AvalonNetworkSection ports;
	gen.updatePorts(ports);
	ports.assignPins();
	pinOut(hash).setName("hash");

	std::array<std::array<uint16_t, 256>, 2> reference;
	std::array<scl::AvalonMM*, 2> mm = {{
			&ports.find("table0"), &ports.find("table1")
	}};

	addSimulationProcess([&]()->SimProcess {

		std::random_device rng;

		for (size_t i = 0; i < reference[0].size(); ++i)
		{
			for (size_t t = 0; t < reference.size(); ++t)
			{
				reference[t][i] = rng() & 0xFFFF;

				simu(mm[t]->address) = i;
				simu(*mm[t]->write) = '1';
				simu(*mm[t]->writeData) = reference[t][i];
			}
			co_await AfterClk(clock);
		}
		for (scl::AvalonMM* p : mm)
			simu(*p->write) = '0';

		for (size_t i = 0; i < 16; ++i)
			co_await AfterClk(clock);

		for (size_t i = 0; i < (1 << 16); i += 97)
		{
			simu(data) = i;
			co_await AfterClk(clock);

			const uint16_t refHash1 = reference[0][i & 0xFF];
			const uint16_t refHash2 = reference[1][i >> 8];
			const uint16_t refHash = refHash1 ^ refHash2;
			BOOST_TEST(simu(hash) == refHash);
		}
		});

	//design.visualize("TabulationHashingTest_before");
	//design.postprocess();
	//design.visualize("TabulationHashingTest");
	//sim::VCDSink vcd(design.getCircuit(), getSimulator(), "TabulationHashingTest.vcd");
	//vcd.addAllSignals();

	design.postprocess();
	runTicks(clock.getClk(), 1024);
}

BOOST_AUTO_TEST_CASE(TabulationHashingDriverBaseTest)
{
	TabulationHashingContext* ctx = tabulation_hashing_init(36, 36, 
		driver_alloc, driver_free);

	MmTestCtx mmCtx;
	tabulation_hashing_set_mm(ctx, MmTestWrite, &mmCtx);

	std::mt19937 rng{ 1337 };
	tabulation_hashing_set_random_content(ctx, driver_random_generator, &rng);

	std::map<std::array<uint32_t, 2>, uint32_t> seen;

	std::array<uint32_t, 2> hash;
	std::array<uint32_t, 2> key;
	for (uint32_t i = 0; i < 2048; ++i)
	{
		key[0] = i * 609598081u;
		key[1] = i * 1067102063u;
		tabulation_hashing_hash(ctx, key.data(), hash.data());

		bool known = seen.contains(hash);
		BOOST_TEST(!known);
		seen[hash] = i;
	}

	for (uint32_t i = 0; i < 2048; ++i)
	{
		key[0] = i * 609598081u;
		key[1] = i * 1067102063u;
		tabulation_hashing_hash(ctx, key.data(), hash.data());

		bool known = seen.contains(hash);
		BOOST_TEST(known);
		BOOST_TEST(seen[hash] == i);
	}

	tabulation_hashing_destroy(ctx);
}
