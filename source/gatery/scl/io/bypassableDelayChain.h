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

#include <gatery/frontend.h>

namespace gtry::scl {

	inline std::function<Bit(Bit)> cyclone10PinDelay = [i = 0](Bit in) mutable->Bit{
		pinOut(in, "simu_input", PinNodeParameter{ .simulationOnlyPin = true });
		Bit sim_output = pinIn(PinNodeParameter{.simulationOnlyPin = true}).setName("simu_output");
		DesignScope::get()->getCircuit().addSimulationProcess([=]()->SimProcess {
			while (true) {
				ReadSignalList allInputs;

				bool val = simu(in).value();
				bool def = simu(in).defined();

				fork([=]() -> SimProcess {
					simu(sim_output) = '0';
					co_await WaitFor({ 8'000, 1'000'000'000'000ull });
					simu(sim_output) = def ? (val ? '1': '0') : 'x';
					});
				co_await allInputs.anyInputChange();
			}
			});
		Bit enable = '1';
		Bit ret = tristatePin(in, enable).setName("delay_io_" + std::to_string(i++));
		tap(in);
		tap(ret);
		tap(enable);
		ret.simulationOverride(sim_output);
		return ret;
	};

	Bit fastRegisterChainDelay(Bit in);

	Bit gateryMux2(Bit a0, Bit a1, Bit selector);

	Bit bypassableDelayChain(Bit input, UInt delay, std::function<Bit(Bit)> delayFunction, std::function<Bit(Bit,Bit,Bit)> mux2Function, size_t delayElementsPerStage = 1);
	Bit delayChainWithTaps(Bit input, UInt delay, std::function<Bit(Bit)> delayFunction, size_t delayElementsPerStage = 1);

}
