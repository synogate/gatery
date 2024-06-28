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
#include "bypassableDelayChain.h"

namespace gtry::scl {
	Bit fastRegisterChainDelay(Bit in) {
		return reg(in, '0');
	}

	Bit gateryMux2(Bit a0, Bit a1, Bit selector) {
		Bit ret = a0;
		IF(selector == '1')
			ret = a1;
		return ret;
	}

	Bit bypassableDelayChain(Bit input, UInt delay, std::function<Bit(Bit)> delayFunction, std::function<Bit(Bit, Bit, Bit)> mux2Function, size_t delayElementsPerStage) {
		Area area{ "bypassable_delay_chain", true };
		HCL_NAMED(delay);
		Bit temp_i_loop = input;
		setName(temp_i_loop, "chain_input");
		for (size_t i = 0; i < delay.width().bits(); i++) {
			Bit temp_j_loop = temp_i_loop;

			for (size_t j = 0; j < (1ull << i) * delayElementsPerStage; j++) {
				hlim::SignalAttributes att;
				att.userDefinedVendorAttributes["intel_quartus"]["keep"] = { .type = "boolean", .value = "true" };

				tap(temp_j_loop);
				attribute(temp_j_loop, att);
				//setName(temp_j_loop, "pre_delay");

				temp_j_loop = delayFunction(temp_j_loop);

				tap(temp_j_loop);
				attribute(temp_j_loop, att);
				//setName(temp_j_loop, "post_delay");
			}

			temp_i_loop = mux2Function(temp_i_loop, temp_j_loop, delay[i]);
		}

		setName(temp_i_loop, "chain_output");
		return temp_i_loop;
	}

	Bit delayChainWithTaps(Bit input, UInt delay, std::function<Bit(Bit)> delayFunction, size_t delayElementsPerStage)
	{
		Area area{ "delay_chain_with_taps", true };
		HCL_NAMED(delay);
		Bit temp_i_loop = input;
		setName(temp_i_loop, "chain_input");

		BVec delayedInputs = ConstBVec(BitWidth(delay.width().count()));

		for (size_t i = 0; i < (delay.width().count() - 1); i++) {
			delayedInputs[i] = temp_i_loop;

			hlim::SignalAttributes att;
			att.userDefinedVendorAttributes["intel_quartus"]["keep"] = { .type = "boolean", .value = "true" };

			tap(temp_i_loop);
			attribute(temp_i_loop, att);
			for (size_t j = 0; j < delayElementsPerStage; j++)
				temp_i_loop = delayFunction(temp_i_loop);

			tap(temp_i_loop);
			attribute(temp_i_loop, att);
		}

		delayedInputs.msb() = temp_i_loop;

		Bit ret = delayedInputs[delay];

		setName(ret, "chain_output");
		return ret;
	}
}

