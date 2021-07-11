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
#pragma once

#include <set>

#include "../utils/Exceptions.h"
#include "../utils/Preprocessor.h"

namespace gtry::hlim {

class Node_Register;
class NodePort;
class Circuit;
class Subnet;

/**
 * @brief Retimes registers forward such that a register is placed after the specified output port without changing functionality.
 * @details Retiming is limited to the specified area.
 * @param circuit The circuit to operate on
 * @param area The area to which retiming is to be restricted.
 * @param anchoredRegisters Set of registers that are not to be moved (e.g. because they are already deemed in a good location).
 * @param output The output that shall recieve a register.
 * @param ignoreRefs Whether or not to throw an exception if a node has to be retimed to which a reference exists.
 */
void retimeForwardToOutput(Circuit &circuit, const Subnet &area, const std::set<Node_Register*> &anchoredRegisters, NodePort output, bool ignoreRefs = false);

/**
 * @brief Performs forward retiming on an area based on rough timing estimates.
 * @param circuit The circuit to operate on
 * @param subnet The area to which retiming is to be restricted.
 */
void retimeForward(Circuit &circuit, const Subnet &subnet);

}
