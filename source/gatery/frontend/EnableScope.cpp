/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

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

#include "EnableScope.h"
#include "DesignScope.h"

#include "Signal.h"
#include "Bit.h"

#include <gatery/hlim/coreNodes/Node_Logic.h>


namespace gtry {

EnableScope::EnableScope(const Bit &enableCondition)
{
	setEnable(enableCondition.readPort(), true);
}

EnableScope::EnableScope(EnableScope::Always)
{
	setEnable(Bit('1').readPort(), false);
}

EnableScope::~EnableScope()
{

}

Bit EnableScope::getFullEnable()
{
	if (m_currentScope == nullptr)
		return '1';
	else
		return Bit(SignalReadPort(m_currentScope->getFullEnableCondition()));
}

void EnableScope::setEnable(const hlim::NodePort &enableCondition, bool checkParent)
{
	m_fullEnableCondition = m_enableCondition = enableCondition;

	if (checkParent && m_parentScope)
	{
		hlim::Node_Logic* andNode = DesignScope::createNode<hlim::Node_Logic>(hlim::Node_Logic::AND);
		andNode->connectInput(0, m_enableCondition);
		andNode->connectInput(1, m_parentScope->m_fullEnableCondition);
		
		m_fullEnableCondition = { .node = andNode, .port = 0ull };
	}
}

}
