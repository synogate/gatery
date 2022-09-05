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
#include "gatery/pch.h"
#include "SignalBitshiftOp.h"
#include "SignalLogicOp.h"
#include "ConditionalScope.h"
#include "SignalArithmeticOp.h"
#include "Pack.h"

#include <gatery/hlim/coreNodes/Node_Shift.h>


namespace gtry {


hlim::Node_Rewire::RewireOperation rightShiftRewireOp(size_t width, size_t amount, hlim::Node_Shift::fill fill)
{
	hlim::Node_Rewire::RewireOperation rewireOp;
	if (amount < width) {
		rewireOp.ranges.push_back({
			.subwidth = width - amount,
			.source = hlim::Node_Rewire::OutputRange::INPUT,
			.inputIdx = 0,
			.inputOffset = (size_t) amount,
		});
	}
		
	switch (fill) {
		case hlim::Node_Shift::fill::rotate:
			rewireOp.ranges.push_back({
				.subwidth = amount,
				.source = hlim::Node_Rewire::OutputRange::INPUT,
				.inputIdx = 0,
				.inputOffset = 0
			});
		break;
		case hlim::Node_Shift::fill::last:
			for (size_t i = 0; i < amount; i++) {
				rewireOp.ranges.push_back({
					.subwidth = 1,
					.source = hlim::Node_Rewire::OutputRange::INPUT,
					.inputIdx = 0,
					.inputOffset = width -1,
				});
			}
		break;
		case hlim::Node_Shift::fill::one:
			rewireOp.ranges.push_back({
				.subwidth = amount,
				.source = hlim::Node_Rewire::OutputRange::CONST_ONE,
			});
		break;
		case hlim::Node_Shift::fill::zero:
			rewireOp.ranges.push_back({
				.subwidth = amount,
				.source = hlim::Node_Rewire::OutputRange::CONST_ZERO,
			});
		break;
	}
	return rewireOp;
}

hlim::Node_Rewire::RewireOperation leftShiftRewireOp(size_t width, size_t amount, hlim::Node_Shift::fill fill)
{
	hlim::Node_Rewire::RewireOperation rewireOp;

	switch (fill) {
		case hlim::Node_Shift::fill::rotate:
			rewireOp.ranges.push_back({
				.subwidth = amount,
				.source = hlim::Node_Rewire::OutputRange::INPUT,
				.inputIdx = 0,
				.inputOffset = width - amount
			});
		break;
		case hlim::Node_Shift::fill::last:
			for (size_t i = 0; i < amount; i++) {
				rewireOp.ranges.push_back({
					.subwidth = 1,
					.source = hlim::Node_Rewire::OutputRange::INPUT,
					.inputIdx = 0,
					.inputOffset = 0,
				});
			}
		break;
		case hlim::Node_Shift::fill::one:
			rewireOp.ranges.push_back({
				.subwidth = amount,
				.source = hlim::Node_Rewire::OutputRange::CONST_ONE,
			});
		break;
		case hlim::Node_Shift::fill::zero:
			rewireOp.ranges.push_back({
				.subwidth = amount,
				.source = hlim::Node_Rewire::OutputRange::CONST_ZERO,
			});
		break;
	}
	if (amount < width) {
		rewireOp.ranges.push_back({
			.subwidth = width - amount,
			.source = hlim::Node_Rewire::OutputRange::INPUT,
			.inputIdx = 0,
			.inputOffset = 0,
		});
	}

	return rewireOp;
}


template<typename T, hlim::Node_Shift::dir direction>
T shift(const T &operand, size_t amount, hlim::Node_Shift::fill fill) {
	const size_t width = operand.size();
	
	hlim::Node_Rewire *node = DesignScope::createNode<hlim::Node_Rewire>(1);
	node->recordStackTrace();
	
	node->changeOutputType(operand.connType());
	
	hlim::Node_Rewire::RewireOperation rewireOp;
	if (direction == hlim::Node_Shift::dir::right)
		rewireOp = rightShiftRewireOp(width, amount, fill);
	else
		rewireOp = leftShiftRewireOp(width, amount, fill);

	node->setOp(std::move(rewireOp));
	node->connectInput(0, operand.readPort());
	return SignalReadPort(node);
}


BVec shl(const BVec &signal, int amount)  {
	HCL_DESIGNCHECK_HINT(amount >= 0, "Shifting by negative amount not allowed!");
	return shift<BVec, hlim::Node_Shift::dir::left>(signal, (size_t) amount, hlim::Node_Shift::fill::zero);
}

BVec shr(const BVec &signal, int amount)  {
	HCL_DESIGNCHECK_HINT(amount >= 0, "Shifting by negative amount not allowed!");
	return shift<BVec, hlim::Node_Shift::dir::right>(signal, (size_t) amount, hlim::Node_Shift::fill::zero);
}

UInt shl(const UInt &signal, int amount)  {
	HCL_DESIGNCHECK_HINT(amount >= 0, "Shifting by negative amount not allowed!");
	return shift<UInt, hlim::Node_Shift::dir::left>(signal, (size_t) amount, hlim::Node_Shift::fill::zero);
}

UInt shr(const UInt &signal, int amount)  {
	HCL_DESIGNCHECK_HINT(amount >= 0, "Shifting by negative amount not allowed!");
	return shift<UInt, hlim::Node_Shift::dir::right>(signal, (size_t) amount, hlim::Node_Shift::fill::zero);
}

SInt shl(const SInt &signal, int amount)  {
	HCL_DESIGNCHECK_HINT(amount >= 0, "Shifting by negative amount not allowed!");
	return shift<SInt, hlim::Node_Shift::dir::left>(signal, (size_t) amount, hlim::Node_Shift::fill::zero);
}

SInt shr(const SInt &signal, int amount)  {
	HCL_DESIGNCHECK_HINT(amount >= 0, "Shifting by negative amount not allowed!");
	return shift<SInt, hlim::Node_Shift::dir::right>(signal, (size_t) amount, hlim::Node_Shift::fill::last);
}



UInt shr(const UInt& signal, size_t amount, const Bit& arithmetic)
{
	Bit inShift = arithmetic & signal.msb();

	auto shiftedHigh = sext(inShift, BitWidth{ amount });
	auto shiftedLow = signal(amount, signal.width() - amount);
	return cat(shiftedHigh, shiftedLow);
}

UInt shr(const UInt& signal, const UInt& amount, const Bit& arithmetic)
{
	HCL_DESIGNCHECK_HINT(signal.size() == amount.width().count(), "not implemented");

	UInt acc(signal);
	for (size_t i = 0; i < amount.width().value; ++i)
	{
		IF(amount[i])
			acc = shr(acc, 1ull << i, arithmetic);
	}
	return acc;
}



/*

UInt shl(const UInt &signal, int amount)  {
	HCL_DESIGNCHECK_HINT(amount >= 0, "Shifting by negative amount not allowed!");
	return shift<UInt, hlim::Node_Shift::dir::left>(signal, (size_t) amount, hlim::Node_Shift::fill::zero);
}

UInt shr(const UInt &signal, int amount)  {
	HCL_DESIGNCHECK_HINT(amount >= 0, "Shifting by negative amount not allowed!");
	return shift<UInt, hlim::Node_Shift::dir::right>(signal, (size_t) amount, hlim::Node_Shift::fill::last);
}

*/




BVec rot(const BVec& signal, int amount)
{
	if (amount > 0)
		return shift<BVec, hlim::Node_Shift::dir::left>(signal, amount, hlim::Node_Shift::fill::rotate);
	else
		return shift<BVec, hlim::Node_Shift::dir::right>(signal, std::abs(amount), hlim::Node_Shift::fill::rotate);
}


UInt rot(const UInt& signal, int amount)
{
	if (amount > 0)
		return shift<UInt, hlim::Node_Shift::dir::left>(signal, amount, hlim::Node_Shift::fill::rotate);
	else
		return shift<UInt, hlim::Node_Shift::dir::right>(signal, std::abs(amount), hlim::Node_Shift::fill::rotate);
}

SInt rot(const SInt& signal, int amount)
{
	if (amount > 0)
		return shift<SInt, hlim::Node_Shift::dir::left>(signal, amount, hlim::Node_Shift::fill::rotate);
	else
		return shift<SInt, hlim::Node_Shift::dir::right>(signal, std::abs(amount), hlim::Node_Shift::fill::rotate);
}


SignalReadPort internal_shift(const SignalReadPort& signal, const SignalReadPort& amount, hlim::Node_Shift::dir direction, hlim::Node_Shift::fill fill)
{
	hlim::Node_Shift* node = DesignScope::createNode<hlim::Node_Shift>(direction, fill);
	node->recordStackTrace();

	node->connectOperand(signal);
	node->connectAmount(amount);
	return SignalReadPort(node);
}

}
