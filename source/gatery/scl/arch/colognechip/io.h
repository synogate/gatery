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

#include <gatery/frontend/Bit.h>
#include <gatery/frontend/ExternalModule.h>

namespace gtry::scl::arch::colognechip
{
	template<class T>
	class CC_PIN
	{
	public:
		CC_PIN() {
			((T&)*this).requiresComponentDeclaration(true);
			((T&)*this).isEntity(false);
		};

		// name = "IO_<dir><bank>_<pin><#>" (IO_NA_A0)
		T& location(std::string_view name) { ((T&)*this).generic("PIN_NAME") = std::string{ name }; return (T&)*this; }

		// value = "1.2", "1.8", "2.5"
		T& voltage(std::string_view value) { ((T&)*this).generic("V_IO") = std::string{ value }; return (T&)*this; }
	};

	template<class T>
	class CC_PIN_IN
	{
	public:
		CC_PIN_IN() {
			//pulldown(false);
			//pullup(false);
			//keeper(false);
			schmittTrigger(false);
			regIn(false);
			delayIn(0);
		}

		T& pullup(bool value) { ((T&)*this).generic("PULLUP") = value ? 1 : 0; return (T&)*this; }
		T& pulldown(bool value) { ((T&)*this).generic("PULLDOWN") = value ? 1 : 0; return (T&)*this; }
		T& keeper(bool value) { ((T&)*this).generic("KEEPER") = value ? 1 : 0; return (T&)*this; }
		T& schmittTrigger(bool value) { ((T&)*this).generic("SCHMITT_TRIGGER") = value ? 1 : 0; return (T&)*this; }
		T& regIn(bool value) { ((T&)*this).generic("FF_IBF") = value ? 1 : 0; return (T&)*this; }

		// delay between (0 and 15) * 50ps
		T& delayIn(size_t value) { ((T&)*this).generic("DELAY_IBF") = value; return (T&)*this; }
	};

	template<class T>
	class CC_PIN_OUT
	{
	public:
		CC_PIN_OUT() {
			driveStrength(3);
			slewRate(false);
			regOut(false);
			delayOut(0);
		}

		// valid values are 3, 6, 9, 12 mA
		T& driveStrength(size_t mA) { HCL_DESIGNCHECK(mA % 3 == 0 && mA > 0 && mA <= 12); ((T&)*this).generic("DRIVE") = std::to_string(mA); return (T&)*this; }
		T& slewRate(bool fast) { ((T&)*this).generic("SLEW") = fast ? "FAST" : "SLOW"; return (T&)*this; }
		T& regOut(bool value) { ((T&)*this).generic("FF_OBF") = value ? 1 : 0; return (T&)*this; }

		// delay between (0 and 15) * 50ps
		T& delayOut(size_t value) { ((T&)*this).generic("DELAY_OBF") = value; return (T&)*this; }
	};

	class CC_IBUF : public ExternalModule, public CC_PIN<CC_IBUF>, public CC_PIN_IN<CC_IBUF>
	{
	public:
		CC_IBUF() : ExternalModule("CC_IBUF") {}

		CC_IBUF& pin(std::string_view portName) {
			pad() = pinIn().setName(std::string{ portName });
			return *this;
		}

		Bit& pad() { return in("I"); }
		Bit I() { return out("Y"); }
	};

	class CC_OBUF : public ExternalModule, public CC_PIN<CC_OBUF>, public CC_PIN_OUT<CC_OBUF>
	{
	public:
		CC_OBUF() : ExternalModule("CC_OBUF") {}

		CC_OBUF& pin(std::string_view portName) {
			pinOut(pad()).setName(std::string{ portName });
			return *this;
		}

		Bit& O() { return in("A"); }
		Bit pad() { Bit o = out("O"); o.simulationOverride(final(O())); return o; }
	};

	class CC_TOBUF : public ExternalModule, public CC_PIN<CC_TOBUF>, public CC_PIN_OUT<CC_TOBUF>
	{
	public:
		CC_TOBUF() : ExternalModule("CC_TOBUF") {
			disable() = '0';
		}

		CC_TOBUF& pin(std::string_view portName) {
			pinOut(pad()).setName(std::string{ portName });
			return *this;
		}

		Bit& disable() { return in("T"); }
		Bit& O() { return in("A"); }
		Bit pad() { 
			Bit o = out("O");
			o.simulationOverride(mux(final(disable()), { final(O()), Bit{'X'} }));
			return o;
		}
	};

	class CC_IOBUF : public ExternalModule, public CC_PIN<CC_IOBUF>, public CC_PIN_IN<CC_IOBUF>, public CC_PIN_OUT<CC_IOBUF>
	{
	public:
		CC_IOBUF() : ExternalModule("CC_IOBUF") {
			disable() = '0';
		}

		CC_IOBUF& pin(std::string_view portName) { 
			Bit simVal = mux(final(disable()), { final(O()), Bit{'x'} });
			inoutPin("IO", portName, {}, simVal);
			return *this; 
		}

