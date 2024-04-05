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

#include "../Node.h"

#include <boost/variant.hpp>

namespace gtry::hlim {

class Node_SignalTap : public Node<Node_SignalTap>
{
	public:
		enum Level {
			LVL_ASSERT,
			LVL_WARN,
			LVL_DEBUG,
			LVL_WATCH
		};
		enum Trigger {
			TRIG_ALWAYS,
			TRIG_FIRST_INPUT_HIGH,
			TRIG_FIRST_INPUT_LOW,
			TRIG_FIRST_CLOCK
		};
		
		struct FormattedSignal {
			unsigned inputIdx;
			unsigned format;
		};
		
		typedef std::variant<std::string, FormattedSignal> LogMessagePart;
		
		inline void setLevel(Level level) { m_level = level; }
		inline Level getLevel() const { return m_level; }
		
		inline void setTrigger(Trigger trigger) { m_trigger = trigger; }
		inline Trigger getTrigger() const { return m_trigger; }
		
		void addInput(hlim::NodePort input);
		inline void addMessagePart(LogMessagePart part) { m_logMessage.push_back(std::move(part)); }
		
		virtual void simulateCommit(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets) const override;
		
		virtual std::string getTypeName() const override;
		virtual void assertValidity() const override;
		virtual std::string getInputName(size_t idx) const override;
		virtual std::string getOutputName(size_t idx) const override;

		virtual std::vector<size_t> getInternalStateSizes() const override;
		
		virtual bool hasSideEffects() const override { return true; }

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;
	protected:
		Level m_level = LVL_DEBUG;
		Trigger m_trigger = TRIG_ALWAYS;
		//bool m_triggerOnlyOnce;
		
		std::vector<LogMessagePart> m_logMessage;
};

}
