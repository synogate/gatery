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

#include "DesignScope.h"
#include "Bit.h"

#include <gatery/utils/Traits.h>
#include <gatery/hlim/coreNodes/Node_PriorityConditional.h>

#include <vector>

namespace gtry {
	
template<BaseSignal DataSignal>
class PriorityConditional
{
	public:
		PriorityConditional<DataSignal> &addCondition(const Bit &enableSignal, const DataSignal &value) {
			m_choices.push_back({});
			m_choices.back().first = enableSignal;
			m_choices.back().first.setName("");
			m_choices.back().second = value;
			m_choices.back().second.setName("");
			return *this;
		}

		DataSignal operator()(const DataSignal &defaultCase) {
			hlim::Node_PriorityConditional *node = DesignScope::createNode<hlim::Node_PriorityConditional>();
			node->recordStackTrace();
			node->connectDefault(defaultCase.readPort());
			
			for (auto &c : m_choices)
				node->addInput(c.first.readPort(), c.second.readPort());

			return SignalReadPort(node);
		}
	protected:
		std::vector<std::pair<Bit, DataSignal>> m_choices;
};



}