		Bit& disable() { return in("T"); }
		Bit& O() { return in("A"); }
		Bit I() { 
			Bit o = out("Y");
			o.simulationOverride(mux(final(disable()), { final(O()), Bit{'X'} }));
			return o;
		}
	};

	template<class T>
	class CC_LVDS_PIN
	{
	public:
		CC_LVDS_PIN() {
			((T&)*this).requiresComponentDeclaration(true);
			((T&)*this).isEntity(false);
		}

		// bank = "IO_<dir><bank>" (IO_NA)
		T& location(std::string_view bank, size_t pinIdx) { 
			((T&)*this).generic("PIN_NAME_P") = std::string{ bank } + "_A" + std::to_string(pinIdx);
			((T&)*this).generic("PIN_NAME_N") = std::string{ bank } + "_B" + std::to_string(pinIdx);
			return (T&)*this; 
		}
		
		// value = "1.8", "2.5"
		T& voltage(std::string_view value) { ((T&)*this).generic("V_IO") = std::string{ value }; return (T&)*this; }
	};

	template<class T>
	class CC_LVDS_PIN_IN
	{
	public:
		CC_LVDS_PIN_IN() {
			oct(false);
			regIn(false);
			delayIn(0);
		}

		// delay between (0 and 15) * 50ps
		T& delayIn(size_t value) { ((T&)*this).generic("DELAY_IBF") = value; return (T&)*this; }
		T& oct(bool value) { ((T&)*this).generic("LVDS_RTERM") = value ? 1 : 0; return (T&)*this; }
		T& regIn(bool value) { ((T&)*this).generic("FF_IBF") = value ? 1 : 0; return (T&)*this; }
	};

	template<class T>
	class CC_LVDS_PIN_OUT
	{
	public:
		CC_LVDS_PIN_OUT() {
			delayOut(0);
			regOut(false);
			boost(false);
		}

		// delay between (0 and 15) * 50ps
		T& delayOut(size_t value) { ((T&)*this).generic("DELAY_OBF") = value; return (T&)*this; }
		T& regOut(bool value) { ((T&)*this).generic("FF_OBF") = value ? 1 : 0; return (T&)*this; }

		// 3.2mA vs. 6.4mA
		T& boost(bool enable) { ((T&)*this).generic("LVDS_BOOST") = enable ? 1 : 0; return (T&)*this; }
	};

	class CC_LVDS_IBUF : public ExternalModule, public CC_LVDS_PIN<CC_LVDS_IBUF>, public CC_LVDS_PIN_IN<CC_LVDS_IBUF>
	{
	public:
		CC_LVDS_IBUF() : ExternalModule("CC_LVDS_IBUF") {}

		CC_LVDS_IBUF& pin(std::string_view portNameP, std::string_view portNameN) { 
			padP() = pinIn().setName(std::string{ portNameP });
			padN() = pinIn().setName(std::string{ portNameN });
			return *this; 
		}

		Bit& padP() { return in("I_P"); }
		Bit& padN() { return in("I_N"); }
		Bit I() { return out("Y"); }
	};

	class CC_LVDS_OBUF : public ExternalModule, public CC_LVDS_PIN<CC_LVDS_OBUF>, public CC_LVDS_PIN_OUT<CC_LVDS_OBUF>
	{
	public:
		CC_LVDS_OBUF() : ExternalModule("CC_LVDS_OBUF") {}

		CC_LVDS_OBUF& pin(std::string_view portNameP, std::string_view portNameN) {
			pinOut(padP()).setName(std::string{ portNameP });
			pinOut(padN()).setName(std::string{ portNameN });
			return *this;
		}

		Bit padP() { Bit o = out("O_P"); o.simulationOverride(O()); return o; }
		Bit padN() { Bit o = out("O_N"); o.simulationOverride(!O()); return o; }
		Bit& O() { return in("A"); }
	};

	class CC_LVDS_TOBUF : public ExternalModule, public CC_LVDS_PIN<CC_LVDS_TOBUF>, public CC_LVDS_PIN_OUT<CC_LVDS_TOBUF>
	{
	public:
		CC_LVDS_TOBUF() : ExternalModule("CC_LVDS_TOBUF") {
			disable() = '0';
		}

		CC_LVDS_TOBUF& pin(std::string_view portNameP, std::string_view portNameN) {
			pinOut(padP()).setName(std::string{ portNameP });
			pinOut(padN()).setName(std::string{ portNameN });
			return *this;
		}
		
		Bit& disable() { return in("T"); }
		Bit& O() { return in("A"); }
		Bit padP() { return out("O_P"); }
		Bit padN() { return out("O_N"); }
	};

	class CC_LVDS_IOBUF : public ExternalModule, public CC_LVDS_PIN<CC_LVDS_IOBUF>, public CC_LVDS_PIN_IN<CC_LVDS_IOBUF>, public CC_LVDS_PIN_OUT<CC_LVDS_IOBUF>
	{
	public:
		CC_LVDS_IOBUF() : ExternalModule("CC_LVDS_IOBUF") {
			disable() = '0';
		}

