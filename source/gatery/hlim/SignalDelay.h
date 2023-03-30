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

#include <gatery/utils/StableContainers.h>

#include <map>
#include <span>
#include <vector>

#include "../utils/Exceptions.h"
#include "../utils/Preprocessor.h"

namespace gtry::hlim {

struct NodePort;
class Circuit;
class Subnet;

class SignalDelay {
	public:
		void compute(const Subnet &subnet);

		inline bool contains(const NodePort &np) const { return m_outputToBitDelays.contains(np); }
		std::span<float> getDelay(const NodePort &np);
		std::span<const float> getDelay(const NodePort &np) const;
	protected:
		mutable std::vector<float> m_zeros;
		std::vector<float> m_delays;
		utils::UnstableMap<NodePort, std::span<float>> m_outputToBitDelays;


		void allocate(const Subnet &subnet);
		void zero();
};

}
