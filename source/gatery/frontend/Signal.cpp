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
#include "Signal.h"
#include "ConditionalScope.h"
#include "DesignScope.h"

#include <gatery/utils/Enumerate.h>
#include <gatery/utils/Preprocessor.h>
#include <gatery/utils/Traits.h>

#include <gatery/hlim/coreNodes/Node_Rewire.h>
#include <gatery/hlim/coreNodes/Node_Signal.h>
#include <gatery/hlim/supportNodes/Node_Attributes.h>

gtry::SignalReadPort gtry::SignalReadPort::expand(size_t width, hlim::ConnectionType::Type resultType) const
{
	hlim::ConnectionType type = connType(*this);
	HCL_DESIGNCHECK_HINT(type.width <= width, "signal width cannot be implicitly decreased");
	HCL_DESIGNCHECK_HINT(type.width == width || expansionPolicy != Expansion::none, "missmatching operands size and no expansion policy specified");

	hlim::NodePort ret{ .node = this->node, .port = this->port };

	if (type.width < width || type.type != resultType)
	{
		auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
		rewire->changeOutputType(hlim::ConnectionType{ .type = resultType, .width = width });
		rewire->connectInput(0, ret);

		switch (expansionPolicy)
		{
		case Expansion::one:	rewire->setPadTo(width, hlim::Node_Rewire::OutputRange::CONST_ONE);		break;
		case Expansion::zero:	rewire->setPadTo(width, hlim::Node_Rewire::OutputRange::CONST_ZERO);	break;
		case Expansion::sign:	rewire->setPadTo(width);	break;
		default: 
				HCL_ASSERT(type.width == width);
				rewire->setConcat();
				break;
		}
/*
		// This actually does more harm than good
		auto* signal = DesignScope::createNode<hlim::Node_Signal>();
		signal->connectInput({ .node = rewire, .port = 0 });
		signal->setName(node->getName());
		ret = hlim::NodePort{ .node = signal, .port = 0 };
*/
		ret = hlim::NodePort{ .node = rewire, .port = 0 };
	}
	return SignalReadPort{ ret, expansionPolicy };
}

gtry::ElementarySignal::ElementarySignal()
{
	if (auto* scope = ConditionalScope::get())
		m_initialScopeId = scope->getId();
}

gtry::ElementarySignal::~ElementarySignal()
{
}

void gtry::ElementarySignal::attribute(const hlim::SignalAttributes &attributes)
{
	auto* node = DesignScope::createNode<hlim::Node_Attributes>();
	node->getAttribs() = std::move(attributes);
	node->connectInput(readPort());
}
