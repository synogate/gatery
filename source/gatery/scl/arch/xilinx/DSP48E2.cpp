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
#include "gatery/scl_pch.h"
#include "DSP48E2.h"

#include <gatery/frontend.h>
#include <gatery/scl/math/PipelinedMath.h>

namespace gtry::scl::arch::xilinx
{
	DSP48E2::DSP48E2() :
		ExternalModule("DSP48E2", "UNISIM", "vcomponents")
	{
		ce('1');
		rst('0');
		carryIn() = '0';
		carryInSel() = 0; // carry in
		inMode() = 0;
		opMode() = 0;
		aluMode() = 0;

		a() = 0;
		b() = 0;
		c() = 0;
		d() = 0;

		in("ACIN", 30_b) = 0;
		in("BCIN", 18_b) = 0;
		in("PCIN", 48_b) = 0;
		in("CARRYCASCIN") = '0';
		in("MULTSIGNIN") = '0';
	}

	void DSP48E2::clock(const Clock& clock)
	{
		clockIn(clock, "CLK");

		if(clock.getClk()->getRegAttribs().resetType != Clock::ResetType::NONE)
		{
			rst(clock.reset(Clock::ResetActive::HIGH));
		}
	}

	void DSP48E2::opMode(MuxW w, MuxX x, MuxY y, MuxZ z)
	{
		HCL_DESIGNCHECK(x != MuxX::m || y == MuxY::m);
		size_t mode = size_t(w) << 7 | size_t(z) << 4 | size_t(y) << 2 | size_t(x);
		opMode() = mode;
	}

	void DSP48E2::ce(const Bit& ce)
	{
		ceA1() = ce;
		ceA2() = ce;
		ceAD() = ce;
		ceAluMode() = ce;
		ceB1() = ce;
		ceB2() = ce;
		ceC() = ce;
		ceCarryIn() = ce;
		ceCtrl() = ce;
		ceD() = ce;
		ceInMode() = ce;
		ceM() = ce;
		ceP() = ce;
	}

	void DSP48E2::rst(const Bit& rst)
	{
		rstA() = rst;
		rstAllCarryIn() = rst;
		rstAluMode() = rst;
		rstB() = rst;
		rstC() = rst;
		rstCtrl() = rst;
		rstD() = rst;
		rstInMode() = rst;
		rstM() = rst;
		rstP() = rst;
	}

	SInt mulAccumulate(SInt a, SInt b, Bit restart, Bit valid, std::string_view instanceName)
	{
		Area ent{ "scl_mulAccumulate", true };
		if(!instanceName.empty())
			ent.instanceName(std::string{ instanceName });

		HCL_DESIGNCHECK(a.width() <= 27_b);
		HCL_DESIGNCHECK(b.width() <= 18_b);
		HCL_NAMED(a);
		HCL_NAMED(b);
		HCL_NAMED(restart);
		HCL_NAMED(valid);

		// model
		SInt m = reg(reg(sext(a, 45_b) * sext(b, 45_b)));
		SInt acc = 48_b;
		Bit restartDelayed = reg(reg(restart));
		IF(reg(reg(valid)))
		{
			IF(restartDelayed)
				acc = 0;
			acc += sext(m);
		}
		acc = reg(acc);

		// export
		DSP48E2 dsp;
		dsp.clock(ClockScope::getClk());
		dsp.a() = (BVec)sext(a, 27_b);
		dsp.b() = (BVec)sext(b, 18_b);

		dsp.ceP() &= reg(reg(valid));

		dsp.inMode()[0] = '1'; // select A1 register
		dsp.inMode()[4] = '1'; // select B1 register

		dsp.opMode(DSP48E2::MuxW::p, DSP48E2::MuxX::m, DSP48E2::MuxY::m, DSP48E2::MuxZ::zero);
		IF(reg(restart))
			dsp.opMode(DSP48E2::MuxW::zero, DSP48E2::MuxX::m, DSP48E2::MuxY::m, DSP48E2::MuxZ::zero);

		acc.exportOverride((SInt)dsp.p());
		HCL_NAMED(acc);
		return acc;
	}

