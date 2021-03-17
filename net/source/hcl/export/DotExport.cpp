#include "DotExport.h"

#include "../hlim/coreNodes/Node_Register.h"
#include "../hlim/coreNodes/Node_Multiplexer.h"
#include "../hlim/coreNodes/Node_Constant.h"
#include "../hlim/coreNodes/Node_Logic.h"
#include "../hlim/coreNodes/Node_Arithmetic.h"
#include "../hlim/coreNodes/Node_Compare.h"
#include "../hlim/coreNodes/Node_Pin.h"
#include "../hlim/supportNodes/Node_SignalTap.h"

#include <fstream>
#include <stdexcept>
#include <cstdlib>

// Adopted from Andy's codebase

namespace hcl::core {

DotExport::DotExport(std::filesystem::path destination) : m_destination(std::move(destination))
{
}

void DotExport::operator()(const hlim::Circuit &circuit)
{
    std::fstream file(m_destination.string().c_str(), std::fstream::out);
    if (!file.is_open())
        throw std::runtime_error("Could not open file!");
    file << "digraph G {" << std::endl;

    std::map<hlim::BaseNode*, unsigned> node2idx;
  //  std::map<hlim::NodeGroup*, unsigned> nodeGroup2idx;


    auto styleNode = [](std::fstream &file, hlim::BaseNode *node) {
        if (dynamic_cast<hlim::Node_Register*>(node))
            file << " shape=\"box\" style=\"filled\" fillcolor=\"#a0a0ff\"";
        else if (dynamic_cast<hlim::Node_Constant*>(node))
            file << " shape=\"ellipse\" style=\"filled\" fillcolor=\"#ffa0a0\"";
        else if (dynamic_cast<hlim::Node_Multiplexer*>(node))
            file << " shape=\"diamond\" style=\"filled\" fillcolor=\"#b0b0b0\"";
        else if (dynamic_cast<hlim::Node_Arithmetic*>(node))
            file << " shape=\"box\" style=\"filled\" fillcolor=\"#a0ffa0\"";
        else if (dynamic_cast<hlim::Node_Logic*>(node))
            file << " shape=\"box\" style=\"filled\" fillcolor=\"#ffffa0\"";
        else if (dynamic_cast<hlim::Node_Compare*>(node))
            file << " shape=\"box\" style=\"filled\" fillcolor=\"#ffd0a0\"";
        else if (dynamic_cast<hlim::Node_Pin*>(node))
            file << " shape=\"house\"";
        else if (dynamic_cast<hlim::Node_SignalTap*>(node))
            file << " shape=\"cds\"";
        else
            if (node->hasRef())
                file << " shape=\"box\" style=\"filled\" fillcolor=\"#eeeeee\"";
            else
                file << " shape=\"box\"";

    };

    {
        unsigned idx = 0;
        unsigned graphIdx = 0;

        std::function<void(const hlim::NodeGroup *nodeGroup)> reccurWalkNodeGroup;
        reccurWalkNodeGroup = [&](const hlim::NodeGroup *nodeGroup) {

            file << "subgraph cluster_" << graphIdx << "{" << std::endl;
          //  nodeGroup2idx[nodeGroup] = graphIdx++;
            graphIdx++;

            file << " label=\"" << nodeGroup->getInstanceName() << "\";" << std::endl;
            switch (nodeGroup->getGroupType()) {
                case hlim::NodeGroup::GroupType::ENTITY:
                    file << " color=blue;" << std::endl;
                break;
                case hlim::NodeGroup::GroupType::AREA:
                    file << " color=black; style=filled; fillcolor=azure; " << std::endl;
                break;
                case hlim::NodeGroup::GroupType::SFU:
                    file << " color=black; style=filled; fillcolor=beige;" << std::endl;
                break;
            }

            for (const auto &subGroup : nodeGroup->getChildren())
                reccurWalkNodeGroup(subGroup.get());

            for (auto *node : nodeGroup->getNodes()) {
                file << "node_" << idx << "[label=\"";
                if (node->getName().length() < 30)
                    file << node->getName();
                else
                    file << "[zip]";
                file << " - " << node->getId() << " - " << node->getTypeName() << "\"";
                styleNode(file, node);
                file << "];" << std::endl;
                node2idx[node] = idx;
                idx++;
            }

            file << "}" << std::endl;
        };
        reccurWalkNodeGroup(circuit.getRootNodeGroup());

        for (auto &node : circuit.getNodes()) {
            if (node->getGroup() == nullptr) {
                file << "node_" << idx << "[label=\"" << node->getName() << " - " << node->getId() << " - " << node->getTypeName() << "\"";
                styleNode(file, node.get());
                file << "];" << std::endl;
                node2idx[node.get()] = idx;
                idx++;
            }
        }
    }

    for (auto &node : circuit.getNodes()) {
        unsigned nodeId = node2idx.find(node.get())->second;

        for (auto port : utils::Range(node->getNumInputPorts())) {
            auto producer = node->getDriver(port);
            if (producer.node == nullptr) continue;

            unsigned producerId = node2idx.find(producer.node)->second;

            auto type = hlim::getOutputConnectionType(producer);

            file << "node_" << producerId << " -> node_" << nodeId << " [";

            file << " label=\"";
            switch (type.interpretation) {
                case hlim::ConnectionType::BOOL:
                    file << "BOOL"; break;
                case hlim::ConnectionType::BITVEC:
                    file << "BVEC(" << type.width << ')'; break;
                case hlim::ConnectionType::DEPENDENCY:
                    file << "DEPENDENCY"; break;
            }
            file << "\"";

            switch (producer.node->getOutputType(producer.port)) {
                case hlim::NodeIO::OUTPUT_LATCHED:
                    file << " style=\"dashed\"";
                break;
                case hlim::NodeIO::OUTPUT_CONSTANT:
                    file << " color=\"blue\"";
                case hlim::NodeIO::OUTPUT_IMMEDIATE:
                default:
                break;
            }


            std::int64_t creationDistance = (std::int64_t)node->getId() - (std::int64_t)producer.node->getId();
            double weight;
            if (creationDistance > 0)
                weight = 100 / std::log(1+std::abs(creationDistance));
            else {
                //file << " constraint=false";
                weight = 1 / std::log(1+std::abs(creationDistance));
            }

            if (producer.node->getGroup() != node->getGroup())
                weight *= 0.01;

            file << " weight="<<std::round(1+weight * 100); // dot wants integers, so scale everything up

            file << "];" << std::endl;
        }
    }

    file << "}" << std::endl;
}

void DotExport::runGraphViz(std::filesystem::path destination)
{
    system((std::string("dot -Tsvg ") + m_destination.string() + " -o " + destination.string()).c_str());
}




void visualize(const hlim::Circuit &circuit, const std::string &filename)
{
    DotExport exp(filename+".dot");
    exp(circuit);
    exp.runGraphViz(filename+".svg");
}

}
