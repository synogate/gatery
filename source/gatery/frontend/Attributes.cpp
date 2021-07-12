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

#include "Attributes.h"

#include "Bit.h"
#include "BitVector.h"

#include "DesignScope.h"
#include "../hlim/supportNodes/Node_PathAttributes.h"

namespace gtry {

void setAttrib(Bit &bit, SignalAttributes attributes)
{
	bit.setAttrib(std::move(attributes));
}

void setAttrib(BVec &bvec, SignalAttributes attributes)
{
	bvec.setAttrib(std::move(attributes));
}

template<class Signal>
void setPathAttribImpl(Signal &start, Signal &end, PathAttributes attributes)
{
    auto* node = DesignScope::createNode<hlim::Node_PathAttributes>();
    node->getAttribs() = std::move(attributes);
    node->connectStart(start.getReadPort());
    node->connectEnd(end.getReadPort());
}

void setPathAttrib(Bit &start, Bit &end, PathAttributes attributes)
{
    setPathAttribImpl(start, end, std::move(attributes));
}

void setPathAttrib(BVec &start, BVec &end, PathAttributes attributes)
{
    setPathAttribImpl(start, end, std::move(attributes));
}


}