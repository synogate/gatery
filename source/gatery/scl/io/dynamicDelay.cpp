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
#include "dynamicDelay.h"

namespace gtry::scl {

	Bit delayChainWithTaps(Bit input, UInt delay, std::function<Bit(Bit)> delayFunction)
	{
		Area area{ "delay_chain_with_taps", true };
		HCL_NAMED(delay);
		Bit temp_i_loop = input; setName(temp_i_loop, "chain_input");

		BVec delayedInputs = ConstBVec(BitWidth(delay.width().count())); //necessary because gatery can't tell that the loop has been fully broken

		for (size_t i = 0; i < (delay.width().count() - 1); i++) {
			delayedInputs[i] = temp_i_loop;
			temp_i_loop = delayFunction(temp_i_loop);
			temp_i_loop.attribute(SignalAttributes{.dont_touch = true}); //to avoid replacing regs with shiftRegs/memories/etc. Might be overkill
			tap(temp_i_loop);
		}

		delayedInputs.msb() = temp_i_loop;

		Bit ret = delayedInputs[delay]; setName(ret, "chain_output");
		return ret;
	}

	Bit simulateDelay(Bit input, picoseconds delay, std::string name) {
		pinOut(input, name + "_input", PinNodeParameter{ .simulationOnlyPin = true });
		Bit sim_output = pinIn(PinNodeParameter{.simulationOnlyPin = true}).setName(name + "_output");
		
		DesignScope::get()->getCircuit().addSimulationProcess([=]()->SimProcess {
			simu(sim_output) = 'x';
			co_await WaitFor(Seconds{ 0,1 }); //every read needs to be able to read what's written
			while (true) {
				ReadSignalList allInputs;
				
				bool val = simu(input).value();
				bool def = simu(input).defined();

				fork([=]() -> SimProcess {
					co_await WaitFor({ delay.count(), 1'000'000'000'000ull });
					simu(sim_output) = def ? (val ? '1': '0') : 'x';
				});
				co_await allInputs.anyInputChange();
			}
		});
		return sim_output;
	}

	Bit PinDelay::operator()(Bit input)
	{
		Area area{ "scl_pin_delay_" + std::to_string(m_iterator), true};
		Bit enable = '1';
		enable.attribute({ .dont_touch = true });
		
		input.attribute({ .dont_touch = true });
		Bit ret = tristatePin(input, enable).setName("delay_io_" + std::to_string(m_iterator));
		ret.attribute({ .dont_touch = true });
		ret.simulationOverride(simulateDelay(input, m_delay, "sim_delay_" + std::to_string(m_iterator++)));
		tap(enable);	//this tap is necessary. Symptom: Quartus bypasses the pins when mapping. Hypothesis: something to do with signals and variables with attributes
		tap(input);		//this tap is necessary. Symptom: Quartus bypasses the pins when mapping. Hypothesis: something to do with signals and variables with attributes
		tap(ret);		//this tap is necessary. Symptom: Quartus bypasses the pins when mapping. Hypothesis: something to do with signals and variables with attributes
		return ret;
	}
}

