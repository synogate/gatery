/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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

#include "../../hlim/NodePort.h"

#include <vector>

namespace gtry::sim {

class Simulator;

/**
 * @brief A set of signals that a simulation process can await changes on.
 */
class SensitivityList {
	public:
		inline const std::vector<hlim::NodePort> &getSignals() const { return m_signals; }
		inline void add(const hlim::NodePort &np) { m_signals.push_back(np); }
	protected:
		std::vector<hlim::NodePort> m_signals;
};


}