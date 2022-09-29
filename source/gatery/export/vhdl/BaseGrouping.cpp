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

#include "../../debug/DebugInterface.h"
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

std::tuple<bool,bool,bool> BaseGrouping::isConsumedInternallyHigherLower(hlim::NodePort nodePort)
{
	bool internally = false;
	bool higher = false;
	bool lower = false;

	for (auto driven : nodePort.node->getDirectlyDriven(nodePort.port)) {
		if (!m_ast.isPartOfExport(driven.node)) continue;
		
		auto drivenNodeGroup = m_ast.getMapping().getScope(driven.node);
		if (drivenNodeGroup == this) {
			internally = true;
		} else if (drivenNodeGroup == nullptr) {
			higher = true;
		} else if (drivenNodeGroup->isChildOf(this)) {
			lower = true;
		} else 
			higher = true;
	}

	return {internally, higher, lower};
}

std::string BaseGrouping::findNearestDesiredName(hlim::NodePort nodePort)
{
	HCL_ASSERT(nodePort.node != nullptr);

	if (nodePort.node->hasGivenName())
		return nodePort.node->getName();

	if (dynamic_cast<hlim::Node_Signal*>(nodePort.node) != nullptr && !nodePort.node->getName().empty())
		return nodePort.node->getName();

	for (auto driven : nodePort.node->getDirectlyDriven(nodePort.port))
		if (dynamic_cast<hlim::Node_Signal*>(driven.node) != nullptr && !driven.node->getName().empty())
			return driven.node->getName();
	
	auto name = nodePort.node->attemptInferOutputName(nodePort.port);
	if (!name.empty()) 
		return name;
		
	return "unnamed";
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
void BaseGrouping::formatConstant(std::ostream &stream, const hlim::Node_Constant *constant, VHDLDataType targetType)
{
	const auto& conType = constant->getOutputConnectionType(0);

	if (targetType == VHDLDataType::BOOL) {
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
		const auto &decl = m_namespaceScope.get(signal);

		cf.indent(stream, indentation+1);
		stream << "CONSTANT ";
		cf.formatDeclaration(stream, decl);
		stream << " := ";
		formatConstant(stream, dynamic_cast<hlim::Node_Constant*>(signal.node), decl.dataType);
		stream << "; "<< std::endl;
	}

	// build clock signals as local variables is they are not part of the port map
	bool isRoot = m_parent == nullptr;
	if (isRoot) {
		for (const auto &clk : m_inputClocks) {
			if (!clk->isSelfDriven(false, true)) {
				cf.indent(stream, indentation+1);
				stream << "SIGNAL " << m_namespaceScope.getClock(clk).name << " : STD_LOGIC;\n";
			}
		}

		for (const auto &clk : m_inputResets) {
			if (!clk->isSelfDriven(false, false)) {
				cf.indent(stream, indentation+1);
				stream << "SIGNAL " << m_namespaceScope.getReset(clk).name << " : STD_LOGIC;\n";
			}
		}	
	}

	for (const auto &signal : m_localSignals) {
		const auto &decl = m_namespaceScope.get(signal);

		cf.indent(stream, indentation+1);
		if (asVariables)
			stream << "VARIABLE ";
		else
			stream << "SIGNAL ";
		cf.formatDeclaration(stream, decl);

		auto it = m_localSignalDefaultValues.find(signal);
		if (it != m_localSignalDefaultValues.end()) {
			stream << " := ";
			formatConstant(stream, it->second, decl.dataType);
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
			} else if (!nh.isSignal())
				nh.backtrack();
			else
				if (nh.node() == signal.node) {
					dbg::awaitDebugger();
					dbg::pushGraph();
					dbg::LogMessage message;
					message << dbg::LogMessage::LOG_ERROR << "Cyclic dependency of signals:";

					hlim::BaseNode* n = nh.node();
					do {
						message << n;
						n = n->getDriver(0).node;
					} while (n != signal.node);

					dbg::log(std::move(message));
					dbg::stopInDebugger();

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
			stream << "ATTRIBUTE " << attrib.first << " of " << m_namespaceScope.get(signal).name << " : ";
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
