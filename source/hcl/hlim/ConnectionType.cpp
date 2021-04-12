/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

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
#include "ConnectionType.h"

namespace hcl::hlim {


bool ConnectionType::operator==(const ConnectionType &rhs) const
{
    if (rhs.interpretation != interpretation) return false;
    if (rhs.width != width) return false;

    return true;
}
    
#if 0

size_t CompoundConnectionType::getTotalWidth() const 
{
    size_t sum = 0;
    for (const auto &sub : m_subConnections)
        sum += sub.type->getTotalWidth();
    return sum;
}

#endif

}