	SInt mulAccumulate(SInt a1, SInt b1, SInt a2, SInt b2, Bit restart, Bit valid, std::string_view instanceName)
	{
		Area ent{ "scl_mulAccumulate", true };
		if (!instanceName.empty())
			ent.instanceName(std::string{ instanceName });

		HCL_DESIGNCHECK(a1.width() <= 27_b);
		HCL_DESIGNCHECK(b1.width() <= 18_b);
		HCL_NAMED(a1);
		HCL_NAMED(b1);
		HCL_DESIGNCHECK(a2.width() <= 27_b);
		HCL_DESIGNCHECK(b2.width() <= 18_b);
		HCL_NAMED(a2);
		HCL_NAMED(b2);
		HCL_NAMED(restart);
		HCL_NAMED(valid);

		// model
		SInt m = sext(a1, 48_b) * sext(b1, 48_b) + sext(a2, 48_b) * sext(b2, 48_b);
		m = reg(reg(reg(m)));

		SInt acc = 48_b;
		Bit restartDelayed = reg(reg(reg(restart)));
		IF(reg(reg(reg(valid))))
		{
			IF(restartDelayed)
				acc = 0;
			acc += sext(m);
		}
		acc = reg(acc);

		// export
		std::array<DSP48E2, 2> dsp;
		for (DSP48E2& d : dsp)
			d.clock(ClockScope::getClk());

		dsp[0].a() = (BVec)sext(a1, 27_b);
		dsp[0].b() = (BVec)sext(b1, 18_b);
		dsp[0].opMode(DSP48E2::MuxW::zero, DSP48E2::MuxX::m, DSP48E2::MuxY::m, DSP48E2::MuxZ::zero);

		dsp[1].generic("AREG") = 2;
		dsp[1].generic("BREG") = 2;
		dsp[1].a() = (BVec)sext(a2, 27_b);
		dsp[1].b() = (BVec)sext(b2, 18_b);
		dsp[1].in("PCIN", 48_b) = dsp[0].out("PCOUT", 48_b);

		dsp[1].ceP() &= reg(reg(reg(valid)));

		dsp[1].opMode(DSP48E2::MuxW::p, DSP48E2::MuxX::m, DSP48E2::MuxY::m, DSP48E2::MuxZ::pcin);
		IF(reg(reg(restart)))
			dsp[1].opMode(DSP48E2::MuxW::zero, DSP48E2::MuxX::m, DSP48E2::MuxY::m, DSP48E2::MuxZ::pcin);

		acc.exportOverride((SInt)dsp[1].p());
		HCL_NAMED(acc);
		return acc;
	}

