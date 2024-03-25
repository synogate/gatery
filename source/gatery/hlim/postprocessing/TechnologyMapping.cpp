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

#include "TechnologyMapping.h"
#include "MemoryDetector.h"

#include "../NodeGroup.h"
#include "../Circuit.h"

namespace gtry::hlim {


TechnologyMappingPattern::TechnologyMappingPattern()
{

}

TechnologyMapping::TechnologyMapping()
{
	addPattern(std::make_unique<Memory2VHDLPattern>());
}

void TechnologyMapping::addPattern(std::unique_ptr<TechnologyMappingPattern> pattern) 
{ 
	m_patterns.push_back(std::move(pattern));
	std::sort(m_patterns.begin(), m_patterns.end(), [](const auto &lhs, const auto &rhs)->bool {
		return lhs->getPriority() < rhs->getPriority();
	});
}


void TechnologyMapping::apply(Circuit &circuit, hlim::NodeGroup *nodeGroup, bool preOptimization) const
{
	bool handled = false;
	{
		for (auto &pattern : m_patterns)
			if ((preOptimization == pattern->runPreOptimization()) && pattern->attemptApply(circuit, nodeGroup)) {
				handled = true;
				break;
			}
	}

	if (!handled)
		for (size_t i = 0; i < nodeGroup->getChildren().size(); i++) {
			auto &g = nodeGroup->getChildren()[i];
			apply(circuit, g.get(), preOptimization);
		}
}


}

