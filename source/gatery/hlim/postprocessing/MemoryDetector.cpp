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
#include "gatery/pch.h"
#include "MemoryDetector.h"

#include "../Circuit.h"

#include "../coreNodes/Node_Signal.h"
#include "../coreNodes/Node_Register.h"
#include "../coreNodes/Node_Constant.h"
#include "../coreNodes/Node_Compare.h"
#include "../coreNodes/Node_Logic.h"
#include "../coreNodes/Node_Multiplexer.h"
#include "../supportNodes/Node_Memory.h"
#include "../supportNodes/Node_MemPort.h"
#include "../GraphExploration.h"
#include "../RegisterRetiming.h"
#include "../GraphTools.h"


#define DEBUG_OUTPUT

#ifdef DEBUG_OUTPUT
#include "../Subnet.h"
#include "../../export/DotExport.h"
#endif

#include <sstream>
#include <vector>
#include <set>
#include <optional>

namespace gtry::hlim {


MemoryGroup::MemoryGroup() : NodeGroup(GroupType::SFU)
{
    m_name = "memory";
}

void MemoryGroup::formAround(Node_Memory *memory, Circuit &circuit)
{
    m_memory = memory;
    m_memory->moveToGroup(this);

    // Initial naive grabbing of everything that might be usefull
    for (auto &np : m_memory->getDirectlyDriven(0)) {
        auto *port = dynamic_cast<Node_MemPort*>(np.node);
        HCL_ASSERT(port->isWritePort() || port->isReadPort());
        // Check all write ports
        if (port->isWritePort()) {
            HCL_ASSERT_HINT(!port->isReadPort(), "For now I don't want to mix read and write ports");
            m_writePorts.push_back({.node=NodePtr<Node_MemPort>{port}});
            port->moveToGroup(this);
        }
        // Check all read ports
        if (port->isReadPort()) {
            m_readPorts.push_back({.node = NodePtr<Node_MemPort>{port}});
            ReadPort &rp = m_readPorts.back();
            port->moveToGroup(this);
            rp.dataOutput = {.node = port, .port = (size_t)Node_MemPort::Outputs::rdData};

            NodePort readPortEnable = port->getNonSignalDriver((unsigned)Node_MemPort::Inputs::enable);

            // Figure out if the data output is registered (== synchronous).
            std::vector<BaseNode*> dataRegisterComponents;
            for (auto nh : port->exploreOutput((size_t)Node_MemPort::Outputs::rdData)) {
                // Any branches in the signal path would mean the unregistered output is also used, preventing register fusion.
                if (nh.isBranchingForward()) break;

                if (nh.isNodeType<Node_Register>()) {
                    auto dataReg = (Node_Register *) nh.node();
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
                rp.syncReadDataReg->getFlags().clear(Node_Register::ALLOW_RETIMING_BACKWARD).clear(Node_Register::ALLOW_RETIMING_FORWARD).insert(Node_Register::IS_BOUND_TO_MEMORY);
                // Move the entire signal path and the data register into the memory group
                for (auto opt : dataRegisterComponents)
                    opt->moveToGroup(this);
                rp.dataOutput = {.node = rp.syncReadDataReg, .port = 0};

                dataRegisterComponents.clear();
#if 0
// todo: Not desired and also not correctly working with RMW hazards
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
                    rp.outputReg->getFlags().clear(Node_Register::ALLOW_RETIMING_BACKWARD).clear(Node_Register::ALLOW_RETIMING_FORWARD).insert(Node_Register::IS_BOUND_TO_MEMORY);
                    // Move the entire signal path and the optional output data register into the memory group
                    for (auto opt : dataRegisterComponents)
                        opt->moveToGroup(this);
                    rp.dataOutput = {.node = rp.outputReg, .port = 0};
                }
#endif
            }
        }
    }

    // Verify writing is only happening with one clock:
    {
        Node_MemPort *firstWritePort = nullptr;
        for (auto &np : m_memory->getDirectlyDriven(0)) {
            auto *port = dynamic_cast<Node_MemPort*>(np.node);
            if (port->isWritePort()) {
                if (firstWritePort == nullptr)
                    firstWritePort = port;
                else {
                    std::stringstream issues;
                    issues << "All write ports to a memory must have the same clock!\n";
                    issues << "from:\n" << firstWritePort->getStackTrace() << "\n and from:\n" << port->getStackTrace();
                    HCL_DESIGNCHECK_HINT(firstWritePort->getClocks()[0] == port->getClocks()[0], issues.str());
                }
            }
        }
    }

    if (m_memory->type() == Node_Memory::MemType::DONT_CARE) {
        if (!m_memory->getDirectlyDriven(0).empty()) {
            size_t numWords = m_memory->getSize() / m_memory->getMaxPortWidth();
            if (numWords > 64)
                m_memory->setType(Node_Memory::MemType::BRAM);
            // todo: Also depend on other things
        }
    }

}

void MemoryGroup::lazyCreateFixupNodeGroup()
{
    if (m_fixupNodeGroup == nullptr) {
        m_fixupNodeGroup = m_parent->addChildNodeGroup(GroupType::ENTITY);
        m_fixupNodeGroup->recordStackTrace();
        m_fixupNodeGroup->setName("Memory_Helper");
        m_fixupNodeGroup->setComment("Auto generated to handle various memory access issues such as read during write and read modify write hazards.");
        moveInto(m_fixupNodeGroup);
    }
}



void MemoryGroup::convertToReadBeforeWrite(Circuit &circuit)
{
    // If an async read happens after a write, it must
    // check if an address collision occured and if so directly forward the new value.
    for (auto &rp : m_readPorts) {
        // Collect a list of all potentially conflicting write ports and sort them in write order, so that conflict resolution can also happen in write order
        std::vector<WritePort*> sortedWritePorts;
        for (auto &wp : m_writePorts)
            if (wp.node->isOrderedBefore(rp.node))
                sortedWritePorts.push_back(&wp);

        // sort last to first because multiplexers are prepended.
        // todo: this assumes that write ports do have an order.
        std::sort(sortedWritePorts.begin(), sortedWritePorts.end(), [](WritePort *left, WritePort *right)->bool{
            return left->node->isOrderedAfter(right->node);
        });

        for (auto wp_ptr : sortedWritePorts) {
            auto &wp = *wp_ptr;

            lazyCreateFixupNodeGroup();

            auto *addrCompNode = circuit.createNode<Node_Compare>(Node_Compare::EQ);
            addrCompNode->recordStackTrace();
            addrCompNode->moveToGroup(m_fixupNodeGroup);
            addrCompNode->setComment("Compare read and write addr for conflicts");
            addrCompNode->connectInput(0, rp.node->getDriver((unsigned)Node_MemPort::Inputs::address));
            addrCompNode->connectInput(1, wp.node->getDriver((unsigned)Node_MemPort::Inputs::address));

            NodePort conflict = {.node = addrCompNode, .port = 0ull};
            circuit.appendSignal(conflict)->setName("conflict");

            if (rp.node->getDriver((unsigned)Node_MemPort::Inputs::enable).node != nullptr) {
                auto *logicAnd = circuit.createNode<Node_Logic>(Node_Logic::AND);
                logicAnd->moveToGroup(m_fixupNodeGroup);
                logicAnd->recordStackTrace();
                logicAnd->connectInput(0, conflict);
                logicAnd->connectInput(1, rp.node->getDriver((unsigned)Node_MemPort::Inputs::enable));
                conflict = {.node = logicAnd, .port = 0ull};
                circuit.appendSignal(conflict)->setName("conflict_and_rdEn");
            }

            HCL_ASSERT(wp.node->getNonSignalDriver((unsigned)Node_MemPort::Inputs::enable) == wp.node->getNonSignalDriver((unsigned)Node_MemPort::Inputs::wrEnable));
            if (wp.node->getDriver((unsigned)Node_MemPort::Inputs::enable).node != nullptr) {
                auto *logicAnd = circuit.createNode<Node_Logic>(Node_Logic::AND);
                logicAnd->moveToGroup(m_fixupNodeGroup);
                logicAnd->recordStackTrace();
                logicAnd->connectInput(0, conflict);
                logicAnd->connectInput(1, wp.node->getDriver((unsigned)Node_MemPort::Inputs::enable));
                conflict = {.node = logicAnd, .port = 0ull};
                circuit.appendSignal(conflict)->setName("conflict_and_wrEn");
            }

            NodePort wrData = wp.node->getDriver((unsigned)Node_MemPort::Inputs::wrData);

            auto delayLike = [&](Node_Register *refReg, NodePort &np, const char *name, const char *comment) {
                auto *reg = circuit.createNode<Node_Register>();
                reg->recordStackTrace();
                reg->moveToGroup(m_fixupNodeGroup);
                reg->setComment(comment);
                reg->setClock(refReg->getClocks()[0]);
                for (auto i : {Node_Register::Input::ENABLE, Node_Register::Input::RESET_VALUE})
                    reg->connectInput(i, refReg->getDriver(i));
                reg->connectInput(Node_Register::Input::DATA, np);
                np = {.node = reg, .port = 0ull};
                circuit.appendSignal(np)->setName(name);
            };

            if (rp.syncReadDataReg != nullptr) {
                // read data gets delayed so we will have to delay the write data and conflict decision as well
                delayLike(rp.syncReadDataReg, wrData, "delayedWrData", "The memory read gets delayed by a register so the write data bypass also needs to be delayed.");
                delayLike(rp.syncReadDataReg, conflict, "delayedConflict", "The memory read gets delayed by a register so the collision detection decision also needs to be delayed.");

                if (rp.outputReg != nullptr) {
                    // need to delay even more
                    delayLike(rp.syncReadDataReg, wrData, "delayed_2_WrData", "The memory read gets delayed by an additional register so the write data bypass also needs to be delayed.");
                    delayLike(rp.syncReadDataReg, conflict, "delayed_2_Conflict", "The memory read gets delayed by an additional register so the collision detection decision also needs to be delayed.");
                }
            }

            std::vector<NodePort> consumers = rp.dataOutput.node->getDirectlyDriven(rp.dataOutput.port);

            // Finally the actual mux to arbitrate between the actual read and the forwarded write data.
            auto *muxNode = circuit.createNode<Node_Multiplexer>(2);

            // Then bind the mux
            muxNode->recordStackTrace();
            muxNode->moveToGroup(m_fixupNodeGroup);
            muxNode->setComment("If read and write addr match and read and write are enabled, forward write data to read output.");
            muxNode->connectSelector(conflict);
            muxNode->connectInput(0, rp.dataOutput);
            muxNode->connectInput(1, wrData);

            NodePort muxOut = {.node = muxNode, .port=0ull};

            circuit.appendSignal(muxOut)->setName("conflict_bypass_mux");

            // Rewire all original consumers to the mux output
            for (auto np : consumers)
                np.node->rewireInput(np.port, muxOut);
        }
    }


    std::vector<Node_MemPort*> sortedWritePorts;
    for (auto &wp : m_writePorts)
        sortedWritePorts.push_back(wp.node);

    std::sort(sortedWritePorts.begin(), sortedWritePorts.end(), [](Node_MemPort *left, Node_MemPort *right)->bool{
        return left->isOrderedBefore(right);
    });


    // Reorder all writes to happen after all reads
    Node_MemPort *lastPort = nullptr;
    for (auto &rp : m_readPorts) {
        rp.node->orderAfter(lastPort);
        lastPort = rp.node;
    }
    // But preserve write order for now
    if (!sortedWritePorts.empty())
        sortedWritePorts[0]->orderAfter(lastPort);
    for (size_t i = 1; i < sortedWritePorts.size(); i++)
        sortedWritePorts[i]->orderAfter(sortedWritePorts[i-1]);
}


void MemoryGroup::resolveWriteOrder(Circuit &circuit)
{
    // If two write ports have an explicit ordering, then the later write always trumps the former if both happen to the same address.
    // Search for such cases and build explicit logic that disables the earlier write.
    for (auto &wp1 : m_writePorts)
        for (auto &wp2 : m_writePorts) {
            if (&wp1 == &wp2) continue;

            if (wp1.node->isOrderedBefore(wp2.node)) {
                // Potential addr conflict, build hazard logic

                lazyCreateFixupNodeGroup();


                auto *addrCompNode = circuit.createNode<Node_Compare>(Node_Compare::NEQ);
                addrCompNode->recordStackTrace();
                addrCompNode->moveToGroup(m_fixupNodeGroup);
                addrCompNode->setComment("We can enable the former write if the write adresses differ.");
                addrCompNode->connectInput(0, wp1.node->getDriver((unsigned)Node_MemPort::Inputs::address));
                addrCompNode->connectInput(1, wp2.node->getDriver((unsigned)Node_MemPort::Inputs::address));

                // Enable write if addresses differ
                NodePort newWrEn1 = {.node = addrCompNode, .port = 0ull};
                circuit.appendSignal(newWrEn1)->setName("newWrEn");

                // Alternatively, enable write if wp2 does not write (no connection on enable means yes)
                HCL_ASSERT(wp2.node->getNonSignalDriver((unsigned)Node_MemPort::Inputs::enable) == wp2.node->getNonSignalDriver((unsigned)Node_MemPort::Inputs::wrEnable));
                if (wp2.node->getDriver((unsigned)Node_MemPort::Inputs::enable).node != nullptr) {

                    auto *logicNot = circuit.createNode<Node_Logic>(Node_Logic::NOT);
                    logicNot->moveToGroup(m_fixupNodeGroup);
                    logicNot->recordStackTrace();
                    logicNot->connectInput(0, wp2.node->getDriver((unsigned)Node_MemPort::Inputs::enable));

                    auto *logicOr = circuit.createNode<Node_Logic>(Node_Logic::OR);
                    logicOr->moveToGroup(m_fixupNodeGroup);
                    logicOr->setComment("We can also enable the former write if the latter write is disabled.");
                    logicOr->recordStackTrace();
                    logicOr->connectInput(0, newWrEn1);
                    logicOr->connectInput(1, {.node = logicNot, .port = 0ull});
                    newWrEn1 = {.node = logicOr, .port = 0ull};
                    circuit.appendSignal(newWrEn1)->setName("newWrEn");
                }

                // But only enable write if wp1 actually wants to write (no connection on enable means yes)
                HCL_ASSERT(wp1.node->getNonSignalDriver((unsigned)Node_MemPort::Inputs::enable) == wp1.node->getNonSignalDriver((unsigned)Node_MemPort::Inputs::wrEnable));
                if (wp1.node->getDriver((unsigned)Node_MemPort::Inputs::enable).node != nullptr) {
                    auto *logicAnd = circuit.createNode<Node_Logic>(Node_Logic::AND);
                    logicAnd->moveToGroup(m_fixupNodeGroup);
                    logicAnd->setComment("But we can only enable the former write if the former write actually wants to write.");
                    logicAnd->recordStackTrace();
                    logicAnd->connectInput(0, newWrEn1);
                    logicAnd->connectInput(1, wp1.node->getDriver((unsigned)Node_MemPort::Inputs::enable));
                    newWrEn1 = {.node = logicAnd, .port = 0ull};
                    circuit.appendSignal(newWrEn1)->setName("newWrEn");
                }


                wp1.node->rewireInput((unsigned)Node_MemPort::Inputs::enable, newWrEn1);
                wp1.node->rewireInput((unsigned)Node_MemPort::Inputs::wrEnable, newWrEn1);
            }
        }


    // Reorder all writes to happen after all reads
    Node_MemPort *lastPort = nullptr;
    for (auto &rp : m_readPorts) {
        rp.node->orderAfter(lastPort);
        lastPort = rp.node;
    }
    // Writes can happen in any order now, but after the last read
    for (auto &wp : m_writePorts)
        wp.node->orderAfter(lastPort);
}



void MemoryGroup::ensureNotEnabledFirstCycle(Circuit &circuit, NodeGroup *ng, Node_MemPort *writePort)
{
    // Ensure enable is low in first cycle
    auto enableDriver = writePort->getNonSignalDriver((size_t)Node_MemPort::Inputs::enable);
    auto wrEnableDriver = writePort->getNonSignalDriver((size_t)Node_MemPort::Inputs::wrEnable);

    HCL_ASSERT(enableDriver == wrEnableDriver);

    // Check if already driven by register
    if (auto *enableReg = dynamic_cast<Node_Register*>(enableDriver.node)) {

        // If that register is already resetting to zero everything is fine
		auto resetDriver = enableReg->getNonSignalDriver(Node_Register::RESET_VALUE);
		if (resetDriver.node != nullptr) {
            auto resetValue = evaluateStatically(circuit, resetDriver);
            HCL_ASSERT(resetValue.size() == 1);
            if (resetValue.get(sim::DefaultConfig::DEFINED, 0) && !resetValue.get(sim::DefaultConfig::VALUE, 0))
                return;
        }

        // Check if that register is only driving the memory port so that the reset can be changed
        bool onlyUser = true;
        for (auto nh : enableReg->exploreOutput(0)) {
            if (nh.isSignal()) continue;
            if (nh.node() == writePort && (nh.port() == (size_t)Node_MemPort::Inputs::enable || nh.port() == (size_t)Node_MemPort::Inputs::wrEnable)) {
                nh.backtrack();
                continue;
            }
            onlyUser = false;
            break;
        }

        if (onlyUser) {
            sim::DefaultBitVectorState state;
            state.resize(1);
            state.set(sim::DefaultConfig::DEFINED, 0);
            state.set(sim::DefaultConfig::VALUE, 0, false);
            auto *constZero = circuit.createNode<Node_Constant>(state, ConnectionType::BOOL);
            constZero->recordStackTrace();
            constZero->moveToGroup(ng);
            enableReg->connectInput(Node_Register::RESET_VALUE, {.node = constZero, .port = 0ull});

            return;
        }

    }

    // Build single register with reset 0 and input 1 

    sim::DefaultBitVectorState state;
    state.resize(1);
    state.set(sim::DefaultConfig::DEFINED, 0);
    state.set(sim::DefaultConfig::VALUE, 0, false);
    auto *constZero = circuit.createNode<Node_Constant>(state, ConnectionType::BOOL);
    constZero->recordStackTrace();
    constZero->moveToGroup(ng);

    state.set(sim::DefaultConfig::VALUE, 0, true);
    auto *constOne = circuit.createNode<Node_Constant>(state, ConnectionType::BOOL);
    constOne->recordStackTrace();
    constOne->moveToGroup(ng);

    auto *reg = circuit.createNode<Node_Register>();
    reg->recordStackTrace();
    reg->moveToGroup(ng);
    reg->setComment("Register that generates a zero after reset and a one on all later cycles");
    reg->setClock(writePort->getClocks()[0]);

    reg->connectInput(Node_Register::Input::RESET_VALUE, {.node = constZero, .port = 0ull});
    reg->connectInput(Node_Register::Input::DATA, {.node = constOne, .port = 0ull});
    reg->getFlags().insert(Node_Register::ALLOW_RETIMING_BACKWARD).insert(Node_Register::ALLOW_RETIMING_FORWARD);

    NodePort newEnable = {.node = reg, .port = 0ull};

    if (wrEnableDriver.node != nullptr) {

        // And register to enable input
        auto *logicAnd = circuit.createNode<Node_Logic>(Node_Logic::AND);
        logicAnd->moveToGroup(ng);
        logicAnd->recordStackTrace();
        logicAnd->setComment("Force the enable port to zero on first cycle after reset since the retiming delayed the entire write port by one cycle!");
        logicAnd->connectInput(0, newEnable);
        logicAnd->connectInput(1, wrEnableDriver);

        newEnable = {.node = logicAnd, .port = 0ull};
    }

    writePort->connectEnable(newEnable);
    writePort->connectWrEnable(newEnable);
}


void MemoryGroup::attemptRegisterRetiming(Circuit &circuit)
{
    if (m_memory->type() != Node_Memory::MemType::BRAM) return;

/*
        {
            auto all = ConstSubnet::all(circuit);
            DotExport exp("beforeRetiming.dot");
            exp(circuit, all);
            exp.runGraphViz("beforeRetiming.svg");            
        }
*/
    std::set<Node_MemPort*> retimeableWritePorts;
    for (auto np : m_memory->getDirectlyDriven(0)) {
        auto *memPort = dynamic_cast<Node_MemPort*>(np.node);
        if (memPort->isWritePort()) {
            HCL_ASSERT_HINT(!memPort->isReadPort(), "Retiming for combined read and write ports not yet implemented!");
            retimeableWritePorts.insert(memPort);
        }
    }



    std::set<Node_MemPort*> actuallyRetimedWritePorts;

    // If we are aiming for blockrams:
    // Check if any read ports are lacking the register that make them synchronous.
    // If they do, scan the read data output bus for any registers buried in the combinatorics that could be pulled back and fused.
    // Keep note of which write ports are "delayed" through this retiming to then, in a second step, build 
    // While doing that, also check for and build read modify write mechanics w. hazard detection in case those combinatorics feed back into
    // a write port of the same memory with same addr and enables.

    for (auto &rp : m_readPorts) {
        // It's fine if it is already synchronous.
        if (rp.syncReadDataReg != nullptr) continue;
        HCL_ASSERT(rp.outputReg == nullptr);

        auto subnet = Subnet::all(circuit);
        Subnet retimedArea;
        // On multi-readport memories there can already appear a register due to the retiming of other read ports. In this case, retimeBackwardtoOutput is a no-op.
        retimeBackwardtoOutput(circuit, subnet, {}, retimeableWritePorts, retimedArea, rp.dataOutput, true, true);
/*
        {
            auto all = ConstSubnet::all(circuit);
            DotExport exp("afterRetiming.dot");
            exp(circuit, all);
            exp.runGraphViz("afterRetiming.svg");            
        }
*/
        // Find register
        HCL_ASSERT(rp.node->getDirectlyDriven((size_t)Node_MemPort::Outputs::rdData).size() == 1);
        rp.syncReadDataReg = dynamic_cast<Node_Register*>(rp.node->getDirectlyDriven((size_t)Node_MemPort::Outputs::rdData)[0].node);
        HCL_ASSERT(rp.syncReadDataReg != nullptr);
        rp.syncReadDataReg->getFlags().clear(Node_Register::ALLOW_RETIMING_BACKWARD).clear(Node_Register::ALLOW_RETIMING_FORWARD).insert(Node_Register::IS_BOUND_TO_MEMORY);
        // find output of register
        rp.dataOutput = {.node = rp.syncReadDataReg, .port = 0ull};


        for (auto wp : retimeableWritePorts) {
            if (retimedArea.contains(wp)) {
                // Handle write port
                lazyCreateFixupNodeGroup();
                ensureNotEnabledFirstCycle(circuit, m_fixupNodeGroup, wp);

                // take note that this write port is delayed.
                actuallyRetimedWritePorts.insert(wp);
            }
        }
    }

    if (actuallyRetimedWritePorts.empty()) return;

    std::vector<Node_MemPort*> sortedWritePorts;
    for (auto wp : actuallyRetimedWritePorts)
        sortedWritePorts.push_back(wp);

    std::sort(sortedWritePorts.begin(), sortedWritePorts.end(), [](Node_MemPort *left, Node_MemPort *right)->bool{
        return left->isOrderedBefore(right);
    });

    if (sortedWritePorts.size() >= 2)
        HCL_ASSERT(sortedWritePorts[0]->isOrderedBefore(sortedWritePorts[1]));

    auto *clock = sortedWritePorts.front()->getClocks()[0];
    ReadModifyWriteHazardLogicBuilder rmwBuilder(circuit, clock);
    rmwBuilder.setReadLatency(1);
    //rmwBuilder.retimeRegisterToMux();
    
    for (auto &rp : m_readPorts)
        rmwBuilder.addReadPort(ReadModifyWriteHazardLogicBuilder::ReadPort{
            .addrInputDriver = rp.node->getDriver((size_t)Node_MemPort::Inputs::address),
            .enableInputDriver = rp.node->getDriver((size_t)Node_MemPort::Inputs::enable),
            .dataOutOutputDriver = (NodePort) rp.dataOutput,
        });

    for (auto wp : sortedWritePorts) {
        HCL_ASSERT(wp->getDriver((size_t)Node_MemPort::Inputs::enable) == wp->getDriver((size_t)Node_MemPort::Inputs::wrEnable));
        rmwBuilder.addWritePort(ReadModifyWriteHazardLogicBuilder::WritePort{
            .addrInputDriver = wp->getDriver((size_t)Node_MemPort::Inputs::address),
            .enableInputDriver = wp->getDriver((size_t)Node_MemPort::Inputs::enable),
            .enableMaskInputDriver = {},
            .dataInInputDriver = wp->getDriver((size_t)Node_MemPort::Inputs::wrData),
        });
    }

    rmwBuilder.build();

    const auto &newNodes = rmwBuilder.getNewNodes();
    for (auto n : newNodes) 
        n->moveToGroup(m_fixupNodeGroup);

/*
    {
        auto all = ConstSubnet::all(circuit);
        DotExport exp("afterRMW.dot");
        exp(circuit, all);
        exp.runGraphViz("afterRMW.svg");            
    }        
*/    
}

void MemoryGroup::verify()
{
    switch (m_memory->type()) {
        case Node_Memory::MemType::BRAM:
            for (auto &rp : m_readPorts) {
                if (rp.syncReadDataReg == nullptr) {
                    std::stringstream issue;
                    issue << "Memory can not become BRAM because a read port is missing it's data register.\nMemory from:\n"
                        << m_memory->getStackTrace() << "\nRead port from:\n" << rp.node->getStackTrace();
                    HCL_DESIGNCHECK_HINT(false, issue.str());
                }
            }
            /*
            if (m_readPorts.size() + m_writePorts.size() > 2) {
                std::stringstream issue;
                issue << "Memory can not become BRAM because it has too many memory ports.\nMemory from:\n"
                      << m_memory->getStackTrace();
                HCL_DESIGNCHECK_HINT(false, issue.str());
            }
            */
        break;
        case Node_Memory::MemType::LUTRAM:
            if (m_readPorts.size() > 1) {
                std::stringstream issue;
                issue << "Memory can not become LUTRAM because it has too many read ports.\nMemory from:\n"
                      << m_memory->getStackTrace();
                HCL_DESIGNCHECK_HINT(false, issue.str());
            }
            if (m_writePorts.size() > 1) {
                std::stringstream issue;
                issue << "Memory can not become LUTRAM because it has too many write ports.\nMemory from:\n"
                      << m_memory->getStackTrace();
                HCL_DESIGNCHECK_HINT(false, issue.str());
            }
        break;
    }
}



void findMemoryGroups(Circuit &circuit)
{
    for (auto &node : circuit.getNodes())
        if (auto *memory = dynamic_cast<Node_Memory*>(node.get())) {
            auto *memoryGroup = memory->getGroup()->addSpecialChildNodeGroup<MemoryGroup>();
            if (memory->getName().empty())
                memoryGroup->setName("memory");
            else
                memoryGroup->setName(memory->getName());
            memoryGroup->setComment("Auto generated");
            memoryGroup->formAround(memory, circuit);
        }
}

void buildExplicitMemoryCircuitry(Circuit &circuit)
{
    for (auto i : utils::Range(circuit.getNodes().size())) {
        auto& node = circuit.getNodes()[i];
        if (auto* memory = dynamic_cast<Node_Memory*>(node.get())) {
            auto* memoryGroup = dynamic_cast<MemoryGroup*>(memory->getGroup());
            memoryGroup->convertToReadBeforeWrite(circuit);
            memoryGroup->attemptRegisterRetiming(circuit);
            memoryGroup->resolveWriteOrder(circuit);
            memoryGroup->verify();
        }
    }
}


}
