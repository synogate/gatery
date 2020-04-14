#include "VHDLExport.h"

#include "../../utils/Range.h"
#include "../../utils/Enumerate.h"
#include "../../utils/Exceptions.h"

#include "../../hlim//coreNodes/Node_Arithmetic.h"
#include "../../hlim//coreNodes/Node_Compare.h"
#include "../../hlim//coreNodes/Node_Logic.h"
#include "../../hlim//coreNodes/Node_Multiplexer.h"
#include "../../hlim//coreNodes/Node_Signal.h"
#include "../../hlim//coreNodes/Node_Register.h"

#include <set>
#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <functional>

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
    
    std::map<const hlim::Node*, std::string> signalNames;
    std::set<std::string> usedSignalNames;
    auto getSignalName = [&](const hlim::Node_Signal *node)->std::string {
        auto it = signalNames.find(node);
        if (it != signalNames.end()) return it->second;
        
        std::string initialName = node->getName();
        if (initialName.empty())
            initialName = "unnamed";
        std::string name = initialName;
        unsigned index = 2;
        while (usedSignalNames.find(name) != usedSignalNames.end())
            name = (boost::format("%s_%d") % initialName % index++).str();
        signalNames[node] = name;
        usedSignalNames.insert(name);
        return name;
    };
    
    
    std::set<const hlim::Node_Signal*> inputSignals;
    std::set<const hlim::Node_Signal*> outputSignals;
    for (const auto &node : group->getNodes()) {
        for (auto i : utils::Range(node->getNumInputs())) {
            if (node->getInput(i).connection != nullptr) {
                if (node->getInput(i).connection->node->getGroup() != group) { ///@todo check hierarchy
#if 0
                    hlim::Node_Signal *signal = dynamic_cast<hlim::Node_Signal *>(node.get());
                    if (signal != nullptr) {
                        inputSignals.insert(signal);
                    } else {
                        hlim::Node_Signal *signal = dynamic_cast<hlim::Node_Signal *>(node->getInput(i).connection->node);
                        MHDL_ASSERT(signal != nullptr);
                        inputSignals.insert(signal);
                    }
#else
                    hlim::Node_Signal *signal = dynamic_cast<hlim::Node_Signal *>(node->getInput(i).connection->node);
                    MHDL_ASSERT(signal != nullptr);
                    inputSignals.insert(signal);
#endif
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
        std::string signalName = getSignalName(signal); ///@todo: codeFormatting
        std::string signalType = "something"; ///@todo: codeFormatting
        file << m_codeFormatting->getIndentation() << m_codeFormatting->getIndentation() << signalName << " : IN " << signalType << "; "<< std::endl;
    }
    for (const auto &signal : outputSignals) {
        std::string signalName = getSignalName(signal); ///@todo: codeFormatting
        std::string signalType = "something"; ///@todo: codeFormatting
        file << m_codeFormatting->getIndentation() << m_codeFormatting->getIndentation() << signalName << " : OUT " << signalType << "; "<< std::endl;
    }
    
    file << m_codeFormatting->getIndentation() << ");" << std::endl;
    file << "END " << entityName << ";" << std::endl << std::endl;    

    file << "ARCHITECTURE impl OF " << entityName << " IS " << std::endl;
    for (const auto &node : group->getNodes()) {
        hlim::Node_Signal *signal = dynamic_cast<hlim::Node_Signal *>(node.get());
        if (signal != nullptr) {
            if (!signal->isOrphaned())
                if (inputSignals.find(signal) == inputSignals.end() && outputSignals.find(signal) == outputSignals.end()) {
                    std::string signalType = "something"; ///@todo: codeFormatting
                    file << m_codeFormatting->getIndentation() << "SIGNAL " << getSignalName(signal) << " : " << signalType << "; "<< std::endl;
                }
        }
    }
        
    std::function<void(std::fstream&, const hlim::Node *node)> formatNode;    
    formatNode = [&](std::fstream &stream, const hlim::Node *node) {
        const hlim::Node_Signal *signalNode = dynamic_cast<const hlim::Node_Signal *>(node);
        if (signalNode != nullptr) { 
            if (!signalNode->getName().empty() || signalNode->getInput(0).connection == nullptr) 
                stream << getSignalName(signalNode); 
            else
                formatNode(stream, signalNode->getInput(0).connection->node);
            return;            
        }
        
        const hlim::Node_Arithmetic *arithmeticNode = dynamic_cast<const hlim::Node_Arithmetic*>(node);
        if (arithmeticNode != nullptr) { 
            stream << "(";
            formatNode(stream, arithmeticNode->getInput(0).connection->node);
            switch (arithmeticNode->getOp()) {
                case hlim::Node_Arithmetic::ADD: stream << " + "; break;
                case hlim::Node_Arithmetic::SUB: stream << " - "; break;
                case hlim::Node_Arithmetic::MUL: stream << " * "; break;
                case hlim::Node_Arithmetic::DIV: stream << " / "; break;
                case hlim::Node_Arithmetic::REM: stream << " % "; break;
                default:
                    MHDL_ASSERT_HINT(false, "Unhandled operation!");
            };
            formatNode(stream, arithmeticNode->getInput(1).connection->node);
            stream << ")";
            return; 
        }
        
        const hlim::Node_Logic *logicNode = dynamic_cast<const hlim::Node_Logic*>(node);
        if (logicNode != nullptr) { 
            stream << "(";
            switch (logicNode->getOp()) {
                case hlim::Node_Logic::AND: formatNode(stream, logicNode->getInput(0).connection->node); stream << " and ";  formatNode(stream, logicNode->getInput(1).connection->node); break;
                case hlim::Node_Logic::NAND: formatNode(stream, logicNode->getInput(0).connection->node); stream << " nand ";  formatNode(stream, logicNode->getInput(1).connection->node); break;
                case hlim::Node_Logic::OR: formatNode(stream, logicNode->getInput(0).connection->node); stream << " or ";  formatNode(stream, logicNode->getInput(1).connection->node); break;
                case hlim::Node_Logic::NOR: formatNode(stream, logicNode->getInput(0).connection->node); stream << " nor ";  formatNode(stream, logicNode->getInput(1).connection->node); break;
                case hlim::Node_Logic::XOR: formatNode(stream, logicNode->getInput(0).connection->node); stream << " xor ";  formatNode(stream, logicNode->getInput(1).connection->node); break;
                case hlim::Node_Logic::EQ: formatNode(stream, logicNode->getInput(0).connection->node); stream << " xnor ";  formatNode(stream, logicNode->getInput(1).connection->node); break;
                case hlim::Node_Logic::NOT: stream << " not "; formatNode(stream, logicNode->getInput(0).connection->node); break;
                default:
                    MHDL_ASSERT_HINT(false, "Unhandled operation!");
            };
            stream << ")";
            return; 
        }
        const hlim::Node_Multiplexer *muxNode = dynamic_cast<const hlim::Node_Multiplexer*>(node);
        if (muxNode != nullptr) { 
            stream << "(";
            for (unsigned i = 1; i+1 < muxNode->getNumInputs(); i++) {
                formatNode(stream, muxNode->getInput(i).connection->node);
                stream << " when ( ";
                formatNode(stream, muxNode->getInput(0).connection->node);
                stream << " = \"" << (i-1) << "\" ) else ";
            }
            formatNode(stream, muxNode->getInput(muxNode->getNumInputs()-1).connection->node);
            stream << ")";
            return; 
        }
        stream << "unhandled_operation" << node->getTypeName();
    };
        
    file << "BEGIN" << std::endl;
    file << m_codeFormatting->getIndentation() << "combinatory : process()" << std::endl;
    file << m_codeFormatting->getIndentation() << "BEGIN" << std::endl;
    for (const auto &node : group->getNodes()) {
        hlim::Node_Signal *signal = dynamic_cast<hlim::Node_Signal *>(node.get());
        if (signal != nullptr) {
            if (!signal->getName().empty() && node->getInput(0).connection != nullptr) {
                hlim::Node *sourceNode = node->getInput(0).connection->node;

                file << m_codeFormatting->getIndentation() << m_codeFormatting->getIndentation() << getSignalName(signal) << " := ";
                formatNode(file, sourceNode);
                file << ";" << std::endl;
                
            }
        }
    }
    
    file << m_codeFormatting->getIndentation() << "END PROCESS;" << std::endl << std::endl;

    file << m_codeFormatting->getIndentation() << "registers : process(clk)" << std::endl;
    file << m_codeFormatting->getIndentation() << "BEGIN" << std::endl;
    file << m_codeFormatting->getIndentation() << m_codeFormatting->getIndentation() << "IF (rising_edge(clk)) THEN" << std::endl;
    for (const auto &node : group->getNodes()) {
        const hlim::Node_Register *reg = dynamic_cast<const hlim::Node_Register *>(node.get());
        if (reg != nullptr) {
            if (reg->getInput(0).connection != nullptr) {
                const hlim::Node_Signal *sourceNode = dynamic_cast<const hlim::Node_Signal *>(reg->getInput(0).connection->node);
                for (const auto &outputConnection : reg->getOutput(0).connections) {
                    const hlim::Node_Signal *destinationNode = dynamic_cast<const hlim::Node_Signal *>(outputConnection->node);

                    file << m_codeFormatting->getIndentation() << m_codeFormatting->getIndentation() << m_codeFormatting->getIndentation() << getSignalName(destinationNode) << " := ";
                    formatNode(file, sourceNode);
                    file << ";" << std::endl;
                }
            }
        }
    }
    file << m_codeFormatting->getIndentation() << m_codeFormatting->getIndentation() << "END IF;" << std::endl;
    file << m_codeFormatting->getIndentation() << "END PROCESS;" << std::endl;

    file << "END impl;" << std::endl;
}

        
}
}
}
