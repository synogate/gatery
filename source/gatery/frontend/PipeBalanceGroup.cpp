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

#include "PipeBalanceGroup.h"
#include "DesignScope.h"
#include "EnableScope.h"

namespace gtry {

PipeBalanceGroup::PipeBalanceGroup()
{
	m_regSpawner = DesignScope::createNode<hlim::Node_RegSpawner>();

	EnableScope* scope = EnableScope::get();
	if (scope)
		m_regSpawner->setEnableCondition(scope->getFullEnableCondition());
}

void PipeBalanceGroup::verifyConsistentEnableScope()
{
	EnableScope* scope = EnableScope::get();
	if (scope) {
		HCL_DESIGNCHECK_HINT(m_regSpawner->getEnableCondition() == scope->getFullEnableCondition(), "Signals added to a pipeline balance group must not have different enable conditions. Usually this happens if the signals are added from different IF/ENIF/... scopes!");
	} else {
		HCL_DESIGNCHECK_HINT(m_regSpawner->getEnableCondition().node == nullptr, "Signals added to a pipeline balance group must not have different enable conditions. Usually this happens if the signals are added from different IF/ENIF/... scopes!");
	}
}

size_t PipeBalanceGroup::getNumPipeBalanceGroupStages() const
{ 
	HCL_DESIGNCHECK_HINT(m_regSpawner->wasResolved(), "The number of pipeline stages can only be queries after the retiming, at least on the part of the graph that is affected, has been performed!");
	return m_regSpawner->getNumStagesSpawned(); 
}

}