	std::tuple<UInt, size_t> mul(const UInt& a, const UInt& b, BitWidth resultW, size_t resultOffset)
	{
		HCL_DESIGNCHECK(a.width() + b.width() >= resultW + resultOffset);
		Area ent{ "scl_dsp_mul", true };

		const size_t mulAWidth = 26;
		const size_t mulBWidth = 17; // one bit less for unsigned multiplication

		const size_t potentialAstepsA = (a.width().bits() + mulAWidth - 1) / mulAWidth;
		const size_t potentialAstepsB = (b.width().bits() + mulAWidth - 1) / mulAWidth;
		BVec A = (BVec)(potentialAstepsA < potentialAstepsB ? a : b);
		BVec B = (BVec)(potentialAstepsA < potentialAstepsB ? b : a);
		HCL_NAMED(A);
		HCL_NAMED(B);

		const size_t mulASteps = (A.width().bits() + mulAWidth - 1) / mulAWidth;
		const size_t mulBSteps = (B.width().bits() + mulBWidth - 1) / mulBWidth;

		UInt outPhys;
		for (size_t iA = 0; iA < mulASteps; ++iA)
		{
			BVec PC;
			BVec Ain = A;
			BVec Bin = B;
			BVec Bout = ConstBVec(0, a.width() + b.width());

			for (size_t iB = 0; iB < mulBSteps; ++iB)
			{
				size_t aOfs = iA * mulAWidth;
				size_t bOfs = iB * mulBWidth;
				if (aOfs + bOfs >= resultOffset + resultW.bits())
				{
					// no DSP needed but the result needs to be delayed as if we had used a DSP
					Bout = reg(Bout);
					continue;
				}

				BitWidth aW = std::min(A.width() - aOfs, BitWidth(mulAWidth));
				BitWidth bW = std::min(B.width() - bOfs, BitWidth(mulBWidth));

				DSP48E2 dsp;
				dsp.clock(ClockScope::getClk());
				dsp.a() = (BVec)zext(Ain(aOfs, aW), 27_b);
				dsp.b() = (BVec)zext(Bin(bOfs, bW), 18_b);

				dsp.opMode(
					DSP48E2::MuxW::zero,
					DSP48E2::MuxX::m,
					DSP48E2::MuxY::m,
					iB == 0 ? DSP48E2::MuxZ::zero : DSP48E2::MuxZ::pcin17
				);

				if (iB != 0) dsp.in("PCIN", 48_b) = PC;
				if (iB != mulBSteps - 1) PC = dsp.out("PCOUT", 48_b);

				if(iB != 0)
				{
					// register moved into dsp slice
					Ain = reg(Ain);
					Bin = reg(Bin);
					dsp.generic("AREG") = 2;
					dsp.generic("BREG") = 2;
				}

				if(iB != 0)
					Bout = reg(Bout);
				BitWidth directOutW = iB != mulBSteps - 1 ? BitWidth{ mulBWidth } : BitWidth{ aW + bW };
				if(directOutW.bits() + aOfs + bOfs > resultOffset)
					Bout(aOfs + bOfs, directOutW) = dsp.p().lower(directOutW);
			}

			if(iA == 0)
				outPhys = (UInt)Bout;
			else
				outPhys += (UInt)Bout;
		}

		size_t latency = mulBSteps + 2;
		if (mulASteps > 1)
		{
			outPhys = reg(outPhys);
			latency++;
		}

		// simulation
		BitWidth immW = resultW + resultOffset;
		UInt out = (resizeTo(a, immW) * resizeTo(b, immW)).upper(resultW);
		for (size_t i = 0; i < latency; ++i) out = reg(out);

		out.exportOverride(outPhys(resultOffset, resultW));
		HCL_NAMED(out);
		return { out, latency };
	}

