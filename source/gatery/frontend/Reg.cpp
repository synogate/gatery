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
#include "Reg.h"

#include "Clock.h"
#include "ConditionalScope.h"
#include "DesignScope.h"
#include "Scope.h"

#include <gatery/hlim/Clock.h>
#include <gatery/hlim/coreNodes/Node_Register.h>

namespace gtry
{
	SignalReadPort internal::reg(SignalReadPort val, std::optional<SignalReadPort> reset, const RegisterSettings& settings)
	{
		auto* reg = DesignScope::createNode<hlim::Node_Register>();

		reg->connectInput(hlim::Node_Register::DATA, val);

		if(reset)
			reg->connectInput(hlim::Node_Register::RESET_VALUE, *reset);

		if(settings.clock)
			reg->setClock(settings.clock->getClk());
		else
			reg->setClock(ClockScope::getClk().getClk());

		if(settings.allowRetimingForward)
			reg->getFlags().insert(hlim::Node_Register::Flags::ALLOW_RETIMING_FORWARD);

		if(settings.allowRetimingBackward)
			reg->getFlags().insert(hlim::Node_Register::Flags::ALLOW_RETIMING_BACKWARD);

		EnableScope* scope = EnableScope::get();
		if(scope)
			reg->connectInput(hlim::Node_Register::ENABLE, scope->getFullEnableCondition());

		return SignalReadPort(reg, val.expansionPolicy);
	}
}
