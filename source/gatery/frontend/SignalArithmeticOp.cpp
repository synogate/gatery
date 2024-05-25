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
#include "DesignScope.h"
#include "SignalLogicOp.h"
#include "ConditionalScope.h"
#include "SignalArithmeticOp.h"

namespace gtry {


	SignalReadPort makeNode(hlim::Node_Arithmetic::Op op, NormalizedWidthOperands ops)
	{
		hlim::Node_Arithmetic* node = DesignScope::createNode<hlim::Node_Arithmetic>(op);
		node->recordStackTrace();
		node->connectInput(0, ops.lhs);
		node->connectInput(1, ops.rhs);
		
		return SignalReadPort(node);
	}

	SignalReadPort makeNode(hlim::Node_Arithmetic::Op op, NormalizedWidthOperands ops, const Bit &carry)
	{
		UInt zextCarry = zext(carry, ops.lhs.width());
		hlim::Node_Arithmetic* node = DesignScope::createNode<hlim::Node_Arithmetic>(op, 3);
		node->recordStackTrace();
		node->connectInput(0, ops.lhs);
		node->connectInput(1, ops.rhs);
		node->connectInput(2, zextCarry.readPort());
		
		return SignalReadPort(node);
	}

	SInt mul(const SInt& lhs, const SInt& rhs) {
		if(lhs.width() == rhs.width())
			return (SInt)mul((UInt)lhs, (UInt)rhs);

		Bit lhSign = lhs.sign();
		Bit rhSign = rhs.sign();
		Bit resultSign = lhSign ^ rhSign;

		UInt absLhs = abs(lhs);
		UInt absRhs = abs(rhs);

		UInt absRes = absLhs * absRhs;
		SInt res = (SInt) absRes;
		IF (resultSign)
			res = (SInt)( ~absRes + 1 );
		return res;
	}

	UInt abs(const std::same_as<SInt> auto &v) { 
		UInt res = (UInt) v;
		IF (v.sign())
			res = ~ (UInt)v + 1;
		return res;
	}
	template UInt abs<SInt>(const SInt &v);

}
