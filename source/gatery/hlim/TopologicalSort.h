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

#include <vector>
#include <set>

namespace gtry::hlim {

class BaseNode;
class Subnet;

class TopologicalSort {
	public:
		enum LoopHandling {
			LOOPS_ARE_ERRORS,
			SPLIT_LOOPS_LOWEST_ID,
			SET_LOOPS_ASIDE
		};

		const std::vector<BaseNode*> &sort(const Subnet &subnet, LoopHandling loopHandling = LOOPS_ARE_ERRORS);

		const utils::StableSet<BaseNode*> &getUnsortedNodes() const { return m_unsortedNodes; }
		utils::StableSet<BaseNode*> getLoop();
	protected:
		std::vector<BaseNode*> m_sortedNodes;
		utils::StableSet<BaseNode*> m_unsortedNodes;
};


}
