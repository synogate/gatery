#include "Signal.h"
#include "Scope.h"

#include <hcl/hlim/coreNodes/Node_Rewire.h>

hcl::core::frontend::SignalReadPort hcl::core::frontend::SignalReadPort::expand(size_t width) const
{
	hlim::ConnectionType type = connType(*this);
	HCL_DESIGNCHECK_HINT(type.width <= width, "signal width cannot be implicitly decreased");
	HCL_DESIGNCHECK_HINT(type.width == width || expansionPolicy != Expansion::none, "missmatching operands size and no expansion policy specified");

	hlim::NodePort ret{ .node = this->node, .port = this->port };

	if (type.width < width && expansionPolicy != Expansion::none ||
		type.width == width && type.interpretation == hlim::ConnectionType::BOOL)
	{
		auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
		rewire->connectInput(0, ret);

		switch (expansionPolicy)
		{
		case Expansion::one:	rewire->setPadTo(width, hlim::Node_Rewire::OutputRange::CONST_ONE);		break;
		case Expansion::zero:	rewire->setPadTo(width, hlim::Node_Rewire::OutputRange::CONST_ZERO);	break;
		case Expansion::sign:	rewire->setPadTo(width);	break;
		default: break;
		}

		ret = hlim::NodePort{ .node = rewire, .port = 0 };
	}
	return SignalReadPort{ ret, expansionPolicy };
}
