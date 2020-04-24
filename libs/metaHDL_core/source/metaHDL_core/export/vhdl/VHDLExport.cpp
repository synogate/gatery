#include "VHDLExport.h"

#include "VHDL_AST.h"

#include "../../utils/Range.h"
#include "../../utils/Enumerate.h"
#include "../../utils/Exceptions.h"

#include "../../hlim/coreNodes/Node_Arithmetic.h"
#include "../../hlim/coreNodes/Node_Compare.h"
#include "../../hlim/coreNodes/Node_Constant.h"
#include "../../hlim/coreNodes/Node_Logic.h"
#include "../../hlim/coreNodes/Node_Multiplexer.h"
#include "../../hlim/coreNodes/Node_Signal.h"
#include "../../hlim/coreNodes/Node_Register.h"
#include "../../hlim/coreNodes/Node_Rewire.h"

#include "VHDL_AST.h"

#include <set>
#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <functional>
#include <list>
#include <map>

namespace mhdl::core::vhdl {
    

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
#if 0
    exportGroup(circuit.getRootNodeGroup());
#else
    ast::Root root(m_codeFormatting.get());
    root.createEntity().buildFrom((hlim::NodeGroup*)circuit.getRootNodeGroup());
    root.print();
    root.write(m_destination);
#endif
}

