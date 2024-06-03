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
#include "SignalMiscOp.h"

#include "SignalLogicOp.h"
#include "SignalCompareOp.h"
#include "Area.h"
#include "Compound.h"
#include "Clock.h"
#include "Pack.h"
#include "Attributes.h"

#include "../hlim/coreNodes/Node_ClkRst2Signal.h"
#include "../hlim/coreNodes/Node_Logic.h"
#include "../hlim/coreNodes/Node_Rewire.h"

#include <gatery/utils/Range.h>
#include <gatery/utils/Preprocessor.h>


namespace gtry 
{

	SignalTapHelper::SignalTapHelper(hlim::Node_SignalTap::Level level)
	{
		m_node = DesignScope::createNode<hlim::Node_SignalTap>();
		m_node->recordStackTrace();
		m_node->setLevel(level);
	}

	size_t SignalTapHelper::addInput(hlim::NodePort nodePort)
	{
		for (auto i : utils::Range(m_node->getNumInputPorts()))
			if (m_node->getDriver(i) == nodePort)
				return i;
		m_node->addInput(nodePort);
		return m_node->getNumInputPorts()-1;
	}

	void SignalTapHelper::triggerIf(const Bit &condition)
	{
		hlim::NodePort conditionNP = condition.readPort();

		if (ClockScope::anyActive()) {
			auto *clk = ClockScope::getClk().getClk();
			if (clk->getRegAttribs().resetType != hlim::RegisterAttributes::ResetType::NONE) {
				auto *rstSignal = DesignScope::createNode<hlim::Node_ClkRst2Signal>();
				rstSignal->recordStackTrace();
				rstSignal->setClock(clk);

				hlim::NodePort rstNotActive;

				if (clk->getRegAttribs().resetActive == hlim::RegisterAttributes::Active::HIGH) {
					auto *logic = DesignScope::createNode<hlim::Node_Logic>(hlim::Node_Logic::NOT);
					logic->recordStackTrace();
					logic->connectInput(0, {.node = rstSignal, .port = 0ull});
					rstNotActive = {.node = logic, .port = 0ull};
				} else {
					rstNotActive = {.node = rstSignal, .port = 0ull};
				}

				auto *logic = DesignScope::createNode<hlim::Node_Logic>(hlim::Node_Logic::AND);
				logic->recordStackTrace();
				logic->connectInput(0, conditionNP);
				logic->connectInput(1, rstNotActive);
				conditionNP = {.node = logic, .port = 0ull};
			}
		}

		HCL_ASSERT_HINT(m_node->getNumInputPorts() == 0, "Condition must be the first input to signal tap!");
		addInput(conditionNP);
		m_node->setTrigger(hlim::Node_SignalTap::TRIG_FIRST_INPUT_HIGH);
	}

	void SignalTapHelper::triggerIfNot(const Bit &condition)
	{
		hlim::NodePort conditionNP = condition.readPort();

		if (ClockScope::anyActive()) {
			auto *clk = ClockScope::getClk().getClk();
			if (clk->getRegAttribs().resetType != hlim::RegisterAttributes::ResetType::NONE) {
				auto *rstSignal = DesignScope::createNode<hlim::Node_ClkRst2Signal>();
				rstSignal->recordStackTrace();
				rstSignal->setClock(clk);

				hlim::NodePort rstActive;

				if (clk->getRegAttribs().resetActive == hlim::RegisterAttributes::Active::HIGH) {
					rstActive = {.node = rstSignal, .port = 0ull};
				} else {
					auto *logic = DesignScope::createNode<hlim::Node_Logic>(hlim::Node_Logic::NOT);
					logic->recordStackTrace();
					logic->connectInput(0, {.node = rstSignal, .port = 0ull});
					rstActive = {.node = logic, .port = 0ull};
				}

				auto *logic = DesignScope::createNode<hlim::Node_Logic>(hlim::Node_Logic::OR);
				logic->recordStackTrace();
				logic->connectInput(0, conditionNP);
				logic->connectInput(1, rstActive);
				conditionNP = {.node = logic, .port = 0ull};
			}
		}

		HCL_ASSERT_HINT(m_node->getNumInputPorts() == 0, "Condition must be the first input to signal tap!");
		addInput(conditionNP);
		m_node->setTrigger(hlim::Node_SignalTap::TRIG_FIRST_INPUT_LOW);
	}


