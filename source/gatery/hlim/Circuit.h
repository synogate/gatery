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
#include "SignalGroup.h"
#include "Node.h"
#include "ConnectionType.h"
#include "Clock.h"

#include <vector>
#include <memory>
#include <map>

namespace gtry::hlim {

/*
class Circuit;
class Report;
class PostProcessor;

class PostProcessingStep {
    public:
        virtual ~PostProcessingStep() = default;
        virtual void perform(PostProcessor &postProcessor, Circuit &circuit, Report &report);
    protected:
};

class PostProcessor {
    public:
        enum Phase {
            OPTIMIZATION,
            COMPLEX_OPERATION_DECOMPOSITION,
            SIGNAL_NAMING,
            PLATFORM_ADAPTATION,
            NUM_PHASES
        };

        PostProcessor &setReport(Report &report) { m_report = &report; return *this; }

        void addPass(Phase phase, std::unique_ptr<PostProcessingStep> pass) { m_passes[phase].push_back(std::move(pass)); }
    protected:
        Report *m_report = nullptr;
        std::array<std::vector<std::unique_ptr<PostProcessingStep>>, NUM_PHASES> m_passes;
};
*/


class PostProcessor {
    // For things to come
};

class DefaultPostprocessing : public PostProcessor
{
    public:
        DefaultPostprocessing() {

        }
};

class Circuit
{
    public:
        Circuit();

        void copySubnet(const std::vector<NodePort> &subnetInputs,
                        const std::vector<NodePort> &subnetOutputs,
                        std::map<BaseNode*, BaseNode*> &mapSrc2Dst);

        template<typename NodeType, typename... Args>
        NodeType *createNode(Args&&... args);

        BaseNode *createUnconnectedClone(BaseNode *srcNode, bool noId = false);

        template<typename... Args>
        SignalGroup *createSignalGroup(Args&&... args);

        template<typename ClockType, typename... Args>
        ClockType *createClock(Args&&... args);

        Clock *createUnconnectedClock(Clock *clock, Clock *newParent);

        inline NodeGroup *getRootNodeGroup() { return m_root.get(); }
        inline const NodeGroup *getRootNodeGroup() const { return m_root.get(); }

        inline std::vector<std::unique_ptr<BaseNode>> &getNodes() { return m_nodes; }
        inline const std::vector<std::unique_ptr<BaseNode>> &getNodes() const { return m_nodes; }
        inline const std::vector<std::unique_ptr<Clock>> &getClocks() const { return m_clocks; }

        void inferSignalNames();

        void cullSequentiallyDuplicatedSignalNodes();
        void cullUnnamedSignalNodes();
        void cullOrphanedSignalNodes();
        void cullUnusedNodes();
        void mergeMuxes();
        void cullMuxConditionNegations();
        void removeIrrelevantMuxes();
        void removeNoOps();
        void foldRegisterMuxEnableLoops();
        void propagateConstants();
        void removeConstSelectMuxes();

        void removeFalseLoops();

        void ensureSignalNodePlacement();

        void postprocess(const PostProcessor &postProcessor);

        Node_Signal *appendSignal(NodePort &nodePort);
        Node_Signal *appendSignal(RefCtdNodePort &nodePort);

        Node_Attributes *getCreateAttribNode(NodePort &nodePort);
    protected:
        std::vector<std::unique_ptr<BaseNode>> m_nodes;
        std::unique_ptr<NodeGroup> m_root;
        std::vector<std::unique_ptr<SignalGroup>> m_signalGroups;
        std::vector<std::unique_ptr<Clock>> m_clocks;

        std::uint64_t m_nextNodeId = 0;
};


template<typename NodeType, typename... Args>
NodeType *Circuit::createNode(Args&&... args) {
    m_nodes.push_back(std::make_unique<NodeType>(std::forward<Args>(args)...));
    m_nodes.back()->setId(m_nextNodeId++, {});
    return (NodeType *) m_nodes.back().get();
}

template<typename ClockType, typename... Args>
ClockType *Circuit::createClock(Args&&... args) {
    m_clocks.push_back(std::make_unique<ClockType>(std::forward<Args>(args)...));
    return (ClockType *) m_clocks.back().get();
}

template<typename... Args>
SignalGroup *Circuit::createSignalGroup(Args&&... args) {
    m_signalGroups.push_back(std::make_unique<SignalGroup>(std::forward<Args>(args)...));
    return m_signalGroups.back().get();
}


}
