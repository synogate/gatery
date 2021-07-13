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
struct NodePort;
class Circuit;
class Subnet;
class Node_MemPort;

/**
 * @brief Retimes registers forward such that a register is placed after the specified output port without changing functionality.
 * @details Retiming is limited to the specified area.
 * @param circuit The circuit to operate on
 * @param area The area to which retiming is to be restricted. The Subnet is modified to include the newly created registers.
 * @param anchoredRegisters Set of registers that are not to be moved (e.g. because they are already deemed in a good location).
 * @param output The output that shall recieve a register.
 * @param ignoreRefs Whether or not to throw an exception if a node has to be retimed to which a reference exists.
 * @param failureIsError Whether to throw an exception if the retiming is unsuccessful
 * @returns Whether the retiming was successful
 */
bool retimeForwardToOutput(Circuit &circuit, Subnet &area, const std::set<Node_Register*> &anchoredRegisters, NodePort output, bool ignoreRefs = false, bool failureIsError = true);

/**
 * @brief Performs forward retiming on an area based on rough timing estimates.
 * @param circuit The circuit to operate on
 * @param subnet The area to which retiming is to be restricted. The Subnet is modified to include the newly created registers.
 */
void retimeForward(Circuit &circuit, Subnet &subnet);


/**
 * @brief Retimes registers backwards such that a register is placed after the specified output port without changing functionality.
 * @param circuit The circuit to operate on
 * @param subnet The area to which retiming is to be restricted. The Subnet is modified to include the newly created registers.
 * @param anchoredRegisters Set of registers that are not to be moved (e.g. because they are already deemed in a good location).
 * @param retimeableWritePorts List of write ports that may be retimed (requires additional RMW hazard detection to be build outside of this function).
 * @param retimedArea The area that was retimed (including registers and potential write ports)
 * @param output The output that shall recieve a register.
 * @param ignoreRefs Whether or not to throw an exception if a node has to be retimed to which a reference exists.
 * @param failureIsError Whether to throw an exception if the retiming is unsuccessful
 * @returns Whether the retiming was successful
 */
bool retimeBackwardtoOutput(Circuit &circuit, Subnet &subnet, const std::set<Node_Register*> &anchoredRegisters, const std::set<Node_MemPort*> &retimeableWritePorts,
                        Subnet &retimedArea, NodePort output, bool ignoreRefs = false, bool failureIsError = true);

}
