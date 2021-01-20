#include "BlockRamDetector.h"

#include "Circuit.h"

#include "coreNodes/Node_Signal.h"
#include "coreNodes/Node_Register.h"
#include "coreNodes/Node_Constant.h"
#include "supportNodes/Node_Memory.h"
#include "supportNodes/Node_MemReadPort.h"
#include "supportNodes/Node_MemWritePort.h"


namespace hcl::core::hlim {

void handleMemory(Circuit &circuit, Node_Memory *memory) 
{
    // list of everything that becomes part of the "bram entity"
    std::vector<BaseNode*> bramComponents;
    bramComponents.push_back(memory);

    auto traceToRegOrConst = [&](NodePort np)->bool {
        while (true) {
            bramComponents.push_back(np.node);

            // note: this assumes that the same data/adress is not send to multiple ports (critical for constants, this will need fixing)
            // But we can't have any reuse of some internal signals of the bram outside the bram.
            if (np.node->getDirectlyDriven(np.port).size() != 1) return false;

            if (dynamic_cast<Node_Register*>(np.node) != nullptr) {
                return true;
            } else
            if (dynamic_cast<Node_Constant*>(np.node) != nullptr) {
                return true;
            } else
            if (dynamic_cast<Node_Signal*>(np.node) != nullptr) {
                np = np.node->getDriver(0);
            } else
                return false;
        }
    };

    // Check all write ports
    for (auto i : utils::Range(memory->getNumInputPorts())) {
        auto *writePort = dynamic_cast<Node_MemWritePort*>(memory->getDriver(i).node);
        bramComponents.push_back(writePort);
        // address and data inputs must be const or registers
        if (!traceToRegOrConst(writePort->getDriver((size_t)Node_MemWritePort::Inputs::address))) return;
        if (!traceToRegOrConst(writePort->getDriver((size_t)Node_MemWritePort::Inputs::data))) return;
    }

    // Check all read ports
    for (auto &np : memory->getDirectlyDriven(0)) {
        auto *readPort = dynamic_cast<Node_MemReadPort*>(np.node);
        bramComponents.push_back(readPort);
        // address input must be const or register
        if (!traceToRegOrConst(readPort->getDriver((size_t)Node_MemReadPort::Inputs::address))) return;


        std::vector<BaseNode*> optionalBramComponents;
        // detect optional output register
        if (readPort->getDirectlyDriven((size_t)Node_MemReadPort::Outputs::data).size() == 1) { // only possible if everything goes through register

            // trace along signal nodes to find non-signal or branching node
            BaseNode *nextNode = readPort->getDirectlyDriven((size_t)Node_MemReadPort::Outputs::data).front().node;
            while (dynamic_cast<Node_Signal*>(nextNode) != nullptr && nextNode->getDirectlyDriven(0).size() == 1) {
                optionalBramComponents.push_back(nextNode);
                nextNode = nextNode->getDirectlyDriven(0).front().node;
            }

            // only if the end of the non-branching signal chain is a register then all of that chain can be part of the bram
            if (dynamic_cast<Node_Register*>(nextNode) != nullptr) {
                bramComponents.push_back(nextNode);
                for (auto opt : optionalBramComponents)
                    bramComponents.push_back(opt);
            }
        }
    }

    // If we got to here, it's a bram. Create a nodegroup and move everything in there.

    auto *bramGroup = memory->getGroup()->addChildNodeGroup(NodeGroup::GroupType::ENTITY);
    bramGroup->setName("bram");
    bramGroup->setComment("Auto generated");
    for (auto node : bramComponents)
        node->moveToGroup(bramGroup);
}

void findBlockRams(Circuit &circuit)
{
    for (auto &node : circuit.getNodes())
        if (auto *memory = dynamic_cast<Node_Memory*>(node.get()))
            handleMemory(circuit, memory);
}

}