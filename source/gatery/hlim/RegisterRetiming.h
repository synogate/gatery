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
#include "NodePort.h"
#include "Subnet.h"
#include "../simulation/BitVectorState.h"

#include <map>
#include <vector>

namespace gtry::hlim {

class Node_Register;
struct NodePort;
class Circuit;
class NodeGroup;
class Subnet;
class Node_MemPort;
class Clock;

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


/**
* @brief Builds read-modify-write logic for circuits where the write has to happen potentially multiple cycles after the corresponding read, but must appear for following reads to happen instantaneous.
*/
class ReadModifyWriteHazardLogicBuilder
{
    public:

        struct ReadPort {
            /// Input of the address of this port
            NodePort addrInputDriver;
            /// Input of the enable of this port
            NodePort enableInputDriver;
            /// Data output of this port (after the port's registers to model read latency)
            NodePort dataOutOutputDriver;
        };

        struct WritePort {
            /// Input of the address of this port
            NodePort addrInputDriver;
            /// Input of the enable of this port
            NodePort enableInputDriver;
            /// Mask to enable only portions of the write (like a byte enable mask).
            /// Width of dataInInput must be a multiple of the width of enableMaskInput.
            NodePort enableMaskInputDriver;
            /// Data input of this port
            NodePort dataInInputDriver;
        };

        ReadModifyWriteHazardLogicBuilder(Circuit &circuit, Clock *clockDomain) : m_circuit(circuit), m_clockDomain(clockDomain) { }

        /// Workaround for now until I can detect the read delay automatically
        void setReadLatency(size_t readLatency) { m_readLatency = readLatency; }

        /// Whether to retime one register to the input of the bypass mux to improve timing.
        void retimeRegisterToMux() { m_retimeToMux = true; }
        /// Global enable to attach to all new registers (e.g. for stalling).
        void setGlobalEnable(NodePort globalEnable) { m_globalEnable = globalEnable; }

        /// Adds a new read port to consider. Within a cycle, all read ports are assumed to be read before write wrt. all write ports.
        inline void addReadPort(const ReadPort &readPort) { m_readPorts.push_back(readPort); }
        /// Adds a new write port. Write ports are assumed to write (and overwrite) in the order in which they are given.
        inline void addWritePort(const WritePort &writePort) { m_writePorts.push_back(writePort); }

        /**
         * @param failureIsError Whether to throw an exception if the retiming is unsuccessful
         * @returns Whether the rmw logic was successfuly created
         */
        bool build(bool failureIsError = true);

        /// Returns a subnet in which all newly created nodes were placed.
        inline const Subnet &getNewNodes() const { return m_newNodes; }
    protected:
        Circuit &m_circuit;
        Clock *m_clockDomain;
        Subnet m_newNodes;
        NodePort m_globalEnable;
        std::vector<ReadPort> m_readPorts;
        std::vector<WritePort> m_writePorts;
        bool m_retimeToMux = false;

        size_t m_readLatency = ~0u;

        void determineResetValues(std::map<NodePort, sim::DefaultBitVectorState> &resetValues);

        NodePort createRegister(NodePort nodePort, const sim::DefaultBitVectorState &resetValue);

        NodePort buildConflictDetection(NodePort rdAddr, NodePort rdEn, NodePort wrAddr, NodePort wrEn, NodePort mask, size_t maskBit);

        std::vector<NodePort> splitWords(NodePort data, NodePort mask);
        NodePort joinWords(const std::vector<NodePort> &words);
};

}
