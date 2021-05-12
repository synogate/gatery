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

#include "NodeGroup.h"

namespace gtry::hlim {

class Circuit;
class Node_Memory;
class Node_MemPort;
class Node_Register;

class MemoryGroup : public NodeGroup
{
    public:
        struct WritePort {
            Node_MemPort *node = nullptr;
        };

        struct ReadPort {
            Node_MemPort *node = nullptr;
            Node_Register *syncReadDataReg = nullptr;
            Node_Register *outputReg = nullptr;
            
            NodePort dataOutput;
        };

        MemoryGroup();
        
        void formAround(Node_Memory *memory, Circuit &circuit);

        void convertPortDependencyToLogic(Circuit &circuit);
        void attemptRegisterRetiming(Circuit &circuit);
        void verify();

        Node_Memory *getMemory() { return m_memory; }
        const std::vector<WritePort> &getWritePorts() { return m_writePorts; }
        const std::vector<ReadPort> &getReadPorts() { return m_readPorts; }
    protected:
        Node_Memory *m_memory = nullptr;
        std::vector<WritePort> m_writePorts;
        std::vector<ReadPort> m_readPorts;

        NodeGroup *m_fixupNodeGroup = nullptr;

        void lazyCreateFixupNodeGroup();
};



void findMemoryGroups(Circuit &circuit);
void buildExplicitMemoryCircuitry(Circuit &circuit);


}