void VHDLExport::exportGroup(const hlim::NodeGroup *group)
{
    
#if 0    
    for (const auto &child : group->getChildren())
        exportGroup(child.get());
    

    std::filesystem::path filePath = m_destination / m_codeFormatting->getFilename(group);
    std::filesystem::create_directories(filePath.parent_path());

    std::cout << "Exporting " << group->getName() << " to " << filePath << std::endl;

    std::fstream file(filePath.string().c_str(), std::fstream::out);
    file << m_codeFormatting->getFileHeader();
    
    file << "LIBRARY ieee;" << std::endl << "USE ieee.std_logic_1164.ALL;" << std::endl << "USE ieee.numeric_std.all;" << std::endl << std::endl;
    
        
    std::string entityName = group->getName(); ///@todo: codeFormatting
    
    // Helper function to produce unique signal names. 
    ///@todo must be case insensitive!
    std::map<const hlim::Node*, std::string> signalNames;
    std::set<std::string> usedSignalNames;
    auto getSignalName = [&](const hlim::Node_Signal *node)->std::string {
        auto it = signalNames.find(node);
        if (it != signalNames.end()) return it->second;
        
        std::string initialName = node->getName();
        if (initialName.empty())
            initialName = "unnamed";
        std::string name = initialName;
        size_t index = 2;
        while (usedSignalNames.find(name) != usedSignalNames.end())
            name = (boost::format("%s_%d") % initialName % index++).str();
        signalNames[node] = name;
        usedSignalNames.insert(name);
        return name;
    };
    
    
    auto formatConnectionType = [](std::fstream &stream, const hlim::ConnectionType &connectionType) {
        switch (connectionType.interpretation) {
            case hlim::ConnectionType::BOOL:
                stream << "STD_LOGIC";
            break;
            case hlim::ConnectionType::RAW:
                stream << "STD_LOGIC_VECTOR("<<connectionType.width-1 << " downto 0)";
            break;
            case hlim::ConnectionType::UNSIGNED:
                stream << "STD_LOGIC_UNSIGNED("<<connectionType.width-1 << " downto 0)";
            break;
            case hlim::ConnectionType::SIGNED_2COMPLEMENT:
                stream << "STD_LOGIC_SIGNED("<<connectionType.width-1 << " downto 0)";
            break;
            
            default:
                stream << "UNHANDLED_DATA_TYPE";
        };        
    };
    
    
    // Find all input and output signals based on node group assignment
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
    
    // Find all signals that need to be made explicit due to explicit naming, vhdl language restrictions, etc.
    std::set<const hlim::Node_Signal*> requiredCombinatorialSignals = outputSignals;
    std::set<const hlim::Node_Signal*> requiredRegisterSignals;
    std::set<const hlim::Node_Register*> requiredRegisters;
    {
        std::set<const hlim::Node_Signal*> closedList;
        std::set<const hlim::Node_Signal*> openList;
        openList = outputSignals;
        closedList = inputSignals;
        
        while (!openList.empty()) {
            const hlim::Node_Signal* signal = *openList.begin();
            openList.erase(openList.begin());
            closedList.insert(signal);
            
            if (!signal->getName().empty())
                requiredCombinatorialSignals.insert(signal);
            
            //MHDL_DESIGNCHECK_HINT(signal->getInput(0).connection != nullptr, "Undriven signal used to compose outputs!");
            if (signal->getInput(0).connection == nullptr) continue; ///@todo Enforce
            
            const hlim::Node *driver = signal->getInput(0).connection->node;        
//std::cout << "  driver " << driver->getName() << " of type " << driver->getTypeName() << std::endl;
            
            if (dynamic_cast<const hlim::Node_Signal*>(driver) != nullptr) {
                auto driverSignal = (const hlim::Node_Signal*)driver;
                
                if (closedList.find(driverSignal) != closedList.end()) continue;                
                openList.insert(driverSignal);
            } else {
                if (dynamic_cast<const hlim::Node_Register*>(driver) != nullptr) {
                    requiredRegisters.insert((const hlim::Node_Register*)driver);
                    
                    requiredRegisterSignals.insert(signal);
                    
                    MHDL_ASSERT(dynamic_cast<const hlim::Node_Signal*>(driver->getInput(0).connection->node) != nullptr);
                    requiredCombinatorialSignals.insert(dynamic_cast<const hlim::Node_Signal*>(driver->getInput(0).connection->node));

                    MHDL_ASSERT(dynamic_cast<const hlim::Node_Signal*>(driver->getInput(1).connection->node) != nullptr);
                    requiredCombinatorialSignals.insert(dynamic_cast<const hlim::Node_Signal*>(driver->getInput(1).connection->node));

                    MHDL_ASSERT(dynamic_cast<const hlim::Node_Signal*>(driver->getInput(2).connection->node) != nullptr);
                    requiredCombinatorialSignals.insert(dynamic_cast<const hlim::Node_Signal*>(driver->getInput(2).connection->node));
                } else
                if (dynamic_cast<const hlim::Node_Multiplexer*>(driver) != nullptr) {
                    requiredCombinatorialSignals.insert(signal);
                }
                
                for (auto i : utils::Range(driver->getNumInputs())) {
                    auto driverSignal = dynamic_cast<const hlim::Node_Signal*>(driver->getInput(i).connection->node);
                    MHDL_ASSERT(driverSignal);
                    if (closedList.find(driverSignal) != closedList.end()) continue;
                    openList.insert(driverSignal);
//std::cout << "      adding signal " << getSignalName(driverSignal) << std::endl;
                }
            }
        }
    }
    
    for (auto sig : requiredRegisterSignals) {
        auto it = requiredCombinatorialSignals.find(sig);
        if (it != requiredCombinatorialSignals.end())
            requiredCombinatorialSignals.erase(it);
    }
    
    file << "ENTITY " << entityName << " IS " << std::endl;
    file << m_codeFormatting->getIndentation() << "PORT(" << std::endl;
    for (const auto &signal : inputSignals) {
        std::string signalName = getSignalName(signal); ///@todo: codeFormatting
        file << m_codeFormatting->getIndentation() << m_codeFormatting->getIndentation() << signalName << " : IN ";
        formatConnectionType(file, signal->getConnectionType());
        file << "; "<< std::endl;
    }
    for (const auto &signal : outputSignals) {
        std::string signalName = getSignalName(signal); ///@todo: codeFormatting
        file << m_codeFormatting->getIndentation() << m_codeFormatting->getIndentation() << signalName << " : OUT ";
        formatConnectionType(file, signal->getConnectionType());
        file << "; "<< std::endl;
    }
    
    file << m_codeFormatting->getIndentation() << ");" << std::endl;
    file << "END " << entityName << ";" << std::endl << std::endl;    

    file << "ARCHITECTURE impl OF " << entityName << " IS " << std::endl;
    file << m_codeFormatting->getIndentation() << "-- Combinatorial signals" << std::endl;
    for (const auto &signal : requiredCombinatorialSignals) {
        if (inputSignals.find(signal) == inputSignals.end() && outputSignals.find(signal) == outputSignals.end()) {
            file << m_codeFormatting->getIndentation() << "SIGNAL " << getSignalName(signal) << " : ";
            formatConnectionType(file, signal->getConnectionType());
            file << "; "<< std::endl;
        }
    }
    file << m_codeFormatting->getIndentation() << "-- Register signals" << std::endl;
    for (const auto &signal : requiredRegisterSignals) {
        if (inputSignals.find(signal) == inputSignals.end() && outputSignals.find(signal) == outputSignals.end()) {
            file << m_codeFormatting->getIndentation() << "SIGNAL " << getSignalName(signal) << " : ";
            formatConnectionType(file, signal->getConnectionType());
            file << "; "<< std::endl;
        }
    }
        
    std::function<void(std::fstream&, const hlim::Node *node)> formatNode;    
    formatNode = [&](std::fstream &stream, const hlim::Node *node) {
        const hlim::Node_Signal *signalNode = dynamic_cast<const hlim::Node_Signal *>(node);
        if (signalNode != nullptr) { 
//            std::cout << getSignalName(signalNode) << std::endl;; 
            if (requiredCombinatorialSignals.find(signalNode) != requiredCombinatorialSignals.end() ||
                requiredRegisterSignals.find(signalNode) != requiredRegisterSignals.end() ||
                inputSignals.find(signalNode) != inputSignals.end() ||
                outputSignals.find(signalNode) != outputSignals.end())
                
                stream << getSignalName(signalNode); 
            else {
                if (signalNode->getInput(0).connection == nullptr)
                    stream << "UNCONNECTED";
                else
                    formatNode(stream, signalNode->getInput(0).connection->node);
            }
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
                case hlim::Node_Arithmetic::REM: stream << " MOD "; break;
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
            for (size_t i = 1; i+1 < muxNode->getNumInputs(); i++) {
                formatNode(stream, muxNode->getInput(i).connection->node);
                stream << " when ( ";
                formatNode(stream, muxNode->getInput(0).connection->node);
                stream << " = \"" << (i-1) << "\" ) else ";
            }
            formatNode(stream, muxNode->getInput(muxNode->getNumInputs()-1).connection->node);
            stream << ")";
            return; 
        }
        const hlim::Node_Rewire *rewireNode = dynamic_cast<const hlim::Node_Rewire*>(node);
        if (rewireNode != nullptr) { 
            
            size_t bitExtractIdx;
            if (rewireNode->getOp().isBitExtract(bitExtractIdx)) {
                formatNode(stream, rewireNode->getInput(0).connection->node);
                
                ///@todo be mindfull of bits vs single element vectors!
                stream << "[" << bitExtractIdx << "]";
            } else {
                
                const auto &op = rewireNode->getOp().ranges;
                if (op.size() > 1)
                    stream << "(";

                for (auto i : utils::Range(op.size())) {
                    if (i > 0)
                        stream << " & ";
                    const auto &range = op[op.size()-1-i];
                    switch (range.source) {
                        case hlim::Node_Rewire::OutputRange::INPUT:
                            formatNode(stream, rewireNode->getInput(range.inputIdx).connection->node);
                            stream << "(" << range.inputOffset + range.subwidth - 1 << " downto " << range.inputOffset << ")";
                        break;
                        case hlim::Node_Rewire::OutputRange::CONST_ZERO:
                            stream << '"';
                            for (auto j : utils::Range(range.subwidth))
                                stream << "0";
                            stream << '"';
                        break;
                        case hlim::Node_Rewire::OutputRange::CONST_ONE:
                            stream << '"';
                            for (auto j : utils::Range(range.subwidth))
                                stream << "1";
                            stream << '"';
                        break;
                        default:
                            stream << "UNHANDLED_REWIRE_OP";
                    }
                }
                
                if (op.size() > 1)
                    stream << ")";
            }

            return; 
        }

        if (const hlim::Node_Constant* constNode = dynamic_cast<const hlim::Node_Constant*>(node))
        {
            // todo: what is the right way to get node width?
            const auto& conType = ((const hlim::Node_Signal*)(*constNode->getOutput(0).connections.begin())->node)->getConnectionType();

            char sep = '"';
            if (conType.interpretation == hlim::ConnectionType::BOOL)
                sep = '\'';

            stream << sep;
            for (bool b : constNode->getValue().bitVec)
                stream << (b ? '1' : '0');
            stream << sep;
            return;
        }

        stream << "unhandled_operation" << node->getTypeName();
    };
    
    file << "BEGIN" << std::endl;
    
    
    
    
    
    file << m_codeFormatting->getIndentation() << "combinatorial : PROCESS(";
    {
        bool first = true;
        for (const auto &signal : requiredCombinatorialSignals) {
            if (!first) 
                file << ", "; 
            first = false;
            file << getSignalName(signal);
        }
        for (const auto &signal : requiredRegisterSignals) {
            if (!first)
                file << ", ";
            first = false;
            file << getSignalName(signal);
        }
    }
    file << ")" << std::endl;
    file << m_codeFormatting->getIndentation() << "BEGIN" << std::endl;
    for (const hlim::Node_Signal *signal : requiredCombinatorialSignals) {
        if (inputSignals.find(signal) != inputSignals.end()) continue;
        
        file << m_codeFormatting->getIndentation() << m_codeFormatting->getIndentation() << getSignalName(signal) << " <= ";
        if (signal->getInput(0).connection != nullptr) {
            hlim::Node *sourceNode = signal->getInput(0).connection->node;
            formatNode(file, sourceNode);
        } else {
            file << "UNCONNECTED";
        }
        file << ";" << std::endl;
    }
    
    file << m_codeFormatting->getIndentation() << "END PROCESS;" << std::endl << std::endl;

    file << m_codeFormatting->getIndentation() << "registers : PROCESS(clk)" << std::endl;
    file << m_codeFormatting->getIndentation() << "BEGIN" << std::endl;
    file << m_codeFormatting->getIndentation() << m_codeFormatting->getIndentation() << "IF (rising_edge(clk)) THEN" << std::endl;
    for (const hlim::Node_Register *reg : requiredRegisters) {
        const hlim::Node_Signal *sourceNode = dynamic_cast<const hlim::Node_Signal *>(reg->getInput(0).connection->node);
        for (const auto &outputConnection : reg->getOutput(0).connections) {
            const hlim::Node_Signal *destinationNode = dynamic_cast<const hlim::Node_Signal *>(outputConnection->node);

            file << m_codeFormatting->getIndentation() << m_codeFormatting->getIndentation() << m_codeFormatting->getIndentation() << getSignalName(destinationNode) << " <= ";
            formatNode(file, sourceNode);
            file << ";" << std::endl;
        }
    }
    file << m_codeFormatting->getIndentation() << m_codeFormatting->getIndentation() << "END IF;" << std::endl;
    file << m_codeFormatting->getIndentation() << "END PROCESS;" << std::endl;

    file << "END impl;" << std::endl;
    
#endif
    
}


}
