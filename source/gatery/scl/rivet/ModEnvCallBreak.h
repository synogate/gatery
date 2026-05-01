/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2026 Michael Offel, Andreas Ley

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
#include <gatery/frontend.h>
#include "ModResultSelect.h"

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class EnvCallBreak : public Base
	{
	public:
		EnvCallBreak()
		{
			Base::addInstr(InstrDef{ .name = "ECALL", .opcode = "11100", .func3 = "000", .jmpTarget = "Ecall" });
		}

		void environmentCallAddress(UInt address) { m_address = address; }

	protected:

		auto ecallUnit()
		{
			return [this](const ResultCollection& in)
				{
					HCL_DESIGNCHECK_HINT(m_address.valid(), "environment call target address not specified");

					ResultCollection ret = in;
					ret.result.push_back({ "Ecall", m_address });
					return ret;
				};
		}

	private:
		UInt m_address;
	};
}

