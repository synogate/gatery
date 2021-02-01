#pragma once

#include "NodeGroup.h"

namespace hcl::core::hlim {

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