		CC_LVDS_IOBUF& pin(std::string_view portNameP, std::string_view portNameN) { inoutPin("IO_P", portNameP); inoutPin("IO_N", portNameN); return *this; }
		Bit& disable() { return in("T"); }
		Bit& O() { return in("A"); }
		Bit I() { return out("Y"); }
	};

	class CC_IDDR : public ExternalModule
	{
	public:
		CC_IDDR() : ExternalModule("CC_IDDR") {
			requiresComponentDeclaration(true);
			isEntity(false);
			clk(ClockScope::getClk());
			clockInversion(false);
		}

		CC_IDDR(CC_IBUF& ibuf) : CC_IDDR() {
			D() = ibuf.I();
		}

		CC_IDDR(CC_IOBUF& ibuf) : CC_IDDR() {
			D() = ibuf.I();
		}

		CC_IDDR(CC_LVDS_IBUF& ibuf) : CC_IDDR() {
			D() = ibuf.I();
		}

		CC_IDDR(CC_LVDS_IOBUF& ibuf) : CC_IDDR() {
			D() = ibuf.I();
		}

		~CC_IDDR() {
			if (!std::uncaught_exceptions())
				generate();
		}

		CC_IDDR& clk(const Clock& clk) { clockIn(clk, "CLK"); return *this; }
		CC_IDDR& clockInversion(bool inv) { generic("CLK_INV") = inv ? 1 : 0; return *this; }

		// D input can be connected to any of the above input buffers
		Bit& D() { return in("D"); }

		Bit Q0() { Bit q = out("Q0"); q.simulationOverride(m_simQ0); return q; }
		Bit Q1() { Bit q = out("Q1"); q.simulationOverride(m_simQ1); return q; }

		void generate()
		{
			Clock clk0 = clockInFind("CLK").deriveClock(ClockConfig{
				.triggerEvent = ClockConfig::TriggerEvent::RISING
			});
			Clock clk1 = clk0.deriveClock(ClockConfig{
				.triggerEvent = ClockConfig::TriggerEvent::FALLING
			});

			if (generic("CLK_INV") == 1)
				std::swap(clk0, clk1);

			m_simQ0 = reg(D(), { .clock = clk0 });
			m_simQ1 = reg(D(), { .clock = clk1 });
		}
	private:
		Bit m_simQ0;
		Bit m_simQ1;
	};

	/**
	 * @brief To output first D0 and then D1 in the next clock cycle clockInversion(true) can be used.
	 */
	class CC_ODDR : public ExternalModule
	{
	public:
		CC_ODDR() : ExternalModule("CC_ODDR") {
			requiresComponentDeclaration(true);
			isEntity(false);

			D0();
			D1();
			clk(ClockScope::getClk());
			clockInversion(false);
			Q();
		}

		CC_ODDR(CC_OBUF& obuf) : CC_ODDR() {
			obuf.O() = Q();
		}

		CC_ODDR(CC_TOBUF& obuf) : CC_ODDR() {
			obuf.O() = Q();
		}

		CC_ODDR(CC_IOBUF& obuf) : CC_ODDR() {
			obuf.O() = Q();
		}

		CC_ODDR(CC_LVDS_OBUF& obuf) : CC_ODDR() {
			obuf.O() = Q();
		}

		CC_ODDR(CC_LVDS_TOBUF& obuf) : CC_ODDR() {
			obuf.O() = Q();
		}

		CC_ODDR(CC_LVDS_IOBUF& obuf) : CC_ODDR() {
			obuf.O() = Q();
		}

		~CC_ODDR() {
			if(!std::uncaught_exceptions())
				generate();
		}

		CC_ODDR& clk(const Clock& clk) {
			clockIn(clk, "CLK");
			clockIn(clk, "DDR");
			return *this;
		}

		// clock inversion will output D0 and D1 in the first and second half of the next clock cycle
		CC_ODDR& clockInversion(bool inv) { generic("CLK_INV") = inv ? 1 : 0; return *this; }

		Bit& D0() { return in("D0"); }
		Bit& D1() { return in("D1"); }

		// Q output can be connected to any of the above output buffers
		Bit Q() {  
			Bit q = out("Q");
			q.simulationOverride(m_simuValue);
			return q; 
		}

		void generate() 
		{
			Clock clk0 = clockInFind("CLK").deriveClock(ClockConfig{
				.triggerEvent = ClockConfig::TriggerEvent::RISING
			});
			Clock clk1 = clk0.deriveClock(ClockConfig{ 
				.triggerEvent = ClockConfig::TriggerEvent::FALLING
			});

			if (generic("CLK_INV") == 1)
				std::swap(clk0, clk1);

			Bit d0r = reg(D0(), { .clock = clk0 });
			Bit d1r = reg(D1(), { .clock = clk1 });

			Clock ddr = clockInFind("DDR");
			m_simuValue = mux(ddr.clkSignal(), { d1r, d0r });
		}

	private:
		Bit m_simuValue;
	};
}
