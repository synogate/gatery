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
#include "SignalCompareOp.h"
#include "DesignScope.h"
#include "SignalLogicOp.h"
#include "ConditionalScope.h"
#include "SignalArithmeticOp.h"

#include <gatery/hlim/coreNodes/Node_Compare.h>


namespace gtry {

	SignalReadPort makeNode(hlim::Node_Compare::Op op, NormalizedWidthOperands ops)
	{
		auto* node = DesignScope::createNode<hlim::Node_Compare>(op);
		node->recordStackTrace();
		node->connectInput(0, ops.lhs);
		node->connectInput(1, ops.rhs);

		return SignalReadPort(node);
	}

	Bit gt(const SInt& lhs, const SInt& rhs) {
		return (rhs - lhs).sign();
	}
	Bit lt(const SInt& lhs, const SInt& rhs) {
		return (lhs - rhs).sign();
	}
	Bit geq(const SInt& lhs, const SInt& rhs) {
		return !lt(lhs, rhs);
	}
	Bit leq(const SInt& lhs, const SInt& rhs) {
		return !gt(lhs, rhs);
	}


}