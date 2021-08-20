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
#include "BaseGrouping.h"

#include "AST.h"

#include "../../utils/Exceptions.h"
#include "../../utils/Preprocessor.h"

#include "../../hlim/coreNodes/Node_Constant.h"
#include "../../hlim/coreNodes/Node_Signal.h"
#include "../../hlim/coreNodes/Node_Register.h"
#include "../../hlim/coreNodes/Node_Pin.h"
#include "../../hlim/supportNodes/Node_Attributes.h"
#include "../../hlim/Clock.h"
#include "../../frontend/SynthesisTool.h"

#include <iostream>

namespace gtry::vhdl {

BaseGrouping::BaseGrouping(AST &ast, BaseGrouping *parent, NamespaceScope *parentNamespace) : 
                    m_ast(ast), m_namespaceScope(ast, parentNamespace), m_parent(parent)
{
    
}

BaseGrouping::~BaseGrouping() 
{ 
    
}

bool BaseGrouping::isChildOf(const BaseGrouping *other) const
{
    const BaseGrouping *parent = getParent();
    while (parent != nullptr) {
        if (parent == other)
            return true;
        parent = parent->getParent();
    }
    return false;
}

bool BaseGrouping::isProducedExternally(hlim::NodePort nodePort)
{
    auto driverNodeGroup = m_ast.getMapping().getScope(nodePort.node);
    return driverNodeGroup == nullptr || (driverNodeGroup != this && !driverNodeGroup->isChildOf(this));        
}

bool BaseGrouping::isConsumedExternally(hlim::NodePort nodePort)
{
    for (auto driven : nodePort.node->getDirectlyDriven(nodePort.port)) {
        if (!m_ast.isPartOfExport(driven.node)) continue;
        
        auto drivenNodeGroup = m_ast.getMapping().getScope(driven.node);
        if (drivenNodeGroup == nullptr || (drivenNodeGroup != this && !drivenNodeGroup->isChildOf(this))) 
            return true;
    }

    return false;
}

std::string BaseGrouping::findNearestDesiredName(hlim::NodePort nodePort)
{
    if (nodePort.node == nullptr)
        return "";

    if (nodePort.node->hasGivenName())
        return nodePort.node->getName();

    if (dynamic_cast<hlim::Node_Signal*>(nodePort.node) != nullptr)
        return nodePort.node->getName();

    for (auto driven : nodePort.node->getDirectlyDriven(nodePort.port))
        if (dynamic_cast<hlim::Node_Signal*>(driven.node) != nullptr)
            return driven.node->getName();
    
    return "";
}

void BaseGrouping::verifySignalsDisjoint()
{
    for (auto & s : m_inputs) {
        HCL_ASSERT(!m_outputs.contains(s));
        HCL_ASSERT(!m_localSignals.contains(s));
        HCL_ASSERT(!m_constants.contains(s));
    } 
    for (auto & s : m_outputs) {
        HCL_ASSERT(!m_inputs.contains(s));
        HCL_ASSERT(!m_localSignals.contains(s));
        HCL_ASSERT(!m_constants.contains(s));
    } 
    for (auto & s : m_localSignals) {
        HCL_ASSERT(!m_outputs.contains(s));
        HCL_ASSERT(!m_inputs.contains(s));
        HCL_ASSERT(!m_constants.contains(s));
    } 
    for (auto & s : m_constants) {
        HCL_ASSERT(!m_outputs.contains(s));
        HCL_ASSERT(!m_inputs.contains(s));
        HCL_ASSERT(!m_localSignals.contains(s));
    } 
}

/// @todo: Better move to code formatting
void BaseGrouping::formatConstant(std::ostream &stream, const hlim::Node_Constant *constant, Context context)
{
    const auto& conType = constant->getOutputConnectionType(0);

    if (context == Context::BOOL) {
        HCL_ASSERT(conType.interpretation == hlim::ConnectionType::BOOL);
        const auto &v = constant->getValue();
        HCL_ASSERT(v.get(sim::DefaultConfig::DEFINED, 0));
        if (v.get(sim::DefaultConfig::VALUE, 0))
            stream << "true";
        else
            stream << "false";
    } else {
        char sep = '"';
        if (conType.interpretation == hlim::ConnectionType::BOOL)
            sep = '\'';

        stream << sep;
        stream << constant->getValue();
        stream << sep;
    }
}

void BaseGrouping::declareLocalSignals(std::ostream &stream, bool asVariables, unsigned indentation)
{
   CodeFormatting &cf = m_ast.getCodeFormatting();


    for (const auto &signal : m_constants) {
        auto targetContext = hlim::outputIsBVec(signal)?Context::STD_LOGIC_VECTOR:Context::STD_LOGIC;

        cf.indent(stream, indentation+1);
        stream << "CONSTANT " << m_namespaceScope.getName(signal) << " : ";
        cf.formatConnectionType(stream, hlim::getOutputConnectionType(signal));
        stream << " := ";
        formatConstant(stream, dynamic_cast<hlim::Node_Constant*>(signal.node), targetContext);
        stream << "; "<< std::endl;
    }

    for (const auto &signal : m_localSignals) {
        cf.indent(stream, indentation+1);
        if (asVariables)
            stream << "VARIABLE ";
        else
            stream << "SIGNAL ";
        stream << m_namespaceScope.getName(signal) << " : ";
        cf.formatConnectionType(stream, hlim::getOutputConnectionType(signal));

        auto it = m_localSignalDefaultValues.find(signal);
        if (it != m_localSignalDefaultValues.end()) {
            auto targetContext = hlim::outputIsBVec(signal)?Context::STD_LOGIC_VECTOR:Context::STD_LOGIC;

            stream << " := ";
            formatConstant(stream, it->second, targetContext);
        }

        stream << "; "<< std::endl;
    }

    std::map<std::string, hlim::AttribValue> alreadyDeclaredAttribs;

    hlim::ResolvedAttributes resolvedAttribs;

    for (const auto &signal : m_localSignals) {

        resolvedAttribs.clear();

        // find signals driven by registers
        if (auto *reg = dynamic_cast<hlim::Node_Register*>(signal.node)) {
            auto *clock = reg->getClocks()[0];
            HCL_ASSERT(clock != nullptr);
            
            m_ast.getSynthesisTool().resolveAttributes(clock->getRegAttribs(), resolvedAttribs);
        }

        // Find and accumulate all attribute nodes
        for (auto nh : signal.node->exploreOutput(signal.port)) {
            if (const auto *attrib = dynamic_cast<const hlim::Node_Attributes*>(nh.node())) {
                m_ast.getSynthesisTool().resolveAttributes(attrib->getAttribs(), resolvedAttribs);
            } else if (!nh.isSignal()) nh.backtrack();

            if (nh.node() == signal.node) {

                std::cout << "Loop: " << std::endl;
                hlim::BaseNode* n = nh.node();
                do {
                    std::cout << n->getName() << " - " << n->getId() << " - " << n->getTypeName() << std::endl;
                    std::cout << n->getStackTrace() << std::endl << std::endl;
                    n = n->getDriver(0).node;
                } while (n != signal.node);


                HCL_DESIGNCHECK_HINT(false, "Loop detected!");
            }
        }

        // write out all attribs
        for (const auto &attrib : resolvedAttribs) {
            auto it = alreadyDeclaredAttribs.find(attrib.first);
            if (it == alreadyDeclaredAttribs.end()) {
                alreadyDeclaredAttribs[attrib.first] = attrib.second;

                cf.indent(stream, indentation+1);
                stream << "ATTRIBUTE " << attrib.first << " : " << attrib.second.type << ';' << std::endl;
            } else
                HCL_DESIGNCHECK_HINT(it->second.type == attrib.second.type, "Same attribute can't have different types!");

            cf.indent(stream, indentation+1);
            stream << "ATTRIBUTE " << attrib.first << " of " << m_namespaceScope.getName(signal) << " : ";
            if (asVariables)
                stream << "VARIABLE";
            else
                stream << "SIGNAL";
            stream << " is " << attrib.second.value << ';' << std::endl;
        }
    }
}

bool BaseGrouping::findLocalDeclaration(hlim::NodePort driver, std::vector<BaseGrouping*> &reversePath)
{
    if (m_localSignals.contains(driver)) {
        reversePath = {this};
        return true;
    }
    HCL_ASSERT_HINT(!m_ioPins.contains(dynamic_cast<hlim::Node_Pin*>(driver.node)), "Requesting base group of an IO-pin which is always top entity!");

    return false;
}


}
