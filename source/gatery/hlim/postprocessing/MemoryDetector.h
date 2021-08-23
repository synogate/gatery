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

#include "../NodePtr.h"

#include "../NodeGroup.h"

namespace gtry::hlim {

class Circuit;
class Node_Memory;
class Node_MemPort;
class Node_Register;

class MemoryGroup : public NodeGroup
{
    public:
        struct WritePort {
            NodePtr<Node_MemPort> node;
        };

        struct ReadPort {
            NodePtr<Node_MemPort> node;
            std::vector<NodePtr<Node_Register>> dedicatedReadLatencyRegisters;
            RefCtdNodePort dataOutput;

            bool findOutputRegisters(size_t readLatency, NodeGroup *memoryNodeGroup);
        };

        MemoryGroup();
        
        void formAround(Node_Memory *memory, Circuit &circuit);

        void convertToReadBeforeWrite(Circuit &circuit);
        void resolveWriteOrder(Circuit &circuit);
        void attemptRegisterRetiming(Circuit &circuit);
        void updateNoConflictsAttrib();
        void buildReset(Circuit &circuit);
        void verify();
        void replaceWithIOPins(Circuit &circuit);
        void bypassSignalNodes();

        Node_Memory *getMemory() { return m_memory; }
        const std::vector<WritePort> &getWritePorts() { return m_writePorts; }
        const std::vector<ReadPort> &getReadPorts() { return m_readPorts; }
    protected:
        NodePtr<Node_Memory> m_memory;
        std::vector<WritePort> m_writePorts;
        std::vector<ReadPort> m_readPorts;

        NodeGroup *m_fixupNodeGroup = nullptr;

        void lazyCreateFixupNodeGroup();

        void ensureNotEnabledFirstCycles(Circuit &circuit, NodeGroup *ng, Node_MemPort *writePort, size_t numCycles);


        void buildResetLogic(Circuit &circuit);
        void buildResetRom(Circuit &circuit);

};



void findMemoryGroups(Circuit &circuit);
void buildExplicitMemoryCircuitry(Circuit &circuit);


}
