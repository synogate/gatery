#include "Process.h"

#include "BasicBlock.h"
#include "AST.h"

#include "../../utils/Range.h"
#include "../../hlim/Clock.h"

#include "../../hlim/coreNodes/Node_Arithmetic.h"
#include "../../hlim/coreNodes/Node_Compare.h"
#include "../../hlim/coreNodes/Node_Constant.h"
#include "../../hlim/coreNodes/Node_Logic.h"
#include "../../hlim/coreNodes/Node_Multiplexer.h"
#include "../../hlim/coreNodes/Node_PriorityConditional.h"
#include "../../hlim/coreNodes/Node_Signal.h"
#include "../../hlim/coreNodes/Node_Register.h"
#include "../../hlim/coreNodes/Node_Rewire.h"

namespace hcl::core::vhdl {

Process::Process(BasicBlock *parent) : BaseGrouping(parent->getAST(), parent, &parent->getNamespaceScope())
{

}

void Process::buildFromNodes(const std::vector<hlim::BaseNode*> &nodes)
{
    m_nodes = nodes;
    for (auto node : m_nodes)
        m_ast.getMapping().assignNodeToScope(node, this);
}

void Process::extractSignals()
{
    for (auto node : m_nodes) {
        // Check IO
        for (auto i : utils::Range(node->getNumInputPorts())) {
            auto driver = node->getDriver(i);
            if (driver.node != nullptr) {
                if (isProducedExternally(driver))
                    m_inputs.insert(driver);
            }
        }
        for (auto i : utils::Range(node->getNumOutputPorts())) {
            hlim::NodePort driver = {
                .node = node,
                .port = i
            };
            
            if (isConsumedExternally(driver))
                m_outputs.insert(driver);
        }
        // Handle clocks
        for (auto i : utils::Range(node->getClocks().size())) {
            if (node->getClocks()[i] != nullptr)
                m_inputClocks.insert(node->getClocks()[i]);
        }
        
        
#if 0
        // Named signals are explicit
        if (dynamic_cast<hlim::Node_Signal*>(node) != nullptr && !node->getName().empty()) {
            hlim::NodePort driver = {.node = node, .port = 0};
            if (m_outputs.find(driver) == m_outputs.end())
                m_localSignals.insert(driver);
        }
#endif
        // Check for multiple use
        for (auto i : utils::Range(node->getNumOutputPorts())) {
            if (node->getDirectlyDriven(i).size() > 1) {
                hlim::NodePort driver{.node = node, .port = i};
                if (m_outputs.find(driver) == m_outputs.end())
                    m_localSignals.insert(driver);
            }
        }
        
        
        // check for multiplexer
        hlim::Node_Multiplexer *muxNode = dynamic_cast<hlim::Node_Multiplexer *>(node);
        if (muxNode != nullptr) {
            hlim::NodePort driver{.node = muxNode, .port = 0};
            if (m_outputs.find(driver) == m_outputs.end())
                m_localSignals.insert(driver);
        }
        
        // check for prio conditional
        hlim::Node_PriorityConditional *prioCon = dynamic_cast<hlim::Node_PriorityConditional *>(node);
        if (prioCon != nullptr) {
            hlim::NodePort driver{.node = prioCon, .port = 0};
            if (!m_outputs.contains(driver))
                m_localSignals.insert(driver);
        }
        
        // check for rewire nodes with slices, which require explicit input signals
        hlim::Node_Rewire *rewireNode = dynamic_cast<hlim::Node_Rewire*>(node);
        if (rewireNode != nullptr) {
            for (const auto &op : rewireNode->getOp().ranges) {
                if (op.source == hlim::Node_Rewire::OutputRange::INPUT) {
                    auto driver = rewireNode->getDriver(op.inputIdx);
                    if (driver.node != nullptr)
                        if (op.inputOffset != 0 || op.subwidth != driver.node->getOutputConnectionType(driver.port).width)
                            if (!m_outputs.contains(driver) && !m_inputs.contains(driver))
                                m_localSignals.insert(driver);
                }
            }
        }
    }
    verifySignalsDisjoint();    
}


CombinatoryProcess::CombinatoryProcess(BasicBlock *parent, const std::string &desiredName) : Process(parent) 
{ 
    m_name = m_parent->getNamespaceScope().allocateProcessName(desiredName, false);
}


void CombinatoryProcess::allocateNames()
{
    for (auto &local : m_localSignals)
        m_namespaceScope.allocateName(local, findNearestDesiredName(local), CodeFormatting::SIG_LOCAL_VARIABLE);
}


void CombinatoryProcess::formatExpression(std::ostream &stream, std::ostream &comments, const hlim::NodePort &nodePort, std::set<hlim::NodePort> &dependentInputs, bool forceUnfold) 
{
    if (nodePort.node == nullptr) {
        stream << "(others => 'X')";
        return;
    }
    
    if (!nodePort.node->getComment().empty())
        comments << nodePort.node->getComment() << std::endl;
    
    if (!forceUnfold) {
        if (m_inputs.find(nodePort) != m_inputs.end() || 
            m_outputs.find(nodePort) != m_outputs.end() || 
            m_localSignals.find(nodePort) != m_localSignals.end()) {
            stream << m_namespaceScope.getName(nodePort);
            dependentInputs.insert(nodePort);
            return;
        }
    }
    
    HCL_ASSERT(dynamic_cast<const hlim::Node_Register*>(nodePort.node) == nullptr);
    
    const hlim::Node_Signal *signalNode = dynamic_cast<const hlim::Node_Signal *>(nodePort.node);
    if (signalNode != nullptr) {
        formatExpression(stream, comments, signalNode->getDriver(0), dependentInputs);
        return;            
    }
    
    const hlim::Node_Arithmetic *arithmeticNode = dynamic_cast<const hlim::Node_Arithmetic*>(nodePort.node);
    if (arithmeticNode != nullptr) { 
#if 0
        stream << "STD_LOGIC_VECTOR(UNSIGNED(";
        formatExpression(stream, comments, arithmeticNode->getDriver(0), dependentInputs);
        switch (arithmeticNode->getOp()) {
            case hlim::Node_Arithmetic::ADD: stream << ") + UNSIGNED("; break;
            case hlim::Node_Arithmetic::SUB: stream << ") - UNSIGNED("; break;
            case hlim::Node_Arithmetic::MUL: stream << ") * UNSIGNED("; break;
            case hlim::Node_Arithmetic::DIV: stream << ") / UNSIGNED("; break;
            case hlim::Node_Arithmetic::REM: stream << ") MOD UNSIGNED("; break;
            default:
                HCL_ASSERT_HINT(false, "Unhandled operation!");
        };
        formatExpression(stream, comments, arithmeticNode->getDriver(1), dependentInputs);
        stream << "))";
#else
        stream << "(";
        formatExpression(stream, comments, arithmeticNode->getDriver(0), dependentInputs);
        switch (arithmeticNode->getOp()) {
            case hlim::Node_Arithmetic::ADD: stream << " + "; break;
            case hlim::Node_Arithmetic::SUB: stream << " - "; break;
            case hlim::Node_Arithmetic::MUL: stream << " * "; break;
            case hlim::Node_Arithmetic::DIV: stream << " / "; break;
            case hlim::Node_Arithmetic::REM: stream << " MOD "; break;
            default:
                HCL_ASSERT_HINT(false, "Unhandled operation!");
        };
        formatExpression(stream, comments, arithmeticNode->getDriver(1), dependentInputs);
        stream << ")";
#endif
        return; 
    }
    
    const hlim::Node_Logic *logicNode = dynamic_cast<const hlim::Node_Logic*>(nodePort.node);
    if (logicNode != nullptr) { 
        stream << "(";
        if (logicNode->getOp() == hlim::Node_Logic::NOT) {
            stream << " not "; formatExpression(stream, comments, logicNode->getDriver(0), dependentInputs);
        } else {
            formatExpression(stream, comments, logicNode->getDriver(0), dependentInputs); 
            switch (logicNode->getOp()) {
                case hlim::Node_Logic::AND: stream << " and "; break;
                case hlim::Node_Logic::NAND: stream << " nand "; break;
                case hlim::Node_Logic::OR: stream << " or ";  break;
                case hlim::Node_Logic::NOR: stream << " nor "; break;
                case hlim::Node_Logic::XOR: stream << " xor "; break;
                case hlim::Node_Logic::EQ: stream << " xnor "; break;
                default:
                    HCL_ASSERT_HINT(false, "Unhandled operation!");
            };
            formatExpression(stream, comments, logicNode->getDriver(1), dependentInputs);
        }
        stream << ")";
        return; 
    }

    const hlim::Node_Compare *compareNode = dynamic_cast<const hlim::Node_Compare*>(nodePort.node);
    if (compareNode != nullptr) { 
        stream << "(";
        formatExpression(stream, comments, compareNode->getDriver(0), dependentInputs); 
        switch (compareNode->getOp()) {
            case hlim::Node_Compare::EQ: stream << " = "; break;
            case hlim::Node_Compare::NEQ: stream << " /= "; break;
            case hlim::Node_Compare::LT: stream << " < ";  break;
            case hlim::Node_Compare::GT: stream << " > "; break;
            case hlim::Node_Compare::LEQ: stream << " <= "; break;
            case hlim::Node_Compare::GEQ: stream << " >= "; break;
            default:
                HCL_ASSERT_HINT(false, "Unhandled operation!");
        };
        formatExpression(stream, comments, compareNode->getDriver(1), dependentInputs);
        stream << ")";
        return; 
    }

    const hlim::Node_Rewire *rewireNode = dynamic_cast<const hlim::Node_Rewire*>(nodePort.node);
    if (rewireNode != nullptr) {
        
        size_t bitExtractIdx;
        if (rewireNode->getOp().isBitExtract(bitExtractIdx)) {
            formatExpression(stream, comments, rewireNode->getDriver(0), dependentInputs);
            
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
                    case hlim::Node_Rewire::OutputRange::INPUT: {
                        auto driver = rewireNode->getDriver(range.inputIdx);
                        formatExpression(stream, comments, driver, dependentInputs);
                        if (driver.node != nullptr)
                            if (range.inputOffset != 0 || range.subwidth != driver.node->getOutputConnectionType(driver.port).width)
                                stream << "(" << range.inputOffset + range.subwidth - 1 << " downto " << range.inputOffset << ")";
                    } break;
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
        for (auto idx : utils::Range(constNode->getValue().bitVec.size())) {
            bool b = constNode->getValue().bitVec[constNode->getValue().bitVec.size()-1-idx];
            stream << (b ? '1' : '0');
        }
        stream << sep;
        return;
    }

    HCL_ASSERT_HINT(false, "Unhandled node type!");
}

void CombinatoryProcess::writeVHDL(std::ostream &stream, unsigned indentation)
{
    CodeFormatting &cf = m_ast.getCodeFormatting();

    cf.indent(stream, indentation);
    stream << m_name << " : PROCESS(all)" << std::endl;

    for (const auto &signal : m_localSignals) {
        cf.indent(stream, indentation+1);
        stream << "VARIABLE " << m_namespaceScope.getName(signal) << " : ";
        cf.formatConnectionType(stream, signal.node->getOutputConnectionType(signal.port));
        stream << "; "<< std::endl;
    }        

    cf.indent(stream, indentation);
    stream << "BEGIN" << std::endl;

    {
        struct Statement {
            std::set<hlim::NodePort> inputs;
            std::set<hlim::NodePort> outputs;
            std::string code;
            std::string comment;
            size_t weakOrderIdx;
        };
        
        std::vector<Statement> statements;
        
        auto constructStatementsFor = [&](hlim::NodePort nodePort) {
            std::stringstream code;
            cf.indent(code, indentation+1);
            
            std::stringstream comment;
            Statement statement;
            statement.weakOrderIdx = 0; ///@todo 
            statement.outputs.insert(nodePort);
            
            hlim::Node_Multiplexer *muxNode = dynamic_cast<hlim::Node_Multiplexer *>(nodePort.node);
            hlim::Node_PriorityConditional *prioCon = dynamic_cast<hlim::Node_PriorityConditional *>(nodePort.node);
            
            auto emitAssignment = [&]{
                code << m_namespaceScope.getName(nodePort);
                if (m_localSignals.find(nodePort) != m_localSignals.end())
                    code << " := ";
                else
                    code << " <= ";
            };
            
            if (muxNode != nullptr) {
                if (muxNode->getNumInputPorts() == 3) {
                    code << "IF ";
                    formatExpression(code, comment, muxNode->getDriver(0), statement.inputs, false);
                    code << " = '1' THEN"<< std::endl;
                    
                        cf.indent(code, indentation+2);
                        emitAssignment();
                        
                        formatExpression(code, comment, muxNode->getDriver(2), statement.inputs, false);
                        code << ";" << std::endl;
                        
                    cf.indent(code, indentation+1);
                    code << "ELSE" << std::endl;

                        cf.indent(code, indentation+2);
                        emitAssignment();
                        
                        formatExpression(code, comment, muxNode->getDriver(1), statement.inputs, false);
                        code << ";" << std::endl;
                    
                    cf.indent(code, indentation+1);
                    code << "END IF;" << std::endl;
                } else {
                    code << "CASE ";
                    formatExpression(code, comment, muxNode->getDriver(0), statement.inputs, false);
                    code << " IS"<< std::endl;
                    
                    for (auto i : utils::Range<size_t>(1, muxNode->getNumInputPorts())) {
                        cf.indent(code, indentation+2);
                        code << "WHEN \""; 
                        size_t inputIdx = i-1;
                        for (auto bitIdx : utils::Range(muxNode->getDriver(0).node->getOutputConnectionType(0).width)) {
                            bool b = inputIdx & (1 << (muxNode->getDriver(0).node->getOutputConnectionType(0).width - 1 - bitIdx));
                            code << (b ? '1' : '0');
                        }
                        code << "\" => ";
                        emitAssignment();
                        
                        formatExpression(code, comment, muxNode->getDriver(i), statement.inputs, false);
                        code << ";" << std::endl;
                    }
                    cf.indent(code, indentation+2);
                    code << "WHEN OTHERS => ";
                    emitAssignment();
                    
                    code << "\"";
                    for (auto bitIdx : utils::Range(muxNode->getDriver(1).node->getOutputConnectionType(0).width)) {
                        code << "X";
                    }
                    code << "\";" << std::endl;
                    
                        
                    cf.indent(code, indentation+1);
                    code << "END CASE;" << std::endl;
                }
                
                if (!nodePort.node->getComment().empty())
                    comment << nodePort.node->getComment() << std::endl;
            } else 
            if (prioCon != nullptr) {
                if (prioCon->getNumChoices() == 0) {
                    emitAssignment();
                    
                    formatExpression(code, comment, prioCon->getDriver(hlim::Node_PriorityConditional::inputPortDefault()), statement.inputs, false);
                    code << ";" << std::endl;
                } else {
                    for (auto choice : utils::Range(prioCon->getNumChoices())) {
                        if (choice == 0)
                            code << "IF ";
                        else {
                            cf.indent(code, indentation+1);
                            code << "ELSIF ";
                        }
                        formatExpression(code, comment, prioCon->getDriver(hlim::Node_PriorityConditional::inputPortChoiceCondition(choice)), statement.inputs, false);
                        code << " = '1' THEN"<< std::endl;
                        
                            cf.indent(code, indentation+2);
                            emitAssignment();
                            
                            formatExpression(code, comment, prioCon->getDriver(hlim::Node_PriorityConditional::inputPortChoiceValue(choice)), statement.inputs, false);
                            code << ";" << std::endl;
                    }
                            
                    cf.indent(code, indentation+1);
                    code << "ELSE" << std::endl;

                        cf.indent(code, indentation+2);
                        emitAssignment();
                        
                        formatExpression(code, comment, prioCon->getDriver(hlim::Node_PriorityConditional::inputPortDefault()), statement.inputs, false);
                        code << ";" << std::endl;
                    
                    cf.indent(code, indentation+1);
                    code << "END IF;" << std::endl;
                }
                if (!nodePort.node->getComment().empty())
                    comment << nodePort.node->getComment() << std::endl;
            } else {
                emitAssignment();
                
                formatExpression(code, comment, nodePort, statement.inputs, true);
                code << ";" << std::endl;
            }
            statement.code = code.str();
            statement.comment = comment.str();
            statements.push_back(std::move(statement));
        };
        
        for (auto s : m_outputs) 
            constructStatementsFor(s);
        
        for (auto s : m_localSignals) 
            constructStatementsFor(s);
        
        
        std::set<hlim::NodePort> signalsReady;
        for (auto s : m_inputs) 
            signalsReady.insert(s);
        
        /// @todo: Will deadlock for circular dependency
        while (!statements.empty()) {
            size_t bestStatement = ~0ull;
            for (auto i : utils::Range(statements.size())) {
                
                auto &statement = statements[i];
                
                bool ready = true;
                for (auto s : statement.inputs)
                    if (signalsReady.find(s) == signalsReady.end()) {
                        ready = false;
                        break;
                    }
                
                if (ready) {
                    if (bestStatement == ~0ull)
                        bestStatement = i;
                    else {
                        if (statement.weakOrderIdx < statements[bestStatement].weakOrderIdx)
                            bestStatement = i;
                    }
                }
            }
            
            HCL_ASSERT_HINT(bestStatement != ~0ull, "Cyclic dependency of signals detected!");
            
            cf.formatCodeComment(stream, indentation+1, statements[bestStatement].comment);
            stream << statements[bestStatement].code << std::flush;

            for (auto s : statements[bestStatement].outputs)
                signalsReady.insert(s);

            statements[bestStatement] = std::move(statements.back());
            statements.pop_back();
        }
    }

    cf.indent(stream, indentation);    
    stream << "END PROCESS;" << std::endl << std::endl;
}


RegisterProcess::RegisterProcess(BasicBlock *parent, const std::string &desiredName, const RegisterConfig &config) : Process(parent), m_config(config)
{ 
    m_name = m_parent->getNamespaceScope().allocateProcessName(desiredName, true);
}

void RegisterProcess::allocateNames()
{
    for (auto &local : m_localSignals)
        m_namespaceScope.allocateName(local, local.node->getName(), CodeFormatting::SIG_LOCAL_VARIABLE);
}


void RegisterProcess::writeVHDL(std::ostream &stream, unsigned indentation)
{
    verifySignalsDisjoint();
    
    CodeFormatting &cf = m_ast.getCodeFormatting();
    
    std::string clockName = m_namespaceScope.getName(m_config.clock);
    std::string resetName = clockName + m_config.clock->getResetName();

    cf.formatProcessComment(stream, indentation, m_name, m_comment);
    cf.indent(stream, indentation);

    if (m_config.hasResetSignal && m_config.clock->getResetType() == hlim::Clock::ResetType::ASYNCHRONOUS)
        stream << m_name << " : PROCESS(" << clockName << ", " << resetName << ")" << std::endl;
    else
        stream << m_name << " : PROCESS(" << clockName << ")" << std::endl;
    
    for (const auto &signal : m_localSignals) {
        cf.indent(stream, indentation+1);
        stream << "VARIABLE " << m_namespaceScope.getName(signal) << " : ";
        cf.formatConnectionType(stream, signal.node->getOutputConnectionType(signal.port));
        stream << "; "<< std::endl;
    }     
    
    cf.indent(stream, indentation);
    stream << "BEGIN" << std::endl;

    if (m_config.hasResetSignal && m_config.clock->getResetType() == hlim::Clock::ResetType::ASYNCHRONOUS) {
        cf.indent(stream, indentation+1);
        stream << "IF (" << m_config.clock->getResetName() << " = '" << (m_config.clock->getResetHighActive()?'1':'0') << "') THEN" << std::endl;

        for (auto node : m_nodes) {
            hlim::Node_Register *regNode = dynamic_cast<hlim::Node_Register *>(node);
            HCL_ASSERT(regNode != nullptr);
                
            hlim::NodePort output = {.node = regNode, .port = 0};
            hlim::NodePort resetValue = regNode->getDriver(hlim::Node_Register::RESET_VALUE);
            
            HCL_ASSERT(resetValue.node != nullptr);
            cf.indent(stream, indentation+2);
            stream << m_namespaceScope.getName(output) << " <= " << m_namespaceScope.getName(resetValue) << ";" << std::endl;
        }

        cf.indent(stream, indentation+1);
        stream << "ELSIF";
    } else {
        cf.indent(stream, indentation+1);
        stream << "IF";
    }
    
    switch (m_config.clock->getTriggerEvent()) {
        case hlim::Clock::TriggerEvent::RISING:
            stream << " (rising_edge(" << clockName << ")) THEN" << std::endl;
        break;
        case hlim::Clock::TriggerEvent::FALLING:
            stream << " (falling_edge(" << clockName << ")) THEN" << std::endl;
        break;
        case hlim::Clock::TriggerEvent::RISING_AND_FALLING:
            stream << " (" << clockName << "'event) THEN" << std::endl;
        break;
    }

    unsigned indentationOffset = 0;
    if (m_config.hasResetSignal && m_config.clock->getResetType() == hlim::Clock::ResetType::SYNCHRONOUS) {
        cf.indent(stream, indentation+2);
        stream << "IF (" << resetName << " = '" << (m_config.clock->getResetHighActive()?'1':'0') << "') THEN" << std::endl;
        
        for (auto node : m_nodes) {
            hlim::Node_Register *regNode = dynamic_cast<hlim::Node_Register *>(node);
            HCL_ASSERT(regNode != nullptr);
                
            hlim::NodePort output = {.node = regNode, .port = 0};
            hlim::NodePort resetValue = regNode->getDriver(hlim::Node_Register::RESET_VALUE);
            
            cf.indent(stream, indentation+3);
            stream << m_namespaceScope.getName(output) << " <= " << m_namespaceScope.getName(resetValue) << ";" << std::endl;            
        }
        
        cf.indent(stream, indentation+2);
        stream << "ELSE" << std::endl;
        indentationOffset++;
    }
    
    
    for (auto node : m_nodes) {
        hlim::Node_Register *regNode = dynamic_cast<hlim::Node_Register *>(node);
        HCL_ASSERT(regNode != nullptr);
            
        hlim::NodePort output = {.node = regNode, .port = 0};
        hlim::NodePort dataInput = regNode->getDriver(hlim::Node_Register::DATA);
        hlim::NodePort enableInput = regNode->getDriver(hlim::Node_Register::ENABLE);
        
        if (enableInput.node != nullptr) {
            cf.indent(stream, indentation+2+indentationOffset);
            stream << "IF (" << m_namespaceScope.getName(enableInput) << " = '1') THEN" << std::endl;
            
            cf.indent(stream, indentation+3+indentationOffset);
            stream << m_namespaceScope.getName(output) << " <= " << m_namespaceScope.getName(dataInput) << ";" << std::endl;
            
            cf.indent(stream, indentation+2+indentationOffset);
            stream << "END IF;" << std::endl;
        } else {
            cf.indent(stream, indentation+2+indentationOffset);
            stream << m_namespaceScope.getName(output) << " <= " << m_namespaceScope.getName(dataInput) << ";" << std::endl;
        }
    }
    
    if (indentationOffset) {
        cf.indent(stream, indentation+2);
        stream << "END IF;" << std::endl;
    }
    
    cf.indent(stream, indentation+1);
    stream << "END IF;" << std::endl;


    cf.indent(stream, indentation);    
    stream << "END PROCESS;" << std::endl << std::endl;
}

}
