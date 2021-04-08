/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "SignalLogicOp.h"

namespace hcl::core::frontend {

    SignalReadPort makeNode(hlim::Node_Logic::Op op, NormalizedWidthOperands ops)
    {
        HCL_DESIGNCHECK_HINT(op != hlim::Node_Logic::NOT, "Trying to perform a not operation with two operands.");

        hlim::Node_Logic* node = DesignScope::createNode<hlim::Node_Logic>(op);
        node->recordStackTrace();
        node->connectInput(0, ops.lhs);
        node->connectInput(1, ops.rhs);

        return SignalReadPort(node);
    }

    SignalReadPort makeNode(hlim::Node_Logic::Op op, const ElementarySignal& in)
    {
        HCL_DESIGNCHECK_HINT(op == hlim::Node_Logic::NOT, "Trying to perform a non-not operation with one operand.");

        hlim::Node_Logic* node = DesignScope::createNode<hlim::Node_Logic>(op);
        node->recordStackTrace();
        node->connectInput(0, in.getReadPort());

        return SignalReadPort(node);
    }

}
