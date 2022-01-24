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
#include "Process.h"

#include "BasicBlock.h"
#include "AST.h"

#include "../../utils/Range.h"
#include "../../hlim/Clock.h"

#include "../../hlim/coreNodes/Node_Arithmetic.h"
#include "../../hlim/coreNodes/Node_Clk2Signal.h"
#include "../../hlim/coreNodes/Node_Compare.h"
#include "../../hlim/coreNodes/Node_Constant.h"
#include "../../hlim/coreNodes/Node_Logic.h"
#include "../../hlim/coreNodes/Node_Multiplexer.h"
#include "../../hlim/coreNodes/Node_PriorityConditional.h"
#include "../../hlim/coreNodes/Node_Signal.h"
#include "../../hlim/coreNodes/Node_Register.h"
#include "../../hlim/coreNodes/Node_Rewire.h"
#include "../../hlim/coreNodes/Node_Pin.h"
#include "../../hlim/coreNodes/Node_ClkRst2Signal.h"
#include "../../hlim/supportNodes/Node_Attributes.h"
#include "../../hlim/supportNodes/Node_ExportOverride.h"
#include "../../hlim/supportNodes/Node_SignalTap.h"

#include <iostream>

namespace gtry::vhdl {

Process::Process(BasicBlock *parent) : BaseGrouping(parent->getAST(), parent, &parent->getNamespaceScope())
{
}

void Process::buildFromNodes(std::vector<hlim::BaseNode*> nodes)
{
    m_nodes = std::move(nodes);
    for (auto node : m_nodes)
        m_ast.getMapping().assignNodeToScope(node, this);
}

void Process::extractSignals()
{
    std::set<hlim::NodePort> potentialLocalSignals;
    std::set<hlim::NodePort> potentialConstants;

    // In the first pass, insert everything as local signals, then remove from that list everything that also ended up as input, output, or is a pin
    for (auto node : m_nodes) {
        // Check IO
        for (auto i : utils::Range(node->getNumInputPorts())) {

            // ignore reset values as they are hard coded anyways.
            if (dynamic_cast<hlim::Node_Register*>(node) && i == hlim::Node_Register::RESET_VALUE) continue;

            // Ignore simulation-only inputs
            if (dynamic_cast<const hlim::Node_ExportOverride*>(node) && i == hlim::Node_ExportOverride::SIM_INPUT) continue;

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

        if (auto *rstSig = dynamic_cast<hlim::Node_ClkRst2Signal*>(node)) {
            m_inputResets.insert(rstSig->getClocks()[0]->getResetPinSource());
        }

#if 1
        // Named signals are explicit
        if (dynamic_cast<hlim::Node_Signal*>(node) != nullptr && node->hasGivenName() && node->getOutputConnectionType(0).width > 0) {
            hlim::NodePort driver = {.node = node, .port = 0};
            potentialLocalSignals.insert(driver);
        }
#endif

        // Need an explicit signal between input-pins and rewire to handle cast
        if (dynamic_cast<hlim::Node_Signal*>(node) != nullptr && dynamic_cast<hlim::Node_Pin*>(node->getNonSignalDriver(0).node)) {
            bool feedsIntoRewire = false;
            for (auto driven : node->getDirectlyDriven(0))
                if (dynamic_cast<hlim::Node_Rewire*>(driven.node)) {
                    feedsIntoRewire = true;
                    break;
                }

            if (feedsIntoRewire) {
                hlim::NodePort driver = {.node = node, .port = 0};
                potentialLocalSignals.insert(driver);
            }
        }


#if 1
        // Named constants are explicit
        if (dynamic_cast<hlim::Node_Constant*>(node) != nullptr && node->hasGivenName()) {
            hlim::NodePort driver = {.node = node, .port = 0};
            potentialConstants.insert(driver);
        }
#endif

        // Check for multiple use
        for (auto i : utils::Range(node->getNumOutputPorts())) {
            if (node->getDirectlyDriven(i).size() > 1 && node->getOutputConnectionType(i).interpretation != hlim::ConnectionType::BOOL) {
                hlim::NodePort driver{.node = node, .port = i};
                potentialLocalSignals.insert(driver);
            }
        }

        // check for multiplexer
        hlim::Node_Multiplexer *muxNode = dynamic_cast<hlim::Node_Multiplexer *>(node);
        if (muxNode != nullptr) {
            hlim::NodePort driver{.node = muxNode, .port = 0};
            potentialLocalSignals.insert(driver);
        }

        // check for prio conditional
        hlim::Node_PriorityConditional *prioCon = dynamic_cast<hlim::Node_PriorityConditional *>(node);
        if (prioCon != nullptr) {
            hlim::NodePort driver{.node = prioCon, .port = 0};
            potentialLocalSignals.insert(driver);
        }

        // check for rewire nodes with slices, which require explicit input signals
        hlim::Node_Rewire *rewireNode = dynamic_cast<hlim::Node_Rewire*>(node);
        if (rewireNode != nullptr) {
            for (const auto &op : rewireNode->getOp().ranges) {
                if (op.source == hlim::Node_Rewire::OutputRange::INPUT) {
                    auto driver = rewireNode->getDriver(op.inputIdx);
                    if (driver.node != nullptr)
                        if (op.inputOffset != 0 || op.subwidth != getOutputWidth(driver))
                            potentialLocalSignals.insert(driver);
                }
            }
        }

        // check for io pins
        hlim::Node_Pin *pinNode = dynamic_cast<hlim::Node_Pin *>(node);
        if (pinNode != nullptr) {
            m_ioPins.insert(pinNode);
        }

    }

    for (auto driver : potentialLocalSignals)
        if (!m_outputs.contains(driver) && !m_inputs.contains(driver) && !dynamic_cast<hlim::Node_Pin*>(driver.node))
            m_localSignals.insert(driver);

    for (auto driver : potentialConstants)
        if (!m_outputs.contains(driver))
            m_constants.insert(driver);
        else
            std::cout << "Warning: Not turning constant into VHDL constant because it is directly wired to output!" << std::endl;


    verifySignalsDisjoint();
}


CombinatoryProcess::CombinatoryProcess(BasicBlock *parent, const std::string &desiredName) : Process(parent)
{
    m_name = m_parent->getNamespaceScope().allocateProcessName(desiredName, false);
}


void CombinatoryProcess::allocateNames()
{
    for (auto &constant : m_constants)
        m_namespaceScope.allocateName(constant, findNearestDesiredName(constant), chooseDataTypeFromOutput(constant), CodeFormatting::SIG_CONSTANT);

    for (auto &local : m_localSignals)
        m_namespaceScope.allocateName(local, findNearestDesiredName(local), chooseDataTypeFromOutput(local), CodeFormatting::SIG_LOCAL_VARIABLE);
}


void CombinatoryProcess::formatExpression(std::ostream &stream, size_t indentation, std::ostream &comments, const hlim::NodePort &nodePort, std::set<hlim::NodePort> &dependentInputs, VHDLDataType context, bool forceUnfold)
{
    if (nodePort.node == nullptr) {
        comments << "-- Warning: Unconnected node, using others=>X" << std::endl;
        stream << "(others => 'X')";
        return;
    }

    auto &cf = m_ast.getCodeFormatting();

    if (!nodePort.node->getComment().empty())
        comments << nodePort.node->getComment() << std::endl;

    if (!forceUnfold) {
        if (m_inputs.contains(nodePort) || m_outputs.contains(nodePort) || m_localSignals.contains(nodePort) || m_constants.contains(nodePort)) {
            const auto &decl = m_namespaceScope.get(nodePort);
            HCL_ASSERT(!decl.name.empty());
            switch (context) {
                case VHDLDataType::BOOL:
                    stream << decl.name << " = '1'";
                break;
                case VHDLDataType::STD_LOGIC:
                    if (hlim::outputIsBVec(nodePort))
                        stream << decl.name << "(0)";
                    else
                        stream << decl.name;
                break;
                case VHDLDataType::STD_LOGIC_VECTOR:
                case VHDLDataType::UNSIGNED:
                    if (decl.dataType != context) {
                        cf.formatDataType(stream, context);
                        stream << '(' << decl.name << ')';
                    } else
                        stream << decl.name;
                    HCL_ASSERT(hlim::outputIsBVec(nodePort));
                break;
                default:
                    HCL_ASSERT_HINT(false, "Unhandled case!");
            }
            dependentInputs.insert(nodePort);
            return;
        }
    }

    HCL_ASSERT(dynamic_cast<const hlim::Node_Register*>(nodePort.node) == nullptr);
    HCL_ASSERT(dynamic_cast<const hlim::Node_Multiplexer*>(nodePort.node) == nullptr);

    if (const auto *signalNode = dynamic_cast<const hlim::Node_Signal *>(nodePort.node)) {
        formatExpression(stream, indentation, comments, signalNode->getDriver(0), dependentInputs, context);
        return;
    }

    if (const auto *attribNode = dynamic_cast<const hlim::Node_Attributes *>(nodePort.node)) {
        formatExpression(stream, indentation, comments, attribNode->getDriver(0), dependentInputs, context);
        return;
    }

    if (const auto *expOverrideNode = dynamic_cast<const hlim::Node_ExportOverride*>(nodePort.node)) {
        formatExpression(stream, indentation, comments, expOverrideNode->getDriver(hlim::Node_ExportOverride::EXP_INPUT), dependentInputs, context);
        return;
    }

    if (const auto *rstSig = dynamic_cast<const hlim::Node_ClkRst2Signal*>(nodePort.node)) {
        HCL_ASSERT(context == VHDLDataType::BOOL || context == VHDLDataType::STD_LOGIC);
        stream << m_namespaceScope.getReset(rstSig->getClocks()[0]->getResetPinSource()).name;
        if (context == VHDLDataType::BOOL)
            stream << " = '1'";
            
        return;
    }

    // not ideal but works for now
    hlim::Node_Pin *ioPinNode = dynamic_cast<hlim::Node_Pin *>(nodePort.node);
    if (ioPinNode != nullptr) {
        const auto &decl = m_namespaceScope.get(ioPinNode);

        switch (context) {
            case VHDLDataType::BOOL:
                stream << decl.name << " = '1'";
            break;
            case VHDLDataType::STD_LOGIC:
                stream << decl.name;
                if (hlim::outputIsBVec(nodePort))
                    stream << "(0)";
            break;
            case VHDLDataType::STD_LOGIC_VECTOR:
            case VHDLDataType::UNSIGNED:
                if (decl.dataType != context) {
                    cf.formatDataType(stream, context);
                    stream << '(' << decl.name << ')';
                } else
                    stream << decl.name;

                HCL_ASSERT(hlim::outputIsBVec(nodePort));
            break;
            default:
                HCL_ASSERT_HINT(false, "Unhandled case!");
        }
        return;
    }

    const hlim::Node_Arithmetic *arithmeticNode = dynamic_cast<const hlim::Node_Arithmetic*>(nodePort.node);
    if (arithmeticNode != nullptr) {
        if (context == VHDLDataType::STD_LOGIC_VECTOR)
            stream << "STD_LOGIC_VECTOR(";
        else
            stream << '(';
        formatExpression(stream, indentation, comments, arithmeticNode->getDriver(0), dependentInputs, VHDLDataType::UNSIGNED);
        switch (arithmeticNode->getOp()) {
            case hlim::Node_Arithmetic::ADD: stream << " + "; break;
            case hlim::Node_Arithmetic::SUB: stream << " - "; break;
            case hlim::Node_Arithmetic::MUL: stream << " * "; break;
            case hlim::Node_Arithmetic::DIV: stream << " / "; break;
            case hlim::Node_Arithmetic::REM: stream << " MOD "; break;
            default:
                HCL_ASSERT_HINT(false, "Unhandled operation!");
        };
        formatExpression(stream, indentation, comments, arithmeticNode->getDriver(1), dependentInputs, VHDLDataType::UNSIGNED);
        stream << ')';
        return;
    }


    const hlim::Node_Logic *logicNode = dynamic_cast<const hlim::Node_Logic*>(nodePort.node);
    if (logicNode != nullptr) {
        stream << '(';
        if (logicNode->getOp() == hlim::Node_Logic::NOT) {
            stream << " not "; formatExpression(stream, indentation, comments, logicNode->getDriver(0), dependentInputs, context);
        } else {
            formatExpression(stream, indentation, comments, logicNode->getDriver(0), dependentInputs, context);
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
            formatExpression(stream, indentation, comments, logicNode->getDriver(1), dependentInputs, context);
        }
        stream << ')';
        return;
    }


    const hlim::Node_Compare *compareNode = dynamic_cast<const hlim::Node_Compare*>(nodePort.node);
    if (compareNode != nullptr) {
        if (context == VHDLDataType::STD_LOGIC)
            stream << "bool2stdlogic(";
        else
            stream << "(";

        VHDLDataType subContext = VHDLDataType::UNSIGNED;
        if (compareNode->getDriverConnType(0).interpretation == hlim::ConnectionType::BOOL)
            subContext = VHDLDataType::STD_LOGIC;

        formatExpression(stream, indentation, comments, compareNode->getDriver(0), dependentInputs, subContext);
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
        formatExpression(stream, indentation, comments, compareNode->getDriver(1), dependentInputs, subContext);
        stream << ")";
        return;
    }

    const hlim::Node_Rewire *rewireNode = dynamic_cast<const hlim::Node_Rewire*>(nodePort.node);
    if (rewireNode != nullptr) {
        HCL_ASSERT(rewireNode->getOutputConnectionType(0).width > 0);

        // Note: VHDL doesn't allow indexing the result of a type conversion (i.e. UNSIGNED(a)(7 downto 0) )
        // one has to convert the result of it (i.e. UNSIGNED(a(7 downto 0)) )

        // Also note that the result of a concatenation must not be explicitly cast but can implicitely be cast to STD_LOGIC_VECTOR or UNSIGNED

        size_t bitExtractIdx;
        if (rewireNode->getOp().isBitExtract(bitExtractIdx)) {
            if (hlim::outputIsBVec(rewireNode->getDriver(0))) {

                switch (context) {
                    case VHDLDataType::BOOL:
                        formatExpression(stream, indentation, comments, rewireNode->getDriver(0), dependentInputs, VHDLDataType::UNSIGNED);
                        stream << "(" << bitExtractIdx << ") = '1'";
                    break;
                    case VHDLDataType::STD_LOGIC:
                        formatExpression(stream, indentation, comments, rewireNode->getDriver(0), dependentInputs, VHDLDataType::UNSIGNED);
                        stream << "(" << bitExtractIdx << ")";
                    break;
                    case VHDLDataType::UNSIGNED:
                        formatExpression(stream, indentation, comments, rewireNode->getDriver(0), dependentInputs, VHDLDataType::UNSIGNED);
                        stream << "(" << bitExtractIdx << " downto " << bitExtractIdx << ")";
                    break;
                    case VHDLDataType::STD_LOGIC_VECTOR:
                        stream << "STD_LOGIC_VECTOR(";
                        formatExpression(stream, indentation, comments, rewireNode->getDriver(0), dependentInputs, VHDLDataType::UNSIGNED);
                        stream << "(" << bitExtractIdx << " downto " << bitExtractIdx << "))";
                    break;
                    default:
                        HCL_ASSERT_HINT(false, "Unhandled case!");
                }
            } else { // is bool->bvec type cast
                HCL_ASSERT(bitExtractIdx == 0);
                stream << "(0 => ";
                formatExpression(stream, indentation, comments, rewireNode->getDriver(0), dependentInputs, VHDLDataType::STD_LOGIC);
                stream << ")";
            }
        } else {
            const auto &op = rewireNode->getOp().ranges;

            bool mustCastToSLV = false;
            for (auto range : op) {
                if (range.source == hlim::Node_Rewire::OutputRange::INPUT) {
                    auto driver = rewireNode->getDriver(range.inputIdx);
                    if (hlim::outputIsBVec(driver)) {
                        mustCastToSLV = true;
                        break;
                    }
                }
            }

            if (context == VHDLDataType::STD_LOGIC_VECTOR && mustCastToSLV)
                stream << "STD_LOGIC_VECTOR(";
            else if (op.size() > 1 )
                stream << "("; // Must not cast since concatenation

            for (auto i : utils::Range(op.size())) {
                if (i > 0) {
                    stream << " & ";
                    if (i % 16 == 15) {
                        stream << '\n';
                        cf.indent(stream, (unsigned) indentation);
                    }
                }
                const auto &range = op[op.size()-1-i];
                switch (range.source) {
                    case hlim::Node_Rewire::OutputRange::INPUT: {
                        auto driver = rewireNode->getDriver(range.inputIdx);
                        auto subContext = hlim::outputIsBVec(driver)?VHDLDataType::UNSIGNED:VHDLDataType::STD_LOGIC;
                        formatExpression(stream, indentation, comments, driver, dependentInputs, subContext);
                        if (driver.node != nullptr)
                            if (range.inputOffset != 0 || range.subwidth != hlim::getOutputWidth(driver))
                                stream << "(" << range.inputOffset + range.subwidth - 1 << " downto " << range.inputOffset << ")";
                    } break;
                    case hlim::Node_Rewire::OutputRange::CONST_ZERO:
                        stream << '"';
                        for ([[maybe_unused]] auto j : utils::Range(range.subwidth))
                            stream << "0";
                        stream << '"';
                    break;
                    case hlim::Node_Rewire::OutputRange::CONST_ONE:
                        stream << '"';
                        for ([[maybe_unused]] auto j : utils::Range(range.subwidth))
                            stream << "1";
                        stream << '"';
                    break;
                    default:
                        stream << "UNHANDLED_REWIRE_OP";
                }
            }

            if (op.size() > 1)
                stream << ')';
            else
                if (context == VHDLDataType::STD_LOGIC_VECTOR)
                    stream << ')';
        }

        return;
    }

    if (const hlim::Node_Constant* constNode = dynamic_cast<const hlim::Node_Constant*>(nodePort.node))
    {
        formatConstant(stream, constNode, context);
        return;
    }

    if (const hlim::Node_Clk2Signal* clk2signal = dynamic_cast<const hlim::Node_Clk2Signal*>(nodePort.node)) {
        stream << m_namespaceScope.getClock(clk2signal->getClocks()[0]).name;
        return;
    }

    HCL_ASSERT_HINT(false, "Unhandled node type!");
}

void CombinatoryProcess::writeVHDL(std::ostream &stream, unsigned indentation)
{
    CodeFormatting &cf = m_ast.getCodeFormatting();

    cf.indent(stream, indentation);
    stream << m_name << " : PROCESS(all)" << std::endl;

    declareLocalSignals(stream, true, indentation);

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
            statement.weakOrderIdx = nodePort.node->getId(); // chronological order
            statement.outputs.insert(nodePort);

            hlim::Node_Multiplexer *muxNode = dynamic_cast<hlim::Node_Multiplexer *>(nodePort.node);
            hlim::Node_PriorityConditional *prioCon = dynamic_cast<hlim::Node_PriorityConditional *>(nodePort.node);

            VHDLDataType targetContext;

            bool isLocalSignal = m_localSignals.contains(nodePort);
            std::string assignmentPrefix;
            bool forceUnfold;
            if (auto *ioPin = dynamic_cast<hlim::Node_Pin*>(nodePort.node); ioPin && ioPin->isOutputPin()) { // todo: all in all this is bad
                const auto &decl = m_namespaceScope.get(ioPin);
                assignmentPrefix = decl.name;
                forceUnfold = false; // assigning to pin, can directly do with a signal/variable;
                nodePort = ioPin->getDriver(0);

                targetContext = decl.dataType;
            } else {
                const auto &decl = m_namespaceScope.get(nodePort);
                assignmentPrefix = decl.name;
                targetContext = decl.dataType;
                forceUnfold = true; // referring to target, must force unfolding
            }


            if (isLocalSignal)
                assignmentPrefix += " := ";
            else
                assignmentPrefix += " <= ";

            if (muxNode != nullptr) {
                if (muxNode->getNumInputPorts() == 3) {
                    code << "IF ";
                    formatExpression(code, indentation+2, comment, muxNode->getDriver(0), statement.inputs, VHDLDataType::BOOL, false);
                    code << " THEN"<< std::endl;

                        cf.indent(code, indentation+2);
                        code << assignmentPrefix;

                        formatExpression(code, indentation+3, comment, muxNode->getDriver(2), statement.inputs, targetContext, false);
                        code << ";" << std::endl;

                    cf.indent(code, indentation+1);
                    code << "ELSE" << std::endl;

                        cf.indent(code, indentation+2);
                        code << assignmentPrefix;

                        formatExpression(code, indentation+3, comment, muxNode->getDriver(1), statement.inputs, targetContext, false);
                        code << ";" << std::endl;

                    cf.indent(code, indentation+1);
                    code << "END IF;" << std::endl;
                } else {
                    code << "CASE ";
                    formatExpression(code, indentation+2, comment, muxNode->getDriver(0), statement.inputs, VHDLDataType::UNSIGNED, false);
                    code << " IS"<< std::endl;

                    for (auto i : utils::Range<size_t>(1, muxNode->getNumInputPorts())) {
                        cf.indent(code, indentation+2);
                        code << "WHEN \"";
                        size_t inputIdx = i-1;
                        auto driverWidth = hlim::getOutputWidth(muxNode->getDriver(0));
                        for (auto bitIdx : utils::Range(driverWidth)) {
                            bool b = inputIdx & (size_t(1) << (driverWidth - 1 - bitIdx));
                            code << (b ? '1' : '0');
                        }
                        code << "\" => ";
                        code << assignmentPrefix;

                        formatExpression(code, indentation+3, comment, muxNode->getDriver(i), statement.inputs, targetContext, false);
                        code << ";" << std::endl;
                    }
                    cf.indent(code, indentation+2);
                    code << "WHEN OTHERS => ";
                    code << assignmentPrefix;

                    if (targetContext == VHDLDataType::UNSIGNED || targetContext == VHDLDataType::STD_LOGIC_VECTOR) {
                        code << "\"";
                        for ([[maybe_unused]] auto bitIdx : utils::Range(hlim::getOutputWidth(muxNode->getDriver(1))))
                            code << "X";
                        code << "\";" << std::endl;
                    } else {
                        code << "'";
                        for ([[maybe_unused]] auto bitIdx : utils::Range(hlim::getOutputWidth(muxNode->getDriver(1))))
                            code << "X";
                        code << "';" << std::endl;
                    }


                    cf.indent(code, indentation+1);
                    code << "END CASE;" << std::endl;
                }

                if (!nodePort.node->getComment().empty())
                    comment << nodePort.node->getComment() << std::endl;
            } else
            if (prioCon != nullptr) {
                if (prioCon->getNumChoices() == 0) {
                    code << assignmentPrefix;

                    formatExpression(code, indentation+2, comment, prioCon->getDriver(hlim::Node_PriorityConditional::inputPortDefault()), statement.inputs, targetContext, false);
                    code << ";" << std::endl;
                } else {
                    for (auto choice : utils::Range(prioCon->getNumChoices())) {
                        if (choice == 0)
                            code << "IF ";
                        else {
                            cf.indent(code, indentation+1);
                            code << "ELSIF ";
                        }
                        formatExpression(code, indentation+2, comment, prioCon->getDriver(hlim::Node_PriorityConditional::inputPortChoiceCondition(choice)), statement.inputs, VHDLDataType::BOOL, false);
                        code << " THEN"<< std::endl;

                            cf.indent(code, indentation+2);
                            code << assignmentPrefix;

                            formatExpression(code, indentation+3, comment, prioCon->getDriver(hlim::Node_PriorityConditional::inputPortChoiceValue(choice)), statement.inputs, targetContext, false);
                            code << ";" << std::endl;
                    }

                    cf.indent(code, indentation+1);
                    code << "ELSE" << std::endl;

                        cf.indent(code, indentation+2);
                        code << assignmentPrefix;

                        formatExpression(code, indentation+3, comment, prioCon->getDriver(hlim::Node_PriorityConditional::inputPortDefault()), statement.inputs, targetContext, false);
                        code << ";" << std::endl;

                    cf.indent(code, indentation+1);
                    code << "END IF;" << std::endl;
                }
                if (!nodePort.node->getComment().empty())
                    comment << nodePort.node->getComment() << std::endl;
            } else {
                code << assignmentPrefix;

                formatExpression(code, indentation+2, comment, nodePort, statement.inputs, targetContext, forceUnfold);
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

        for (auto s : m_constants)
            signalsReady.insert(s);

        for (auto s : m_ioPins) {
            if (s->isInputPin())
                signalsReady.insert({.node=s, .port=0});

            if (s->isOutputPin() && s->getNonSignalDriver(0).node != nullptr)
                constructStatementsFor({.node=s, .port=0});
        }

        for (auto &n : m_nodes) {
            if (auto *sig_tap = dynamic_cast<hlim::Node_SignalTap*>(n)) {

                HCL_ASSERT(sig_tap->getLevel() == hlim::Node_SignalTap::LVL_ASSERT || sig_tap->getLevel() == hlim::Node_SignalTap::LVL_WARN);
                HCL_ASSERT(sig_tap->getTrigger() == hlim::Node_SignalTap::TRIG_FIRST_INPUT_HIGH || sig_tap->getTrigger() == hlim::Node_SignalTap::TRIG_FIRST_INPUT_LOW);

                std::stringstream code;
                cf.indent(code, indentation+1);

                std::stringstream comment;
                Statement statement;
                statement.weakOrderIdx = n->getId(); // chronological order

                code << "ASSERT ";
                if (sig_tap->getTrigger() == hlim::Node_SignalTap::TRIG_FIRST_INPUT_HIGH)
                    code << "not (";

                formatExpression(code, indentation+2, comment, sig_tap->getDriver(0), statement.inputs, VHDLDataType::BOOL, false);
                
                if (sig_tap->getTrigger() == hlim::Node_SignalTap::TRIG_FIRST_INPUT_HIGH)                
                    code << ")";

                switch (sig_tap->getLevel()) {
                    case hlim::Node_SignalTap::LVL_ASSERT:
                        code << " severity error"; break;
                    case hlim::Node_SignalTap::LVL_WARN:
                        code << " severity warning"; break;
                    default: break;
                }

                code << ";" << std::endl;

                statement.code = code.str();
                statement.comment = comment.str();
                statements.push_back(std::move(statement));
            }
        }


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

            if (bestStatement+1 != statements.size())
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


void RegisterProcess::extractSignals()
{
    for (auto node : m_nodes) {
        // Handle clocks
        if (auto *reg = dynamic_cast<hlim::Node_Register*>(node)) {
            m_inputClocks.insert(node->getClocks()[0]->getClockPinSource());

			auto resetValue = reg->getDriver((unsigned)hlim::Node_Register::Input::RESET_VALUE);
			if (resetValue.node != nullptr)
				if (reg->getClocks()[0]->getRegAttribs().resetType != hlim::RegisterAttributes::ResetType::NONE)
                    m_inputResets.insert(node->getClocks()[0]->getResetPinSource());
        } else {
            HCL_ASSERT(node->getClocks().empty());
        }
    }
    Process::extractSignals();
}


void RegisterProcess::allocateNames()
{
    for (auto &constant : m_constants)
        m_namespaceScope.allocateName(constant, findNearestDesiredName(constant), chooseDataTypeFromOutput(constant), CodeFormatting::SIG_CONSTANT);

    for (auto &local : m_localSignals)
        m_namespaceScope.allocateName(local, local.node->getName(), chooseDataTypeFromOutput(local), CodeFormatting::SIG_LOCAL_VARIABLE);
}


void RegisterProcess::writeVHDL(std::ostream &stream, unsigned indentation)
{
    verifySignalsDisjoint();

    CodeFormatting &cf = m_ast.getCodeFormatting();

    std::string clockName = m_namespaceScope.getClock(m_config.clock).name;
    std::string resetName;
    if (m_config.reset != nullptr)
        resetName = m_namespaceScope.getReset(m_config.reset).name;

    cf.formatProcessComment(stream, indentation, m_name, m_comment);
    cf.indent(stream, indentation);

    if (m_config.reset != nullptr && m_config.resetType == hlim::RegisterAttributes::ResetType::ASYNCHRONOUS)
        stream << m_name << " : PROCESS(" << clockName << ", " << resetName << ")" << std::endl;
    else
        stream << m_name << " : PROCESS(" << clockName << ")" << std::endl;

    declareLocalSignals(stream, true, indentation);

    cf.indent(stream, indentation);
    stream << "BEGIN" << std::endl;

    if (m_config.reset != nullptr && m_config.resetType == hlim::RegisterAttributes::ResetType::ASYNCHRONOUS) {
        cf.indent(stream, indentation+1);
        const char resetValue = m_config.clock->getRegAttribs().resetActive == hlim::RegisterAttributes::Active::HIGH ? '1' : '0';
        stream << "IF (" << resetName << " = '" << resetValue << "') THEN" << std::endl;

        for (auto node : m_nodes) {
            hlim::Node_Register *regNode = dynamic_cast<hlim::Node_Register *>(node);
            HCL_ASSERT(regNode != nullptr);

            hlim::NodePort output = {.node = regNode, .port = 0};
            hlim::NodePort resetValue = regNode->getNonSignalDriver(hlim::Node_Register::RESET_VALUE);

            HCL_ASSERT(resetValue.node != nullptr);
            auto *constReset = dynamic_cast<hlim::Node_Constant*>(resetValue.node);
            HCL_DESIGNCHECK_HINT(constReset, "Resets of registers must be constants uppon export!");

            const auto &outputDecl = m_namespaceScope.get(output);

            cf.indent(stream, indentation+2);
            stream << outputDecl.name << " <= ";
            formatConstant(stream, constReset, outputDecl.dataType);
            stream << ";" << std::endl;
        }

        cf.indent(stream, indentation+1);
        stream << "ELSIF";
    } else {
        cf.indent(stream, indentation+1);
        stream << "IF";
    }

    switch (m_config.triggerEvent) {
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
    if (m_config.reset != nullptr && m_config.resetType == hlim::RegisterAttributes::ResetType::SYNCHRONOUS) {
        cf.indent(stream, indentation+2);
        const char resetValue = m_config.clock->getRegAttribs().resetActive == hlim::RegisterAttributes::Active::HIGH ? '1' : '0';
        stream << "IF (" << resetName << " = '" << resetValue << "') THEN" << std::endl;

        for (auto node : m_nodes) {
            hlim::Node_Register *regNode = dynamic_cast<hlim::Node_Register *>(node);
            HCL_ASSERT(regNode != nullptr);

            hlim::NodePort output = {.node = regNode, .port = 0};
            hlim::NodePort resetValue = regNode->getNonSignalDriver(hlim::Node_Register::RESET_VALUE);

            HCL_ASSERT(resetValue.node != nullptr);
            auto *constReset = dynamic_cast<hlim::Node_Constant*>(resetValue.node);
            HCL_DESIGNCHECK_HINT(constReset, "Resets of registers must be constants uppon export!");

            const auto &outputDecl = m_namespaceScope.get(output);

            cf.indent(stream, indentation+3);
            stream << outputDecl.name << " <= ";
            formatConstant(stream, constReset, outputDecl.dataType);
            stream << ";" << std::endl;
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


        const auto &inputDecl = m_namespaceScope.get(dataInput);
        const auto &outputDecl = m_namespaceScope.get(output);

        if (enableInput.node != nullptr) {
            cf.indent(stream, indentation+2+indentationOffset);
            stream << "IF (" << m_namespaceScope.get(enableInput).name << " = '1') THEN" << std::endl;

            cf.indent(stream, indentation+3+indentationOffset);
            stream << outputDecl.name << " <= ";
            if (outputDecl.dataType != inputDecl.dataType) {
                cf.formatDataType(stream, outputDecl.dataType);
                stream << '(' << inputDecl.name << ");" << std::endl;
            } else
                stream << inputDecl.name << ";" << std::endl;

            cf.indent(stream, indentation+2+indentationOffset);
            stream << "END IF;" << std::endl;
        } else {
            cf.indent(stream, indentation+2+indentationOffset);
            stream << outputDecl.name << " <= ";
            if (outputDecl.dataType != inputDecl.dataType) {
                cf.formatDataType(stream, outputDecl.dataType);
                stream << '(' << inputDecl.name << ");" << std::endl;
            } else
                stream << inputDecl.name << ";" << std::endl;
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
