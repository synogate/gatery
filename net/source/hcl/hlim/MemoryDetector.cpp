#include "MemoryDetector.h"

#include "Circuit.h"

#include "coreNodes/Node_Signal.h"
#include "coreNodes/Node_Register.h"
#include "coreNodes/Node_Constant.h"
#include "supportNodes/Node_Memory.h"
#include "supportNodes/Node_MemReadPort.h"
#include "supportNodes/Node_MemWritePort.h"
#include "GraphExploration.h"


namespace hcl::core::hlim {


MemoryGroup::MemoryGroup(Node_Memory *memory) : NodeGroup(GroupType::SFU), m_memory(memory)
{
    m_memory->moveToGroup(this);

    for (auto &np : m_memory->getDirectlyDriven(0)) {
        // Check all write ports
        if (auto *writePort = dynamic_cast<Node_MemWritePort*>(np.node)) {
            m_writePorts.push_back({.node=writePort});
            writePort->moveToGroup(this);
        }
        // Check all read ports
        if (auto *readPort = dynamic_cast<Node_MemReadPort*>(np.node)) {
            m_readPorts.push_back({.node = readPort});
            ReadPort &rp = m_readPorts.back();
            readPort->moveToGroup(this);
            rp.dataOutput = {.node = readPort, .port = (size_t)Node_MemReadPort::Outputs::data};

            NodePort readPortEnable = readPort->getNonSignalDriver((unsigned)Node_MemReadPort::Inputs::enable);

            // Figure out if the data output is registered (== synchronous).
            std::vector<BaseNode*> dataRegisterComponents;
            for (auto nh : readPort->exploreOutput((size_t)Node_MemReadPort::Outputs::data)) {
                // Any branches in the signal path would mean the unregistered output is also used, preventing register fusion.
                if (nh.isBranchingForward()) break;

                if (nh.isNodeType<Node_Register>()) {
                    auto dataReg = (Node_Register *) nh.node();
                    // The register needs to be enabled by the same signal as the read port.
                    if (dataReg->getNonSignalDriver(Node_Register::Input::ENABLE) != readPortEnable)
                        break;
                    // The register can't have a reset (since it's essentially memory).
                    if (dataReg->getNonSignalDriver(Node_Register::Input::RESET_VALUE).node != nullptr)
                        break;
                    dataRegisterComponents.push_back(nh.node());
                    rp.syncReadDataReg = dataReg;
                    break;
                } else if (nh.isSignal()) {
                    dataRegisterComponents.push_back(nh.node());
                } else break;
            }

            if (rp.syncReadDataReg != nullptr) {
                // Move the entire signal path and the data register into the memory group
                for (auto opt : dataRegisterComponents)
                    opt->moveToGroup(this);
                rp.dataOutput = {.node = rp.syncReadDataReg, .port = 0};

                dataRegisterComponents.clear();

                // Figure out if the optional output register is active.
                for (auto nh : rp.syncReadDataReg->exploreOutput(0)) {
                    // Any branches in the signal path would mean the unregistered output is also used, preventing register fusion.
                    if (nh.isBranchingForward()) break;

                    if (nh.isNodeType<Node_Register>()) {
                        auto dataReg = (Node_Register *) nh.node();
                        // Optional output register must be with the same clock as the sync memory read access.
                        // TODO: Actually, apparently this is not the case for intel?
                        if (dataReg->getClocks()[0] != rp.syncReadDataReg->getClocks()[0])
                            break;
                        dataRegisterComponents.push_back(nh.node());
                        rp.outputReg = dataReg;
                        break;
                    } else if (nh.isSignal()) {
                        dataRegisterComponents.push_back(nh.node());
                    } else break;
                }

                if (rp.outputReg) {
                    // Move the entire signal path and the optional output data register into the memory group
                    for (auto opt : dataRegisterComponents)
                        opt->moveToGroup(this);
                    rp.dataOutput = {.node = rp.outputReg, .port = 0};
                }
            }
        }
    }
}




void findMemoryGroups(Circuit &circuit)
{
    for (auto &node : circuit.getNodes())
        if (auto *memory = dynamic_cast<Node_Memory*>(node.get())) {
            auto *memoryGroup = memory->getGroup()->addSpecialChildNodeGroup<MemoryGroup>(memory);
            memoryGroup->setName("memory");
            memoryGroup->setComment("Auto generated");
        }
}

}