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
#include "Node_SignalTap.h"

#include <gatery/simulation/BitVectorState.h>
#include <gatery/simulation/SimulatorCallbacks.h>

#include <boost/format.hpp>

#include <sstream>

namespace gtry::hlim {

void Node_SignalTap::addInput(hlim::NodePort input)
{
	resizeInputs(getNumInputPorts()+1);
	connectInput(getNumInputPorts()-1, input);
}

void Node_SignalTap::simulateCommit(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets) const
{
	if (m_level == LVL_WATCH)
		return;

	bool triggered;
	if (m_trigger == TRIG_ALWAYS) {
		triggered = true;
	} else {
		HCL_ASSERT_HINT(getNumInputPorts() > 0, "Missing condition input signal!");
		if (inputOffsets[0] == ~0ull) {
			triggered = true; // unconnected is undefined and undefined always triggers.
		} else {
			auto conditionDriver = getDriver(0);
			const auto &conditionType = hlim::getOutputConnectionType(conditionDriver);
			HCL_ASSERT_HINT(conditionType.width == 1, "Condition must be 1 bit!");

			bool defined = state.get(sim::DefaultConfig::DEFINED, inputOffsets[0]);
			bool value = state.get(sim::DefaultConfig::VALUE, inputOffsets[0]);

			triggered = !defined || (m_trigger == TRIG_FIRST_INPUT_HIGH && value) || (m_trigger == TRIG_FIRST_INPUT_LOW && !value);
		}
	}

	if (triggered) {
		struct Concatenator : public boost::static_visitor<> {
			void operator()(const std::string &str) {
				message << str;
			}
			void operator()(const FormattedSignal &signal) {  /// @todo: invoke pretty printer
				auto driver = node->getNonSignalDriver(signal.inputIdx);
				if (driver.node == nullptr) {
					message << "UNCONNECTED";
					return;
				}
				const auto &type = hlim::getOutputConnectionType(driver);
				for (auto i : utils::Range(type.width)) {
					if (state.get(sim::DefaultConfig::DEFINED, inputOffsets[signal.inputIdx]+type.width-1-i)) {
						if (state.get(sim::DefaultConfig::VALUE, inputOffsets[signal.inputIdx]+type.width-1-i))
							message << '1';
						else
							message << '0';
					} else
						message << 'X';
				}
			}

			const Node_SignalTap *node;
			sim::DefaultBitVectorState &state;
			const size_t *inputOffsets;
			std::stringstream message;

			Concatenator(const Node_SignalTap *node, sim::DefaultBitVectorState &state, const size_t *inputOffsets) : node(node), state(state), inputOffsets(inputOffsets) { }
		};

		Concatenator concatenator(this, state, inputOffsets);
		for (const auto &part : m_logMessage)
			std::visit(concatenator, part);

		switch (m_level) {
			case LVL_WATCH:
			break;
			case LVL_ASSERT:
				simCallbacks.onAssert(this, concatenator.message.str());
			break;
			case LVL_WARN:
				simCallbacks.onWarning(this, concatenator.message.str());
			break;
			case LVL_DEBUG:
				simCallbacks.onDebugMessage(this, concatenator.message.str());
			break;
		}
	}
}

std::string Node_SignalTap::getTypeName() const
{
	switch (m_level) {
		case LVL_ASSERT: return "sig_tap_assert";
		case LVL_WARN: return "sig_tap_warn";
		case LVL_DEBUG: return "sig_tap_debug";
		default: return "sig_tap";
	}
}

void Node_SignalTap::assertValidity() const
{
}

std::string Node_SignalTap::getInputName(size_t idx) const
{
	if (m_trigger != TRIG_ALWAYS && idx == 0)
		return "trigger";
	return (boost::format("input_%d") % idx).str();
}

std::string Node_SignalTap::getOutputName(size_t idx) const
{
	return "";
}


std::vector<size_t> Node_SignalTap::getInternalStateSizes() const
{
	return {};
}




std::unique_ptr<BaseNode> Node_SignalTap::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> res(new Node_SignalTap());
	copyBaseToClone(res.get());
	((Node_SignalTap*)res.get())->m_level = m_level;
	((Node_SignalTap*)res.get())->m_trigger = m_trigger;
	((Node_SignalTap*)res.get())->m_logMessage = m_logMessage;
	return res;
}




}
