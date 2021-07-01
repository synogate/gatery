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
#pragma once
#include "riscv.h"

namespace gtry::scl::riscv
{
	class DualCycleRV : public RV32I
	{
	public:

		DualCycleRV(BitWidth instructionAddrWidth = 32_b, BitWidth dataAddrWidth = 32_b);

		virtual Memory<BVec>& fetch(uint64_t entryPoint = 0);
		virtual BVec fetch(const BVec& instruction, uint64_t entryPoint);

	protected:
		virtual void setIP(const BVec& ip);
		virtual void setResult(const BVec& result);
		virtual void setStall(const Bit& wait);

		Bit m_storeResult;

		Bit m_stall;
		Bit m_resultValid;
		BVec m_resultData = 32_b;

		Bit m_overrideIPValid;
		BVec m_overrideIP;

		Memory<BVec> m_rf;
		Memory<BVec> m_instructionMem;

	};
}
