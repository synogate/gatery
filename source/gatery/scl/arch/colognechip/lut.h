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
#include <exception>

#include <gatery/frontend.h>
#include <gatery/frontend/ExternalModule.h>

namespace gtry::scl::arch::colognechip
{
	class CC_DFF : public ExternalModule
	{
	public:
		CC_DFF() : ExternalModule("CC_DFF") {
			clk(ClockScope::getClk());
			clockInversion(false);
			enInversion(false);
			srInversion(false);
			srValue(false);
			requiresComponentDeclaration(true);
			isEntity(false);

			EN() = '1';
			SR() = '0';
			D() = 'X';
		}

		~CC_DFF()
		{
			if(!std::uncaught_exceptions())
				generate();
		}

		CC_DFF& clk(const Clock& clk) { clockIn(clk, "CLK"); return *this; }
		CC_DFF& clockInversion(bool inv) { generic("CLK_INV") = inv ? 1 : 0; return *this; }
		CC_DFF& enInversion(bool inv) { generic("EN_INV") = inv ? 1 : 0; return *this; }
		CC_DFF& srInversion(bool inv) { generic("SR_INV") = inv ? 1 : 0; return *this; }
		CC_DFF& srValue(bool inv) { generic("SR_VAL") = inv ? 1 : 0; return *this; }
		CC_DFF& initValue(bool inv) { generic("INIT") = inv ? 1 : 0; return *this; }

		Bit& EN() { return in("EN"); }
		Bit& SR() { return in("SR"); }
		Bit& D() { return in("D"); }

		Bit Q() { Bit q = out("Q"); q.simulationOverride(m_simQ); return q; }

		void generate()
		{
			Clock clk = clockInFind("CLK").deriveClock(ClockConfig{
				.triggerEvent = (generic("CLK_INV") == 0) ? ClockConfig::TriggerEvent::RISING : ClockConfig::TriggerEvent::FALLING,
				.resetType = ClockConfig::ResetType::ASYNCHRONOUS,
				.resetActive = (generic("SR_INV") == 0) ? ClockConfig::ResetActive::HIGH : ClockConfig::ResetActive::LOW,
			});
			ENIF(EN())
				m_simQ = reg(D(), (generic("SR_VAL") == 0) ? '0' : '1', {.clock = clk}) | name("CC_DFF_Q");
		}
	private:
		Bit m_simQ;
	};

	class CC_DLT : public ExternalModule
	{
	public:
		CC_DLT();

		Bit& D() { return in("D"); }
		Bit& G() { return in("G"); }
		Bit& SR() { return in("SR"); }
		Bit Q() { return out("Q"); }

		CC_DLT& gInv(bool invert) { generic("G_INV") = invert ? 1 : 0; return *this; }
		CC_DLT& srInv(bool invert) { generic("SR_INV") = invert ? 1 : 0; return *this; }
		CC_DLT& srVal(bool value) { generic("SR_VAL") = value ? 1 : 0; return *this; }
		CC_DLT& init(bool value) { generic("INIT") = value ? 1 : 0; return *this; }
	};

	class CC_MX2 : public ExternalModule
	{
	public:
		CC_MX2() : ExternalModule("CC_MX2") {
			requiresComponentDeclaration(true);
			isEntity(false);

			m_Y = simOverride(out("Y"), mux(S0(), { D0(), D1() }));
		}

		Bit& D0() { return in("D0"); }
		Bit& D1() { return in("D1"); }
		Bit& S0() { return in("S0"); }

		const Bit& Y() { return m_Y; }

	private:
		Bit m_Y;
	};

	class CC_MX4 : public ExternalModule
	{
	public:
		CC_MX4() : ExternalModule("CC_MX4") {
			requiresComponentDeclaration(true);
			isEntity(false);

			m_Y = simOverride(out("Y"), mux(cat(S1(), S0()), { D0(), D1(), D2(), D3() }));
		}

		Bit& D0() { return in("D0"); }
		Bit& D1() { return in("D1"); }
		Bit& D2() { return in("D2"); }
		Bit& D3() { return in("D3"); }
		Bit& S0() { return in("S0"); }
		Bit& S1() { return in("S1"); }

		const Bit& Y() { return m_Y; }

	private:
		Bit m_Y;
	};


}
