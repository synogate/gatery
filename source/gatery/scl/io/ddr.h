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

#include <optional>

namespace gtry::scl {

struct DDROutParams : public hlim::NodeGroupMetaInfo {
	bool inputRegs = true;
	bool outputRegs = false;

	DDROutParams() = default;
	DDROutParams(const DDROutParams &rhs) = default;
};

template<Signal T>
T ddr(T D0, T D1, std::optional<T> reset = {}, const DDROutParams &params = {})
{
	Area area{"scl_oddr", true};
	area.createMetaInfo<DDROutParams>(params);
	
	setName(D0, "D0");
	setName(D1, "D1");
	if (reset)
		setName(*reset, "reset");

	if (params.inputRegs) {
		if (reset) {
			D0 = reg(D0, *reset);
			D1 = reg(D1, *reset);
		} else {
			D0 = reg(D0);
			D1 = reg(D1);
		}
	}

	auto &clock = ClockScope::getClk();

	Bit sel = clock.clkSignal();
	setName(sel, "CLK");

	T O;

	IF (sel)
		O = D0;
	ELSE
		O = D1;

	if (params.outputRegs) {
		if (reset)
			O = reg(O, *reset);
		else
			O = reg(O);
	}

	setName(O, "O");

	return O;
}

extern template Bit ddr(Bit D0, Bit D1, std::optional<Bit> reset, const DDROutParams &params);
extern template BVec ddr(BVec D0, BVec D1, std::optional<BVec> reset, const DDROutParams &params);
extern template UInt ddr(UInt D0, UInt D1, std::optional<UInt> reset, const DDROutParams &params);

inline Bit ddr(Bit D0, Bit D1, std::optional<Bit> reset = {}, const DDROutParams &params = {}) { return ddr<Bit>(D0, D1, reset, params); }

}