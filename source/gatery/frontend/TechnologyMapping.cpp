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

#include "Scope.h"

#include "TechnologyMapping.h"

#include "../hlim/NodeGroup.h"
#include "../hlim/Circuit.h"
#include "../hlim/coreNodes/Node_Signal.h"

#include <set>

namespace gtry {


TechnologyMappingPattern::TechnologyMappingPattern()
{
}


void TechnologyMapping::apply()
{
	for (auto &g : DesignScope::get()->getCircuit().getRootNodeGroup()->getChildren())
		apply(g.get());
}

void TechnologyMapping::apply(hlim::NodeGroup *nodeGroup)
{
	bool handled = false;
	{
		// put everything by default into parent group
		GroupScope scope(nodeGroup->getParent());

		for (auto &pattern : m_patterns)
			if (pattern->attemptApply(nodeGroup)) {
				handled = true;
				break;
			}

	}

	if (!handled)
		for (auto &g : nodeGroup->getChildren())
			apply(g.get());
}


}