	SignalTapHelper &SignalTapHelper::operator<<(const std::string &msg)
	{
		m_node->addMessagePart(msg);
		return *this;
	}

	template<BitVectorDerived T>
	T swapEndian(const T& word, BitWidth byteSize)
	{
		const size_t numSymbols = word.width().numBeats(byteSize);
		const size_t srcWidth = numSymbols * byteSize.value;

		hlim::Node_Rewire* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
		rewire->connectInput(0, word.readPort().expand(srcWidth, hlim::ConnectionType::BITVEC));

		hlim::Node_Rewire::RewireOperation op;
		op.ranges.reserve(numSymbols);
		for (size_t i = 0; i < numSymbols; ++i)
			op.addInput(0, srcWidth - byteSize.value * (i + 1), byteSize.value);
		rewire->setOp(std::move(op));

		T ret{ SignalReadPort(rewire) };
		if (!word.getName().empty())
			ret.setName(std::string(word.getName()) + "_swapped");
		return ret;
	}

	template<BitVectorDerived T>
	T swapEndian(const T& word, BitWidth byteSize, BitWidth wordSize)
	{
		T ret = word.width();
		for (size_t i = 0; i < word.width() / wordSize; ++i)
		{
			auto sym = Selection::Symbol(i, wordSize);
			ret(sym) = swapEndian(word(sym), byteSize);
		}
		return ret;
	}

	BVec swapEndian(const BVec& word, BitWidth byteSize, BitWidth wordSize) { return swapEndian<BVec>(word, byteSize, wordSize); }
	BVec swapEndian(const BVec& word, BitWidth byteSize) { return swapEndian<BVec>(word, byteSize); }

	UInt swapEndian(const UInt& word, BitWidth byteSize, BitWidth wordSize) { return swapEndian<UInt>(word, byteSize, wordSize); }
	UInt swapEndian(const UInt& word, BitWidth byteSize) { return swapEndian<UInt>(word, byteSize); }

	SInt swapEndian(const SInt& word, BitWidth byteSize, BitWidth wordSize) { return swapEndian<SInt>(word, byteSize, wordSize); }
	SInt swapEndian(const SInt& word, BitWidth byteSize) { return swapEndian<SInt>(word, byteSize); }

	template<BitVectorDerived T>
	T muxWord(UInt selector, T flat_array)
	{
		auto entity = Area{ "flat_mux" }.enter();
		HCL_NAMED(selector);
		HCL_NAMED(flat_array);

		const size_t num_entries = selector.width().count();
		SymbolSelect sym{ flat_array.width() / num_entries };
#if 0		
		T selected = flat_array(sym[0]);
		for (size_t i = 1; i < num_entries; ++i)
		{
			IF(selector == i)
				selected = flat_array(sym[i]);
		}
#else
		hlim::Node_Multiplexer *node = DesignScope::createNode<hlim::Node_Multiplexer>(num_entries);
		node->recordStackTrace();
		node->connectSelector(selector.readPort());

		for (size_t i = 0; i < num_entries; ++i) {
			T input = flat_array(sym[i]);
			node->connectInput(i, input.readPort());
		}

		T selected{SignalReadPort(node)};
#endif		
		HCL_NAMED(selected);
		return selected;
	}

