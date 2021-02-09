#include "DotExport.h"

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

    {
        unsigned idx = 0;
        unsigned graphIdx = 0;

        std::function<void(const hlim::NodeGroup *nodeGroup)> reccurWalkNodeGroup;
        reccurWalkNodeGroup = [&](const hlim::NodeGroup *nodeGroup) {

            file << "subgraph cluster_" << graphIdx << "{" << std::endl;
          //  nodeGroup2idx[nodeGroup] = graphIdx++;
            graphIdx++;
            
            file << " label=\"" << nodeGroup->getName() << "\";" << std::endl;
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
                file << "node_" << idx << "[label=\"" << node->getName() << " - " << node->getTypeName() << "\" shape=\"polygon\"];" << std::endl;
                node2idx[node] = idx;
                idx++;
            }

            file << "}" << std::endl;
        };
        reccurWalkNodeGroup(circuit.getRootNodeGroup());

        for (auto &node : circuit.getNodes()) {
            if (node->getGroup() == nullptr) {
                file << "node_" << idx << "[label=\"" << node->getName() << " - " << node->getTypeName() << "\" shape=\"polygon\"];" << std::endl;
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

            auto type = producer.node->getOutputConnectionType(producer.port);

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


            ///@todo weight based on how closely together they were created
            //file << " weight="<<node->getInput(port).displayWeight;

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
