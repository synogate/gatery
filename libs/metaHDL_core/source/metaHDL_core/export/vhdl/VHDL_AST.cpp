#include "VHDL_AST.h"

#include "../../hlim/NodeCategorization.h"
#include "../../hlim/Node.h"

#include "../../hlim/coreNodes/Node_Arithmetic.h"
#include "../../hlim/coreNodes/Node_Compare.h"
#include "../../hlim/coreNodes/Node_Constant.h"
#include "../../hlim/coreNodes/Node_Logic.h"
#include "../../hlim/coreNodes/Node_Multiplexer.h"
#include "../../hlim/coreNodes/Node_Signal.h"
#include "../../hlim/coreNodes/Node_Register.h"
#include "../../hlim/coreNodes/Node_Rewire.h"


#include "../../utils/Range.h"

#include <iostream>
#include <fstream>
#include <iomanip>

namespace mhdl::core::vhdl::ast {
    
    
namespace {
    
void formatConnectionType(std::ostream &stream, const hlim::ConnectionType &connectionType) 
{
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

    
}
    
/*
std::string Namespace::getName(hlim::NodePort nodePort, const std::string &desiredName)
{
    auto it = m_nodeNames.find(nodePort);
    if (it != m_nodeNames.end()) return it->second;
    
    unsigned attempt = 0;
    std::string name;
    do {
        name = m_codeFormatting->getNodeName(node, attempt++);
    } while (isNameInUse(name));

    m_nodeNames[node] = name;
    m_namesInUse.insert(name);
    return name;    
}
*/

std::string Namespace::allocateName(const std::string &desiredName, CodeFormatting::SignalType type)
{
    unsigned attempt = 0;
    std::string name;
    do {
        name = m_codeFormatting->getSignalName(desiredName, type, attempt++);
    } while (isNameInUse(name));

    m_namesInUse.insert(name);
    return name;    
}

std::string Namespace::getGlobalsName(const std::string &id)
{
    auto it = m_globalsNames.find(id);
    if (it != m_globalsNames.end()) return it->second;
    
    unsigned attempt = 0;
    std::string name;
    do {
        name = m_codeFormatting->getGlobalName(id, attempt++);
    } while (isNameInUse(name));

    m_globalsNames[id] = name;
    m_namesInUse.insert(name);
    return name;    
}
        
void Namespace::setup(Namespace *parent, CodeFormatting *codeFormatting)
{
    m_parent = parent;
    m_codeFormatting = codeFormatting;
}

bool Namespace::isNameInUse(const std::string &name) const
{
    if (m_namesInUse.find(name) != m_namesInUse.end()) return true;
    if (m_parent != nullptr)
        return m_parent->isNameInUse(name);
    return false;
}



BaseBlock::BaseBlock(Namespace *parent, CodeFormatting *codeFormatting, hlim::NodeGroup *nodeGroup)
{
    m_namespace.setup(parent, codeFormatting);
    m_nodeGroup = nodeGroup;
}

void BaseBlock::extractExplicitSignals()
{
    // Find all explicit signals/variables (signals that will need to be declared and assigned)
    for (auto node : m_nodeGroup->getNodes()) {
        
#if 0
        // Named signals are explicit
        if (dynamic_cast<hlim::Node_Signal*>(node) != nullptr && !node->getName().empty()) {
            hlim::NodePort driver = {.node = node, .port = 0};
            ExplicitSignal &explicitSignal = m_explicitSignals[driver];
            explicitSignal.producerOutput = driver;
            
            if (explicitSignal.desiredName.empty())
                explicitSignal.desiredName = node->getName();
            
            explicitSignal.hintedExplicit = true;
        }
#endif
        
        // Check for inputs
        for (auto i : utils::Range(node->getNumInputPorts())) {
            auto driver = node->getDriver(i);

            if (driver.node == nullptr) {
                std::cout << "Warning: Unconnected node: Port " << i << " of node '"<<node->getName()<<"' not connected!" << std::endl;
                std::cout << node->getStackTrace() << std::endl;
            } else if (driver.node->getGroup() != m_nodeGroup) {
                
                ExplicitSignal &explicitSignal = m_explicitSignals[driver];
                explicitSignal.producerOutput = driver;
                
                if (explicitSignal.desiredName.empty())
                    explicitSignal.desiredName = driver.node->getName(); // prefer driver's name for inputs
                if (explicitSignal.desiredName.empty())
                    explicitSignal.desiredName = node->getName();
                
                if (driver.node->getGroup() != nullptr && driver.node->getGroup()->isChildOf(m_nodeGroup)) { // Either it is being driven by a child module...
                    explicitSignal.drivenByChild = true;
                } else {                                             // .. or by an outside node.
                    explicitSignal.drivenByExternal = true;
                }
            }
        }
        
        // Check for outputs
        for (auto i : utils::Range(node->getNumOutputPorts())) {
            if (node->getDirectlyDriven(i).empty()) {
                std::cout << "Warning: Unused node: Port " << i << " of node '"<<node->getName()<<"' not connected!" << std::endl;
                std::cout << node->getStackTrace() << std::endl;
            }
            hlim::NodePort driver;
            driver.node = node;
            driver.port = i;
            
            for (auto driven : node->getDirectlyDriven(i)) {
                if (driven.node->getGroup() != m_nodeGroup) {
                
                    ExplicitSignal &explicitSignal = m_explicitSignals[driver];
                    explicitSignal.producerOutput = driver;
                    
                    if (explicitSignal.desiredName.empty())
                        explicitSignal.desiredName = node->getName(); // prefer own name for outputs
                    if (explicitSignal.desiredName.empty())
                        explicitSignal.desiredName = driven.node->getName(); 
                    
                    if (driven.node->getGroup() != nullptr && driven.node->getGroup()->isChildOf(m_nodeGroup)) { // Either it is driving a child module...
                        explicitSignal.drivingChild = true;
                    } else {                                             // .. or an outside node.
                        explicitSignal.drivingExternal = true;
                    }
                }
            }
        }
        
        // Check for multiple use
        for (auto i : utils::Range(node->getNumOutputPorts())) {
            if (node->getDirectlyDriven(i).size() > 1) {
                hlim::NodePort driver;
                driver.node = node;
                driver.port = i;

                ExplicitSignal &explicitSignal = m_explicitSignals[driver];
                explicitSignal.producerOutput = driver;
                
                if (explicitSignal.desiredName.empty())
                    explicitSignal.desiredName = node->getName();
                
                explicitSignal.multipleConsumers = true;
            }
        }
        
        
        // check for multiplexer
        hlim::Node_Multiplexer *muxNode = dynamic_cast<hlim::Node_Multiplexer *>(node);
        if (muxNode != nullptr) {
            
            // Handle output
            {
                hlim::NodePort driver;
                driver.node = muxNode;
                driver.port = 0;

                ExplicitSignal &explicitSignal = m_explicitSignals[driver];
                explicitSignal.producerOutput = driver;
                
                if (explicitSignal.desiredName.empty() && !muxNode->getDirectlyDriven(0).empty() && dynamic_cast<hlim::Node_Signal*>(muxNode->getDirectlyDriven(0)[0].node) != nullptr)
                    explicitSignal.desiredName = muxNode->getDirectlyDriven(0)[0].node->getName();
                if (explicitSignal.desiredName.empty())
                    explicitSignal.desiredName = node->getName();                
                
                explicitSignal.syntaxNecessity = true;
            }
        }
        
        
        // check for registers
        hlim::Node_Register *regNode = dynamic_cast<hlim::Node_Register *>(node);
        if (regNode != nullptr) {
            
            // Handle output
            {
                hlim::NodePort driver;
                driver.node = regNode;
                driver.port = 0;
                
#if 0                
                // find first explicit or branching signal within same group
                while (true) {
                    if (m_explicitSignals.find(driver) == m_explicitSignals.end())
                        break;
                    
                    if (driver.node->getDirectlyDriven(driver.port).size() != 1)
                        break;
                    
                    auto nextInput = driver.node->getDirectlyDriven(driver.port).front();
                    if (dynamic_cast<hlim::Node_Signal*>(nextInput.node) == nullptr)
                        break;
                    
                    if (nextInput.node->getGroup() != nodeGroup)
                        break;                    
                    
                    driver.node = nextInput.node;
                    driver.port = 0;
                }

                ExplicitSignal &explicitSignal = m_explicitSignals[driver];
                explicitSignal.producerOutput = driver;
                
                if (explicitSignal.desiredName.empty())
                    explicitSignal.desiredName = node->getName();
#else
                ExplicitSignal &explicitSignal = m_explicitSignals[driver];
                explicitSignal.producerOutput = driver;
                
                if (explicitSignal.desiredName.empty() && !regNode->getDirectlyDriven(0).empty() && dynamic_cast<hlim::Node_Signal*>(regNode->getDirectlyDriven(0)[0].node) != nullptr)
                    explicitSignal.desiredName = regNode->getDirectlyDriven(0)[0].node->getName();
                if (explicitSignal.desiredName.empty())
                    explicitSignal.desiredName = node->getName();                
#endif
                
                explicitSignal.registerOutput = true;
            }
            
            // Handle inputs (data, enable, reset value)
            for (auto input : std::array<hlim::Node_Register::Input, 3>{hlim::Node_Register::DATA, hlim::Node_Register::ENABLE, hlim::Node_Register::RESET_VALUE}) {
                hlim::NodePort driver = regNode->getDriver(input);

                if (driver.node == nullptr) {
                    std::cout << "Warning: Unused node: Port " << input << " of node '"<<node->getName()<<"' not connected!" << std::endl;
                    std::cout << node->getStackTrace() << std::endl;
                } else {
                    ExplicitSignal &explicitSignal = m_explicitSignals[driver];
                    explicitSignal.producerOutput = driver;
                    
                    if (explicitSignal.desiredName.empty())
                        explicitSignal.desiredName = driver.node->getName();
                    
                    explicitSignal.registerInput = true;
                }
            }
        }
    }
    
    

    
    std::cout << "Found " << m_explicitSignals.size() << " explicit signals" << std::endl;
    for (auto p : m_explicitSignals) {
        std::cout << "Explicit signal " << p.second.desiredName << std::endl;
        std::cout << "    drivenByExternal " << (p.second.drivenByExternal?"true":"false") << std::endl;
        std::cout << "    drivingExternal " << (p.second.drivingExternal?"true":"false") << std::endl;
        std::cout << "    drivenByChild " << (p.second.drivenByChild?"true":"false") << std::endl;
        std::cout << "    drivingChild " << (p.second.drivingChild?"true":"false") << std::endl;
        std::cout << "    hintedExplicit " << (p.second.hintedExplicit?"true":"false") << std::endl;
        std::cout << "    syntaxNecessity " << (p.second.syntaxNecessity?"true":"false") << std::endl;
        std::cout << "    registerInput " << (p.second.registerInput?"true":"false") << std::endl;
        std::cout << "    registerOutput " << (p.second.registerOutput?"true":"false") << std::endl;
        std::cout << "    multipleConsumers " << (p.second.multipleConsumers?"true":"false") << std::endl;
    }
}


void BaseBlock::allocateLocalSignals()
{
    for (auto &p : m_explicitSignals) {
        if (m_signalDeclaration.signalNames.find(p.first) != m_signalDeclaration.signalNames.end()) continue;
        
        m_signalDeclaration.localSignals.push_back(p.first);
        m_signalDeclaration.signalNames[p.first] = m_namespace.allocateName(p.second.desiredName, CodeFormatting::SIG_LOCAL_VARIABLE);
    }
}


Process::Process(Entity &parent, hlim::NodeGroup *nodeGroup) : BaseBlock(&parent.getNamespace(), parent.getRoot().getCodeFormatting(), nodeGroup), m_parent(parent)
{ 
    m_name = parent.getNamespace().getGlobalsName(nodeGroup->getName());    
}

void Process::allocateExternalIOSignals()
{
    for (auto &p : m_explicitSignals) {
        if (m_signalDeclaration.signalNames.find(p.first) != m_signalDeclaration.signalNames.end()) continue;
        
        MHDL_ASSERT(!p.second.drivenByExternal || !p.second.drivingExternal);
        
        if (p.second.drivenByExternal) {
            if (isInterEntityInputSignal(p.first)) {
                auto it = m_parent.getSignalDeclaration().signalNames.find(p.first);
                if (it == m_parent.getSignalDeclaration().signalNames.end()) {
                    auto actualName = m_parent.getNamespace().allocateName(p.second.desiredName, CodeFormatting::SIG_ENTITY_INPUT);
                    m_parent.getSignalDeclaration().inputSignals.push_back(p.first);
                    m_parent.getSignalDeclaration().signalNames[p.first] = actualName;
                    m_signalDeclaration.signalNames[p.first] = actualName;
                } else {
                    m_signalDeclaration.signalNames[p.first] = it->second;
                }
                m_signalDeclaration.inputSignals.push_back(p.first);
            }
        } 
        if (p.second.drivingExternal) {
            if (isInterEntityOutputSignal(p.first)) {
                auto it = m_parent.getSignalDeclaration().signalNames.find(p.first);
                if (it == m_parent.getSignalDeclaration().signalNames.end()) {
                    auto actualName = m_parent.getNamespace().allocateName(p.second.desiredName, CodeFormatting::SIG_ENTITY_OUTPUT);
                    m_parent.getSignalDeclaration().outputSignals.push_back(p.first);
                    m_parent.getSignalDeclaration().signalNames[p.first] = actualName;
                    m_signalDeclaration.signalNames[p.first] = actualName;
                } else {
                    m_signalDeclaration.signalNames[p.first] = it->second;
                }
                m_signalDeclaration.outputSignals.push_back(p.first);
            }
        }
    }
}

void Process::allocateIntraEntitySignals()
{
    for (auto &p : m_explicitSignals) {
        if (m_signalDeclaration.signalNames.find(p.first) != m_signalDeclaration.signalNames.end()) continue;
        
        MHDL_ASSERT(!p.second.drivenByExternal || !p.second.drivingExternal);
        
        if (p.second.drivenByExternal || p.second.drivingExternal) {
            if (!isInterEntityInputSignal(p.first)) {
                // parent entity is the crossover point, wire through local signal
                
                auto it = m_parent.getSignalDeclaration().signalNames.find(p.first);
                if (it == m_parent.getSignalDeclaration().signalNames.end()) {
                    auto actualName = m_parent.getNamespace().allocateName(p.second.desiredName, CodeFormatting::SIG_LOCAL_SIGNAL);
                    m_parent.getSignalDeclaration().localSignals.push_back(p.first);
                    m_parent.getSignalDeclaration().signalNames[p.first] = actualName;
                    m_signalDeclaration.signalNames[p.first] = actualName;
                } else {
                    m_signalDeclaration.signalNames[p.first] = it->second;
                }
                if (p.second.drivenByExternal)
                    m_signalDeclaration.inputSignals.push_back(p.first);
                else
                    m_signalDeclaration.outputSignals.push_back(p.first);
            }
        }
    }
}

void Process::allocateChildEntitySignals()
{
    for (auto &p : m_explicitSignals) {
        if (m_signalDeclaration.signalNames.find(p.first) != m_signalDeclaration.signalNames.end()) continue;
        
        if (p.second.drivenByChild || p.second.drivingChild) {

            auto it = m_parent.getSignalDeclaration().signalNames.find(p.first);
            if (it == m_parent.getSignalDeclaration().signalNames.end()) {
                auto actualName = m_parent.getNamespace().allocateName(p.second.desiredName, p.second.drivingChild?CodeFormatting::SIG_CHILD_ENTITY_INPUT:CodeFormatting::SIG_CHILD_ENTITY_OUTPUT);
                m_parent.getSignalDeclaration().localSignals.push_back(p.first);
                m_parent.getSignalDeclaration().signalNames[p.first] = actualName;
                m_signalDeclaration.signalNames[p.first] = actualName;
            } else {
                m_signalDeclaration.signalNames[p.first] = it->second;
            }
            if (p.second.drivenByChild)
                m_signalDeclaration.inputSignals.push_back(p.first);
            else
                m_signalDeclaration.outputSignals.push_back(p.first);
        }
    }
}


void Process::allocateRegisterSignals()
{
    for (auto &p : m_explicitSignals) {
        if (m_signalDeclaration.signalNames.find(p.first) != m_signalDeclaration.signalNames.end()) continue;

        if (p.second.registerInput || p.second.registerOutput) {
            auto it = m_parent.getSignalDeclaration().signalNames.find(p.first);
            if (it == m_parent.getSignalDeclaration().signalNames.end()) {
                auto actualName = m_parent.getNamespace().allocateName(p.second.desiredName, p.second.registerInput?CodeFormatting::SIG_REGISTER_INPUT:CodeFormatting::SIG_REGISTER_OUTPUT);
                m_parent.getSignalDeclaration().localSignals.push_back(p.first);
                m_parent.getSignalDeclaration().signalNames[p.first] = actualName;
                m_signalDeclaration.signalNames[p.first] = actualName;
            } else {
                m_signalDeclaration.signalNames[p.first] = it->second;
            }

            if (p.second.registerOutput)
                m_signalDeclaration.inputSignals.push_back(p.first);
            else
                m_signalDeclaration.outputSignals.push_back(p.first);
        }
    }
}


bool Process::isInterEntityInputSignal(hlim::NodePort nodePort)
{
    return nodePort.node->getGroup() == nullptr || !nodePort.node->getGroup()->isChildOf(m_parent.getNodeGroup());
}

bool Process::isInterEntityOutputSignal(hlim::NodePort nodePort)
{
    for (auto p : nodePort.node->getDirectlyDriven(nodePort.port))
        if (p.node->getGroup() == nullptr || !p.node->getGroup()->isChildOf(m_parent.getNodeGroup()))
            return true;
    return false;
}


void Process::formatExpression(std::ostream &stream, const hlim::NodePort &nodePort, std::set<hlim::NodePort> &dependentInputs, bool forceUnfold) 
{
    if (nodePort.node == nullptr) {
        stream << "UNCONNECTED";
        return;
    }
    
    if (!forceUnfold && m_explicitSignals.find(nodePort) != m_explicitSignals.end()) {
        stream << m_signalDeclaration.signalNames[nodePort];
        dependentInputs.insert(nodePort);
        return;
    }
    
    const hlim::Node_Signal *signalNode = dynamic_cast<const hlim::Node_Signal *>(nodePort.node);
    if (signalNode != nullptr) { 
        formatExpression(stream, signalNode->getDriver(0), dependentInputs);
        return;            
    }
    
    const hlim::Node_Arithmetic *arithmeticNode = dynamic_cast<const hlim::Node_Arithmetic*>(nodePort.node);
    if (arithmeticNode != nullptr) { 
        stream << "(";
        formatExpression(stream, arithmeticNode->getDriver(0), dependentInputs);
        switch (arithmeticNode->getOp()) {
            case hlim::Node_Arithmetic::ADD: stream << " + "; break;
            case hlim::Node_Arithmetic::SUB: stream << " - "; break;
            case hlim::Node_Arithmetic::MUL: stream << " * "; break;
            case hlim::Node_Arithmetic::DIV: stream << " / "; break;
            case hlim::Node_Arithmetic::REM: stream << " MOD "; break;
            default:
                MHDL_ASSERT_HINT(false, "Unhandled operation!");
        };
        formatExpression(stream, arithmeticNode->getDriver(1), dependentInputs);
        stream << ")";
        return; 
    }
    
    const hlim::Node_Logic *logicNode = dynamic_cast<const hlim::Node_Logic*>(nodePort.node);
    if (logicNode != nullptr) { 
        stream << "(";
        if (logicNode->getOp() == hlim::Node_Logic::NOT) {
            stream << " not "; formatExpression(stream, logicNode->getDriver(0), dependentInputs);
        } else {
            formatExpression(stream, logicNode->getDriver(0), dependentInputs); 
            switch (logicNode->getOp()) {
                case hlim::Node_Logic::AND: stream << " and "; break;
                case hlim::Node_Logic::NAND: stream << " nand "; break;
                case hlim::Node_Logic::OR: stream << " or ";  break;
                case hlim::Node_Logic::NOR: stream << " nor "; break;
                case hlim::Node_Logic::XOR: stream << " xor "; break;
                case hlim::Node_Logic::EQ: stream << " xnor "; break;
                default:
                    MHDL_ASSERT_HINT(false, "Unhandled operation!");
            };
            formatExpression(stream, logicNode->getDriver(1), dependentInputs);
        }
        stream << ")";
        return; 
    }

    const hlim::Node_Rewire *rewireNode = dynamic_cast<const hlim::Node_Rewire*>(nodePort.node);
    if (rewireNode != nullptr) { 
        
        size_t bitExtractIdx;
        if (rewireNode->getOp().isBitExtract(bitExtractIdx)) {
            formatExpression(stream, rewireNode->getDriver(0), dependentInputs);
            
            ///@todo be mindfull of bits vs single element vectors!
            stream << "(" << bitExtractIdx << ")";
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
                        formatExpression(stream, rewireNode->getDriver(range.inputIdx), dependentInputs);
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

    if (const hlim::Node_Constant* constNode = dynamic_cast<const hlim::Node_Constant*>(nodePort.node))
    {
        // todo: what is the right way to get node width?
        const auto& conType = constNode->getOutputConnectionType(0);

        char sep = '"';
        if (conType.interpretation == hlim::ConnectionType::BOOL)
            sep = '\'';

        stream << sep;
        for (bool b : constNode->getValue().bitVec)
            stream << (b ? '1' : '0');
        stream << sep;
        return;
    }

    stream << "unhandled_operation" << nodePort.node->getTypeName();
};


void Process::write(std::fstream &file)
{
    auto &codeFormatting = *m_parent.getRoot().getCodeFormatting();
    
    codeFormatting.indent(file, 1);
    file << "combinatorial_" << m_name << " : PROCESS(";
    {
        bool first = true;
        for (const auto &signal : m_signalDeclaration.inputSignals) {
            if (!first) 
                file << ", "; 
            first = false;
            file << m_signalDeclaration.signalNames[signal];
        }
        /*
        for (const auto &signal : m_signalDeclaration.outputSignals) {
            if (!first)
                file << ", ";
            first = false;
            file << m_signalDeclaration.signalNames[signal];
        }
        */
    }
    file << ")" << std::endl;
    
    std::set<hlim::NodePort> variableList;
    for (const auto &signal : m_signalDeclaration.localSignals) {
        variableList.insert(signal);
        codeFormatting.indent(file, 2);
        file << "variable " << m_signalDeclaration.signalNames[signal] << " : ";
        formatConnectionType(file, signal.node->getOutputConnectionType(signal.port));
        file << ";" << std::endl;
    }
    
    codeFormatting.indent(file, 1);
    file << "BEGIN" << std::endl;

    {
        struct Statement {
            std::set<hlim::NodePort> inputs;
            std::set<hlim::NodePort> outputs;
            std::string code;
        };
        
        std::vector<Statement> statements;
        
        auto constructStatementsFor = [&](hlim::NodePort nodePort) {
            std::stringstream code;
            codeFormatting.indent(code, 3);
            
            Statement statement;
            statement.outputs.insert(nodePort);
            
            hlim::Node_Multiplexer *muxNode = dynamic_cast<hlim::Node_Multiplexer *>(nodePort.node);
            if (muxNode != nullptr) {
                code << "IF (";
                formatExpression(code, muxNode->getDriver(0), statement.inputs, false);
                code << " = '1') THEN"<< std::endl;
                
                    codeFormatting.indent(code, 4);
                    code << m_signalDeclaration.signalNames[nodePort];
                    if (variableList.find(nodePort) != variableList.end())
                        code << " := ";
                    else
                        code << " <= ";
                    
                    formatExpression(code, muxNode->getDriver(2), statement.inputs, false);
                    code << ";" << std::endl;
                    
                codeFormatting.indent(code, 3);
                code << "ELSE" << std::endl;

                    codeFormatting.indent(code, 4);
                    code << m_signalDeclaration.signalNames[nodePort];
                    if (variableList.find(nodePort) != variableList.end())
                        code << " := ";
                    else
                        code << " <= ";
                    
                    formatExpression(code, muxNode->getDriver(1), statement.inputs, false);
                    code << ";" << std::endl;
                
                codeFormatting.indent(code, 3);
                code << "END IF;" << std::endl;
            } else {
                code << m_signalDeclaration.signalNames[nodePort];
                if (variableList.find(nodePort) != variableList.end())
                    code << " := ";
                else
                    code << " <= ";
                
                formatExpression(code, nodePort, statement.inputs, true);
                code << ";" << std::endl;
            }
            statement.code = code.str();
            statements.push_back(std::move(statement));
        };
        
        for (auto s : m_signalDeclaration.outputSignals) 
            constructStatementsFor(s);
        
        for (auto s : m_signalDeclaration.localSignals) 
            constructStatementsFor(s);
        
        
        
        
        
        
        std::set<hlim::NodePort> signalsReady;
        for (auto s : m_signalDeclaration.inputSignals) 
            signalsReady.insert(s);

        /// @todo: Will deadlock for circular dependency
        while (!statements.empty()) {
            for (auto i : utils::Range(statements.size())) {
                
                auto &statement = statements[i];
                
                bool ready = true;
                for (auto s : statement.inputs)
                    if (signalsReady.find(s) == signalsReady.end()) {
                        ready = false;
                        break;
                    }
                
                if (ready) {
                    file << statement.code;

                    for (auto s : statement.outputs)
                        signalsReady.insert(s);

                    statements[i] = std::move(statements.back());
                    statements.pop_back();
                    
                    break;
                }
            }
        }
    }
    
    
    codeFormatting.indent(file, 1);    
    file << "END PROCESS;" << std::endl << std::endl;
    
    codeFormatting.indent(file, 1);
    file << "register_" << m_name << " : PROCESS(clk)" << std::endl;
        
    codeFormatting.indent(file, 1);
    file << "BEGIN" << std::endl;
    
    codeFormatting.indent(file, 2);
    file << "IF (rising_edge(clk)) THEN" << std::endl;
    
    for (auto node : m_nodeGroup->getNodes()) {
        hlim::Node_Register *regNode = dynamic_cast<hlim::Node_Register *>(node);
        if (regNode != nullptr) {
            
            hlim::NodePort output = {.node = regNode, .port = 0};
            hlim::NodePort dataInput = regNode->getDriver(hlim::Node_Register::DATA);
            
            codeFormatting.indent(file, 3);
            file << m_signalDeclaration.signalNames[output] << " <= " << m_signalDeclaration.signalNames[dataInput] << ";" << std::endl;
        }
    }
    codeFormatting.indent(file, 2);
    file << "END IF;" << std::endl;
    
    codeFormatting.indent(file, 1);    
    file << "END PROCESS;" << std::endl << std::endl;
}



Entity::Entity(Root &root) : m_root(root)
{
    m_namespace.setup(&m_root.getNamespace(), m_root.getCodeFormatting());
}


void Entity::buildFrom(hlim::NodeGroup *nodeGroup)
{
    MHDL_ASSERT(nodeGroup->getGroupType() == hlim::NodeGroup::GRP_ENTITY);
    m_nodeGroup = nodeGroup;
    
    m_name = m_root.getNamespace().getGlobalsName(nodeGroup->getName());
    
    for (auto &childGroup : nodeGroup->getChildren()) {
        
        switch (childGroup->getGroupType()) {
            case hlim::NodeGroup::GRP_ENTITY:
                m_subEntities.push_back(&m_root.createEntity());
                m_subEntities.back()->buildFrom(childGroup.get());
            break;
            case hlim::NodeGroup::GRP_AREA:
                for (auto &subChildGroup : childGroup->getChildren()) {
                    switch (subChildGroup->getGroupType()) {
                        case hlim::NodeGroup::GRP_ENTITY:
                            m_subEntities.push_back(&m_root.createEntity());
                            m_subEntities.back()->buildFrom(subChildGroup.get());
                        break;
                        default:
                            MHDL_ASSERT_HINT(false, "Unhandled case!");
                    }
                }
                
                m_processes.push_back(Process(*this, childGroup.get()));
                m_processes.back().extractExplicitSignals();
            break;
            default:
                MHDL_ASSERT_HINT(false, "Unhandled case!");
        }
    }
    
    // Prioritize signal name allocation

    for (auto &process : m_processes)
        process.allocateExternalIOSignals();
    
    for (auto &process : m_processes)
        process.allocateIntraEntitySignals();
    
    for (auto &process : m_processes)
        process.allocateRegisterSignals();

    for (auto &process : m_processes)
        process.allocateChildEntitySignals();

    for (auto &process : m_processes)
        process.allocateLocalSignals();
}

void Entity::print() 
{
    std::cout << "Entity: " << m_name << std::endl;
    std::cout << "   Inputs: " << std::endl;
    for (auto &s : m_signalDeclaration.inputSignals)
        std::cout << "        " << m_signalDeclaration.signalNames[s] << std::endl;
    std::cout << "   Outputs: " << std::endl;
    for (auto &s : m_signalDeclaration.outputSignals)
        std::cout << "        " << m_signalDeclaration.signalNames[s] << std::endl;
    std::cout << "   Local signals: " << std::endl;
    for (auto &s : m_signalDeclaration.localSignals)
        std::cout << "        " << m_signalDeclaration.signalNames[s] << std::endl;
    std::cout << "   Sub entities: " << std::endl;
    for (auto &s : m_subEntities)
        std::cout << "        " << s->m_name << std::endl;
    std::cout << "   Processes: " << std::endl;
    for (auto &s : m_processes)
        std::cout << "        " << s.getName() << std::endl;
}

void Entity::write(std::filesystem::path destination)
{
    auto &codeFormatting = *m_root.getCodeFormatting();
    
    //std::filesystem::path filePath = destination / codeFormatting.getFilename(m_nodeGroup);
    std::filesystem::path filePath = destination / (m_name + ".vhdl");
    std::filesystem::create_directories(filePath.parent_path());

    std::cout << "Exporting " << m_nodeGroup->getName() << " to " << filePath << std::endl;

    std::fstream file(filePath.string().c_str(), std::fstream::out);
    file << codeFormatting.getFileHeader();
    
    file << "LIBRARY ieee;" << std::endl << "USE ieee.std_logic_1164.ALL;" << std::endl << "USE ieee.numeric_std.all;" << std::endl << std::endl;
    
    
    file << "ENTITY " << m_name << " IS " << std::endl;
    codeFormatting.indent(file, 1);
    file << "PORT(" << std::endl;
    
    {
        std::vector<std::string> portList;
    
        for (const auto &signal : m_signalDeclaration.inputSignals) {
            std::stringstream line;
            line << m_signalDeclaration.signalNames[signal] << " : IN ";
            formatConnectionType(line, signal.node->getOutputConnectionType(signal.port));
            portList.push_back(line.str());
        }
        for (const auto &signal : m_signalDeclaration.outputSignals) {
            std::stringstream line;
            line << m_signalDeclaration.signalNames[signal] << " : OUT ";
            formatConnectionType(line, signal.node->getOutputConnectionType(signal.port));
            portList.push_back(line.str());
        }
        
        for (auto i : utils::Range(portList.size())) {
            codeFormatting.indent(file, 2);        
            file << portList[i];
            if (i+1 < portList.size())
                file << ";";
            file << std::endl;
        }
    }
    
    codeFormatting.indent(file, 1);
    file << ");" << std::endl;
    file << "END " << m_name << ";" << std::endl << std::endl;    

    file << "ARCHITECTURE impl OF " << m_name << " IS " << std::endl;
    for (const auto &signal : m_signalDeclaration.localSignals) {
        codeFormatting.indent(file, 1);
        file << "SIGNAL " << m_signalDeclaration.signalNames[signal] << " : ";
        formatConnectionType(file, signal.node->getOutputConnectionType(signal.port));
        file << "; "<< std::endl;
    }        
    
    file << "BEGIN" << std::endl;
    
    for (auto subEntity : m_subEntities) {
        codeFormatting.indent(file, 1);
        file << "inst_" << subEntity->m_name << " : entity work." << subEntity->m_name << "(impl) port map (" << std::endl;
        
        std::vector<std::string> portmapList;
        for (auto &s : subEntity->m_signalDeclaration.inputSignals) {
            std::stringstream line;
            line << subEntity->m_signalDeclaration.signalNames[s] << " => ";
            line << m_signalDeclaration.signalNames[s];
            portmapList.push_back(line.str());
        }
        for (auto &s : subEntity->m_signalDeclaration.outputSignals) {
            std::stringstream line;
            line << subEntity->m_signalDeclaration.signalNames[s] << " => ";
            line << m_signalDeclaration.signalNames[s];
            portmapList.push_back(line.str());
        }
        
        for (auto i : utils::Range(portmapList.size())) {
            codeFormatting.indent(file, 2);
            file << portmapList[i];
            if (i+1 < portmapList.size())
                file << ",";
            file << std::endl;
        }
        
        
        codeFormatting.indent(file, 1);
        file << ");" << std::endl;
    }
    
    for (auto &process : m_processes)
        process.write(file);
    
    file << "END impl;" << std::endl;
}


Root::Root(CodeFormatting *codeFormatting) : m_codeFormatting(codeFormatting)
{
    m_namespace.setup(nullptr, m_codeFormatting);
}

Entity &Root::createEntity()
{
    m_entities.push_back(Entity(*this));
    return m_entities.back();
}

void Root::print()
{
    for (auto &c : m_entities)
        c.print();
}

void Root::write(std::filesystem::path destination)
{
    for (auto &c : m_entities)
        c.write(destination);
}

}