	UInt pipelinedMulDSP48E2(const UInt& a, const UInt& b, BitWidth resultW, size_t resultOffset)
	{
		HCL_DESIGNCHECK(a.width() + b.width() >= resultW + resultOffset);
		Area ent{ "scl_dsp48e2_mul", true };

		Bit enable;
		sim_assert(enable == '1') << "pipelinedMulDSP48E2 can not be disabled. From " << __FILE__ << ':' << __LINE__;

		const size_t mulAWidth = 26;
		const size_t mulBWidth = 17; // one bit less for unsigned multiplication

		const size_t potentialAstepsA = (a.width().bits() + mulAWidth - 1) / mulAWidth;
		const size_t potentialAstepsB = (b.width().bits() + mulAWidth - 1) / mulAWidth;
		BVec A = (BVec)(potentialAstepsA < potentialAstepsB ? a : b);
		BVec B = (BVec)(potentialAstepsA < potentialAstepsB ? b : a);
		HCL_NAMED(A);
		HCL_NAMED(B);

		const size_t mulASteps = (A.width().bits() + mulAWidth - 1) / mulAWidth;
		const size_t mulBSteps = (B.width().bits() + mulBWidth - 1) / mulBWidth;

		UInt outPhys;
		for (size_t iA = 0; iA < mulASteps; ++iA)
		{
			BVec PC;
			BVec Ain = A;
			BVec Bin = B;
			BVec Bout = ConstBVec(0, a.width() + b.width());

			for (size_t iB = 0; iB < mulBSteps; ++iB)
			{
				size_t aOfs = iA * mulAWidth;
				size_t bOfs = iB * mulBWidth;
				if (aOfs + bOfs >= resultOffset + resultW.bits())
				{
					// no DSP needed but the result needs to be delayed as if we had used a DSP
					ENIF (enable)
						Bout = reg(Bout);//, { .allowRetimingForward = true });
					continue;
				}

				BitWidth aW = std::min(A.width() - aOfs, BitWidth(mulAWidth));
				BitWidth bW = std::min(B.width() - bOfs, BitWidth(mulBWidth));

				DSP48E2 dsp;
				dsp.clock(ClockScope::getClk());
				dsp.a() = (BVec)zext(Ain(aOfs, aW), 27_b);
				dsp.b() = (BVec)zext(Bin(bOfs, bW), 18_b);

				dsp.opMode(
					DSP48E2::MuxW::zero,
					DSP48E2::MuxX::m,
					DSP48E2::MuxY::m,
					iB == 0 ? DSP48E2::MuxZ::zero : DSP48E2::MuxZ::pcin17
				);

				if (iB != 0) dsp.in("PCIN", 48_b) = PC;
				if (iB != mulBSteps - 1) PC = dsp.out("PCOUT", 48_b);

				if(iB != 0)
				{
					// register moved into dsp slice
					ENIF (enable) {
						Ain = reg(Ain);//, { .allowRetimingForward = true });
						Bin = reg(Bin);//, { .allowRetimingForward = true });
					}
					dsp.generic("AREG") = 2;
					dsp.generic("BREG") = 2;
				}

				if(iB != 0)
					ENIF (enable)
						Bout = reg(Bout);//, { .allowRetimingForward = true });
				BitWidth directOutW = iB != mulBSteps - 1 ? BitWidth{ mulBWidth } : BitWidth{ aW + bW };
				if(directOutW.bits() + aOfs + bOfs > resultOffset)
					Bout(aOfs + bOfs, directOutW) = dsp.p().lower(directOutW);
			}

			if(iA == 0)
				outPhys = (UInt)Bout;
			else
				outPhys += (UInt)Bout;
		}

		size_t latency = mulBSteps + 2;
		if (mulASteps > 1)
		{
			ENIF (enable)
				outPhys = reg(outPhys);
			latency++;
		}

		for (size_t i = 0; i < latency; ++i)
			std::tie(outPhys, enable) = negativeReg(outPhys);

		UInt outPhysCropped = outPhys(resultOffset, resultW);
		HCL_NAMED(outPhysCropped);
		return outPhysCropped;
	}


	PipelinedMulDSP48E2Pattern::PipelinedMulDSP48E2Pattern()
	{
		m_runPreOptimization = true;
	}


	bool PipelinedMulDSP48E2Pattern::scopedAttemptApply(hlim::NodeGroup *nodeGroup) const
	{
		if (nodeGroup->getName() != "scl_pipelinedMul") return false;

		const auto *meta = dynamic_cast<math::PipelinedMulMeta*>(nodeGroup->getMetaInfo());
		if (meta == nullptr) return false;

		NodeGroupSurgeryHelper surgery(nodeGroup);

		if (surgery.containsSignal("a") && surgery.containsSignal("b") && surgery.containsSignal("out")) {
			BVec a = surgery.hookBVecAfter("a");
			BVec b = surgery.hookBVecAfter("b");
			BVec out = surgery.hookBVecBefore("out");

			out.exportOverride((BVec) pipelinedMulDSP48E2((UInt)a, (UInt) b, out.width(), meta->resultOffset));
		} else
			dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
					<< "Not replacing " << nodeGroup << " with DSP48E2 because necessary signals could not be found!");


		return true;
	}

	UInt mulRetimable(const UInt& a, const UInt& b, BitWidth resultW, size_t resultOffset)
	{
		auto [result, latency] = mul(a, b, resultW, resultOffset);

		for (size_t i = 0; i < latency; ++i)
		{
			Bit en;
			std::tie(result, en) = negativeReg(result);
		}
		return result;
	}
}