	template<BitVectorDerived T>
	T muxWord(Bit selector, T flat_array)
	{
		HCL_DESIGNCHECK_HINT(flat_array.width().divisibleBy(2), "flat array must be evenly divisible");

		auto entity = Area{ "flat_mux" }.enter();
		HCL_NAMED(selector);
		HCL_NAMED(flat_array);

		SymbolSelect sym{ flat_array.width() / 2 };
		T selected = flat_array(sym[0]);
		IF(selector)
			selected = flat_array(sym[1]);

		HCL_NAMED(selected);
		return selected;
	}

	BVec muxWord(UInt selector, BVec flat_array) { return muxWord<BVec>(selector, flat_array); }
	BVec muxWord(Bit selector, BVec flat_array) { return muxWord<BVec>(selector, flat_array); }
	UInt muxWord(UInt selector, UInt flat_array) { return muxWord<UInt>(selector, flat_array); }
	UInt muxWord(Bit selector, UInt flat_array) { return muxWord<UInt>(selector, flat_array); }
	SInt muxWord(UInt selector, SInt flat_array) { return muxWord<SInt>(selector, flat_array); }
	SInt muxWord(Bit selector, SInt flat_array) { return muxWord<SInt>(selector, flat_array); }

	UInt demux(const UInt& selector, const Bit& input, const Bit& inactiveOutput)
	{
		std::vector<Bit> ret{ (size_t)selector.width().count(), inactiveOutput };
		for (size_t i = 0; i < ret.size(); ++i)
			IF(selector == i)
				ret[i] = input;
		return pack(ret);
	}

	UInt demux(const UInt& selector)
	{
		std::vector<Bit> ret;
		ret.reserve(selector.width().count());
		for (size_t i = 0; i < selector.width().count(); ++i)
			ret.emplace_back(selector == i);
		return pack(ret);
	}

	SignalTapHelper sim_assert(Bit condition)
	{
		if(auto* scope = ConditionalScope::get())
			condition |= !Bit(SignalReadPort{ scope->getFullCondition() });

		SignalTapHelper helper(hlim::Node_SignalTap::LVL_ASSERT);
		helper.triggerIfNot(condition);
		return helper;
	}

	SignalTapHelper sim_warnIf(Bit condition)
	{
		if(auto* scope = ConditionalScope::get())
			condition |= !Bit(SignalReadPort{ scope->getFullCondition() });

		SignalTapHelper helper(hlim::Node_SignalTap::LVL_WARN);
		helper.triggerIf(condition);
		return helper;
	}


	SignalTapHelper sim_debug()
	{
		SignalTapHelper helper(hlim::Node_SignalTap::LVL_DEBUG);
		if (ConditionalScope::get() != nullptr)
			helper.triggerIf(Bit(SignalReadPort{ConditionalScope::get()->getFullCondition()}));
		return helper;
	}

	SignalTapHelper sim_debugAlways()
	{
		SignalTapHelper helper(hlim::Node_SignalTap::LVL_DEBUG);
		return helper;
	}

	SignalTapHelper sim_debugIf(Bit condition)
	{
		if(auto* scope = ConditionalScope::get())
			condition |= !Bit(SignalReadPort{ scope->getFullCondition() });

		SignalTapHelper helper(hlim::Node_SignalTap::LVL_DEBUG);
		helper.triggerIf(condition);
		return helper;
	}

	void internal::tap(const ElementarySignal& signal)
	{
		hlim::SignalAttributes att;
		att.userDefinedVendorAttributes["xilinx"]["mark_debug"] = { .type = "string", .value = "\"true\"" };
		att.userDefinedVendorAttributes["intel_quartus"]["preserve"] = { .type = "boolean", .value = "true" };
		attribute((ElementarySignal&)signal, att);

		auto *node = DesignScope::createNode<hlim::Node_SignalTap>();
		node->recordStackTrace();
		node->setLevel(hlim::Node_SignalTap::LVL_WATCH);
		node->setName(std::string(signal.getName()));

		node->addInput(signal.readPort());
	}	
}
