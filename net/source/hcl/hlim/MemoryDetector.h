#pragma once

#include "NodeGroup.h"

namespace hcl::core::hlim {

class Circuit;
class Node_Memory;
class Node_MemReadPort;
class Node_MemWritePort;
class Node_Register;

class MemoryGroup : public NodeGroup
{
    public:
        struct WritePort {
            Node_MemWritePort *node = nullptr;
        };

        struct ReadPort {
            Node_MemReadPort *node = nullptr;
            Node_Register *syncReadDataReg = nullptr;
            Node_Register *outputReg = nullptr;
            
            NodePort dataOutput;
        };

        MemoryGroup(Node_Memory *memory);

        Node_Memory *getMemory() { return m_memory; }
        const std::vector<WritePort> &getWritePorts() { return m_writePorts; }
        const std::vector<ReadPort> &getReadPorts() { return m_readPorts; }
    protected:
        Node_Memory *m_memory;
        std::vector<WritePort> m_writePorts;
        std::vector<ReadPort> m_readPorts;
};



void findMemoryGroups(Circuit &circuit);


}