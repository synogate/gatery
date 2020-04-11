#include "VHDLExport.h"

#include "../../utils/Range.h"
#include "../../utils/Enumerate.h"
#include "../../utils/Exceptions.h"

#include "../../hlim//coreNodes/Node_Arithmetic.h"
#include "../../hlim//coreNodes/Node_Compare.h"
#include "../../hlim//coreNodes/Node_Logic.h"
#include "../../hlim//coreNodes/Node_Multiplexer.h"
#include "../../hlim//coreNodes/Node_Signal.h"

#include <fstream>
#include <iostream>
#include <iomanip>

namespace mhdl {
namespace core {
namespace vhdl {

VHDLExport::VHDLExport(std::filesystem::path destination)
{
    m_destination = std::move(destination);
    m_codeFormatting.reset(new DefaultCodeFormatting());
}


VHDLExport &VHDLExport::setFormatting(CodeFormatting *codeFormatting)
{
    m_codeFormatting.reset(codeFormatting);
    return *this;
}

void VHDLExport::operator()(const hlim::Circuit &circuit)
{
    exportGroup(circuit.getRootNodeGroup());
}

void VHDLExport::exportGroup(const hlim::NodeGroup *group)
{
    
    for (const auto &child : group->getChildren())
        exportGroup(child.get());
    

    std::filesystem::path filePath = m_destination / m_codeFormatting->getFilename(group);
    std::filesystem::create_directories(filePath.parent_path());

    std::cout << "Exporting " << group->getName() << " to " << filePath << std::endl;

    std::fstream file(filePath.string().c_str(), std::fstream::out);
    file << m_codeFormatting->getFileHeader();
    
    file << "LIBRARY ieee;" << std::endl << "USE ieee.std_logic_1164.ALL;" << std::endl << std::endl;
    
    std::string entityName = group->getName(); ///@todo: codeFormatting
    
    std::set<const hlim::Node_Signal*> inputSignals;
    std::set<const hlim::Node_Signal*> outputSignals;
    for (const auto &node : group->getNodes()) {
        for (auto i : utils::Range(node->getNumInputs())) {
            if (node->getInput(i).connection != nullptr) {
                if (node->getInput(i).connection->node->getGroup() != group) { ///@todo check hierarchy
                    hlim::Node_Signal *signal = dynamic_cast<hlim::Node_Signal *>(node.get());
                    if (signal != nullptr) {
                        inputSignals.insert(signal);
                    } else {
                        hlim::Node_Signal *signal = dynamic_cast<hlim::Node_Signal *>(node->getInput(i).connection->node);
                        MHDL_ASSERT(signal != nullptr);
                        inputSignals.insert(signal);
                    }
                }
            }
        }
        
        for (auto i : utils::Range(node->getNumOutputs())) {
            for (const auto &connection : node->getOutput(i).connections) {
                if (connection->node->getGroup() != group) { ///@todo check hierarchy
                    hlim::Node_Signal *signal = dynamic_cast<hlim::Node_Signal *>(node.get());
                    if (signal != nullptr) {
                        outputSignals.insert(signal);
                    } else {
                        hlim::Node_Signal *signal = dynamic_cast<hlim::Node_Signal *>(connection->node);
                        MHDL_ASSERT(signal != nullptr);
                        outputSignals.insert(signal);
                    }
                }
            }
        }
    }
    
    
    file << "ENTITY " << entityName << " IS " << std::endl;
    file << m_codeFormatting->getIndentation() << "PORT(" << std::endl;
    for (const auto &signal : inputSignals) {
        std::string signalName = signal->getName(); ///@todo: codeFormatting
        std::string signalType = "something"; ///@todo: codeFormatting
        file << m_codeFormatting->getIndentation() << m_codeFormatting->getIndentation() << signalName << " : IN " << signalType << "; "<< std::endl;
    }
    for (const auto &signal : outputSignals) {
        std::string signalName = signal->getName(); ///@todo: codeFormatting
        std::string signalType = "something"; ///@todo: codeFormatting
        file << m_codeFormatting->getIndentation() << m_codeFormatting->getIndentation() << signalName << " : OUT " << signalType << "; "<< std::endl;
    }
    
    file << m_codeFormatting->getIndentation() << ");" << std::endl;
    file << "END " << entityName << ";" << std::endl << std::endl;    

    file << "ARCHITECTURE impl OF " << entityName << " IS " << std::endl;
        
    file << "END impl;" << std::endl;
}

        
}
}
}
