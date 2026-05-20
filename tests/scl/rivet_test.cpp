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

#include <gatery/simulation/waveformFormats/VCDSink.h>
#include <gatery/export/vhdl/VHDLExport.h>
#include <gatery/export/DotExport.h>

#include <gatery/scl/riscv/PipelineRV.h>
#include <gatery/scl/riscv/ElfLoader.h>
#include <gatery/scl/tilelink/TileLinkCache.h>

using namespace boost::unit_test;
using namespace gtry;
using namespace gtry::scl;

class RiscvInstructionSelfTest : public BoostUnitTestSimulationFixture
{
public:

	void runTest(std::string testName)
	{
		Clock clk{ ClockConfig{
			.absoluteFrequency = 100_mhz,
			.memoryResetType = hlim::RegisterAttributes::ResetType::NONE,
		} };

		ClockScope clkScp(clk);

		riscv::ElfLoader elf{ "external/riscv-tests/" + testName };
		BOOST_TEST(elf.segments().size() == 2);

		using namespace rivet;
		using TestRV = PipelineFiveStage <
		//	mod::GateMate<
			mod::InstrTileLink<
		//	mod::DataTileLink<
			mod::DataTileLinkReschedule<
			mod::Csr<
			mod::HazardDataResolve<
			mod::BranchMissPredict<
			mod::BaseIntInstrSet<
			mod::Branch<
			mod::FenceNoop<
			mod::LoadStoreSubword<
			mod::LoadStore<
			mod::EnvCallBreak<
		//	mod::ShiftUnit<
		//	mod::ShiftUnitSequential<
			mod::ShiftUnitReschedule<
			mod::ExternalResult<
			mod::ControlFlow<
			mod::RegisterFile<
			mod::IntAlu<
			mod::ResultSelect<
			mod::InstrDecode<
			mod::InstrBoolSolver<
			mod::InstrSet<
			mod::Root>>>>>>>>>>>>>>>>>>>>>;

		TestRV rv;
		rv.instrStartAddress(0);
		rv.environmentCallAddress("32x4");
		rv.addCsrReadOnly(0x342, ConstBVec(8, 32_b));

		riscv::ElfLoader::Segment text = elf.segments()[0];
		{ // code link
			Memory<BVec> imem(text.size.bytes() / 4, 32_b);
			imem.fillPowerOnState(sim::createDefaultBitVectorState(std::as_bytes(std::span{ text.data })));

			std::function<void(TileLinkChannelA&)> burstSelect = [&](TileLinkChannelA& a) {
				a->size = 2;
				IF(a->address.lower(3_b) == 0)
					a->size = 3;
				IF(a->address.lower(4_b) == 0)
					a->size = 4;
			};

			TileLinkU imemLink = 
				tileLinkMemory(imem, TileLinkMemoryConfig{ .sourceW = 6_b, .writePort = false, .latency = 1 }) | 
#if 0
				tileLinkAddBurst(4) |
				name("imemLinkPreCache") |
				tileLinkCacheDirectMappedReadOnlyBurst(4_b, burstSelect) | 
#endif
				name("imemLink");

			rv.instrFetch(move(imemLink));
		}

		riscv::ElfLoader::Segment data = elf.segments()[1];
		size_t dmemOffset = data.offset - text.offset;
		size_t dmemSize = data.size.bytes() + dmemOffset;
		Memory<BVec> dmem(dmemSize / 4, 32_b);

		sim::DefaultBitVectorState dmemData;
		dmemData.resize(dmemSize * 8);
		dmemData.insert(sim::createDefaultBitVectorState(std::as_bytes(std::span{ data.data })), dmemOffset * 8);
		dmem.fillPowerOnState(dmemData);

		TileLinkU dmemLink = tileLinkMemory(dmem, TileLinkMemoryConfig{ .sourceW = 0_b, .latency = 2 });
		Stream result = strm::createVStream(dmemLink.a->data.lower(8_b), transfer(dmemLink.a) & dmemLink.a->isPut() & dmemLink.a->address == dmemOffset);
		rv.dataMem(move(dmemLink));

		pinOut(result, "result");

		addSimulationProcess([&]()->SimProcess {
			co_await OnClk(clk);
			co_await performTransferWait(result, clk);
			BOOST_TEST(simu(*result) == 1);

			if (simu(*result) != 1)
				std::cerr << "failed subtest: " << (simu(*result).value() >> 1);

			co_await OnClk(clk);
			stopTest();
		});

		rv.generate();

		design.postprocess();
		BOOST_TEST(!runHitsTimeout(100us));
	}
};

#define TEST_NAME(name) rivet_riscv_instructions_self_test_##name
#define SELF_TEST(name) BOOST_FIXTURE_TEST_CASE(rivet_instruction_self_test_##name, RiscvInstructionSelfTest) { runTest("rv32ui-p-"#name); }

SELF_TEST(add)
SELF_TEST(addi)
SELF_TEST(and)
SELF_TEST(andi)
SELF_TEST(auipc)
SELF_TEST(beq)
SELF_TEST(bge)
SELF_TEST(bgeu)
SELF_TEST(blt)
SELF_TEST(bltu)
SELF_TEST(bne)
//SELF_TEST(fence_i) // test framework does not allow self modifying code
SELF_TEST(jal)
SELF_TEST(jalr)
SELF_TEST(lb)
SELF_TEST(lbu)
SELF_TEST(ld_st)
SELF_TEST(lh)
SELF_TEST(lhu)
SELF_TEST(lui)
SELF_TEST(lw)
//SELF_TEST(ma_data) // no support for missaligned access
SELF_TEST(or)
SELF_TEST(ori)
SELF_TEST(sb)
SELF_TEST(sh)
SELF_TEST(simple)
SELF_TEST(sll)
SELF_TEST(slli)
SELF_TEST(slt)
SELF_TEST(slti)
SELF_TEST(sltiu)
SELF_TEST(sltu)
SELF_TEST(sra)
SELF_TEST(srai)
SELF_TEST(srl)
SELF_TEST(srli)
SELF_TEST(st_ld)
SELF_TEST(sub)
SELF_TEST(sw)
SELF_TEST(xor)
SELF_TEST(xori)
