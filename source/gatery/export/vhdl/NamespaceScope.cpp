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
#include "NamespaceScope.h"

#include "AST.h"

#include "../../utils/Exceptions.h"
#include "../../utils/Preprocessor.h"

#include "../../hlim/coreNodes/Node_Pin.h"

namespace gtry::vhdl {

NamespaceScope::NamespaceScope(AST &ast, NamespaceScope *parent) : m_ast(ast), m_parent(parent)
{
	for (auto keyword :
			{"abs", "access", "after", "alias", "all", "and", "architecture", "array", "assert", "attribute", "begin", "block", "body", "buffer", "bus", "case", "component", "configuration", "constant",
			"disconnect", "downto", "else", "elsif", "end", "entity", "exit", "file", "for", "function", "generate", "generic", "group", "guarded", "if", "impure", "in", "inertial", "inout",
			"is", "label", "library", "linkage", "literal", "loop", "map", "mod", "nand", "new", "next", "nor", "not", "null", "of", "on", "open", "or", "others",
			"out", "package", "port", "postponed", "procedure", "process", "pure", "range", "record", "register", "reject", "return", "rol", "ror", "select", "severity", "signal", "shared", "sla",
			"sli", "sra", "srl", "subtype", "then", "to", "transport", "type", "unaffected", "units", "until", "use", "variable", "wait", "when", "while", "with", "xnor", "xor"})
		m_namesInUse.insert(keyword);
}

std::string NamespaceScope::allocateName(hlim::NodePort nodePort, const std::string &desiredName, VHDLDataType dataType, CodeFormatting::SignalType type)
{
	HCL_ASSERT(!desiredName.empty());

	CodeFormatting &cf = m_ast.getCodeFormatting();

	HCL_ASSERT(m_nodeNames.find(nodePort) == m_nodeNames.end());

	auto &attempt = m_nextNameAttempt[desiredName];
	std::string name, lowerCaseName;
	do {
		name = cf.getSignalName(desiredName, type, attempt++);
		lowerCaseName = boost::to_lower_copy(name);
	} while (isNameInUse(lowerCaseName));

	m_namesInUse.insert(lowerCaseName);
	auto &data = m_nodeNames[nodePort];
	data.name = name;
	data.dataType = dataType;
	data.width = hlim::getOutputWidth(nodePort);
	return name;
}
const VHDLSignalDeclaration &NamespaceScope::get(const hlim::NodePort nodePort) const
{
	auto it = m_nodeNames.find(nodePort);
	if (it != m_nodeNames.end())
		return it->second;

	HCL_ASSERT_HINT(m_parent != nullptr, "End of namespace scope chain reached, it seems no name was allocated for the given NodePort!");
	return m_parent->get(nodePort);
}

std::string NamespaceScope::allocateName(hlim::Clock *clock, const std::string &desiredName)
{
	HCL_ASSERT(!desiredName.empty());
	CodeFormatting &cf = m_ast.getCodeFormatting();

	HCL_ASSERT(m_clockNames.find(clock) == m_clockNames.end());

	auto &attempt = m_nextNameAttempt[desiredName];
	std::string name, lowerCaseName;
	do {
		name = cf.getClockName(desiredName, attempt++);
		lowerCaseName = boost::to_lower_copy(name);
	} while (isNameInUse(lowerCaseName));

	m_namesInUse.insert(lowerCaseName);
	auto &data = m_clockNames[clock];
	data.name = name;
	data.dataType = VHDLDataType::STD_LOGIC;
	data.width = 1;
	return name;
}

const VHDLSignalDeclaration &NamespaceScope::getClock(const hlim::Clock *clock) const
{
	auto it = m_clockNames.find(const_cast<hlim::Clock*>(clock));
	if (it != m_clockNames.end())
		return it->second;

	HCL_ASSERT_HINT(m_parent != nullptr, "End of namespace scope chain reached, it seems no name was allocated for the given clock!");
	return m_parent->getClock(clock);
}

std::string NamespaceScope::allocateResetName(hlim::Clock *clock, const std::string &desiredName)
{
	HCL_ASSERT(!desiredName.empty());
	CodeFormatting &cf = m_ast.getCodeFormatting();

	HCL_ASSERT(m_resetNames.find(clock) == m_resetNames.end());

	auto &attempt = m_nextNameAttempt[desiredName];
	std::string name, lowerCaseName;
	do {
		name = cf.getClockName(desiredName, attempt++);
		lowerCaseName = boost::to_lower_copy(name);
	} while (isNameInUse(lowerCaseName));

	m_namesInUse.insert(lowerCaseName);
	auto &data = m_resetNames[clock];
	data.name = name;
	data.dataType = VHDLDataType::STD_LOGIC;
	data.width = 1;
	return name;
}

const VHDLSignalDeclaration &NamespaceScope::getReset(const hlim::Clock *clock) const
{
	auto it = m_resetNames.find(const_cast<hlim::Clock*>(clock));
	if (it != m_resetNames.end())
		return it->second;

	HCL_ASSERT_HINT(m_parent != nullptr, "End of namespace scope chain reached, it seems no name was allocated for the given reset!");
	return m_parent->getReset(clock);
}


std::string NamespaceScope::allocateName(hlim::Node_Pin *ioPin, const std::string &desiredName, VHDLDataType dataType)
{
	HCL_ASSERT(!desiredName.empty());
	CodeFormatting &cf = m_ast.getCodeFormatting();

	HCL_ASSERT(m_ioPinNames.find(ioPin) == m_ioPinNames.end());

	auto &attempt = m_nextNameAttempt[desiredName];
	std::string name, lowerCaseName;
	do {
		name = cf.getIoPinName(desiredName, attempt++);
		lowerCaseName = boost::to_lower_copy(name);
	} while (isNameInUse(lowerCaseName));

	m_namesInUse.insert(lowerCaseName);
	auto &data = m_ioPinNames[ioPin];
	data.name = name;
	data.dataType = dataType;
	data.width = ioPin->getConnectionType().width;
	return name;
}

const VHDLSignalDeclaration &NamespaceScope::get(const hlim::Node_Pin *ioPin) const
{
	auto it = m_ioPinNames.find(const_cast<hlim::Node_Pin*>(ioPin));
	if (it != m_ioPinNames.end())
		return it->second;

	HCL_ASSERT_HINT(m_parent != nullptr, "End of namespace scope chain reached, it seems no name was allocated for the given ioPin!");
	return m_parent->get(ioPin);
}

std::string NamespaceScope::allocatePackageName(const std::string &desiredName)
{
	HCL_ASSERT(m_parent == nullptr);
	HCL_ASSERT(!desiredName.empty());
	CodeFormatting &cf = m_ast.getCodeFormatting();

	auto &attempt = m_nextNameAttempt[desiredName];
	std::string name, lowerCaseName;
	do {
		name = cf.getPackageName(desiredName, attempt++);
		lowerCaseName = boost::to_lower_copy(name);
	} while (isNameInUse(lowerCaseName));

	m_namesInUse.insert(lowerCaseName);
	return name;
}

std::string NamespaceScope::allocateSupportFileName(const std::string &desiredName)
{
	if (m_parent != nullptr)
		return m_parent->allocateSupportFileName(desiredName);

	return desiredName; // todo: Actually do some checking
}

std::string NamespaceScope::allocateEntityName(const std::string &desiredName)
{
	HCL_ASSERT(m_parent == nullptr);
	HCL_ASSERT(!desiredName.empty());
	CodeFormatting &cf = m_ast.getCodeFormatting();

	auto &attempt = m_nextNameAttempt[desiredName];
	std::string name, lowerCaseName;
	do {
		name = cf.getEntityName(desiredName, attempt++);
		lowerCaseName = boost::to_lower_copy(name);
	} while (isNameInUse(lowerCaseName));

	m_namesInUse.insert(lowerCaseName);
	return name;
}

std::string NamespaceScope::allocateBlockName(const std::string &desiredName)
{
	HCL_ASSERT(!desiredName.empty());
	CodeFormatting &cf = m_ast.getCodeFormatting();

	auto &attempt = m_nextNameAttempt[desiredName];
	std::string name, lowerCaseName;
	do {
		name = cf.getBlockName(desiredName, attempt++);
		lowerCaseName = boost::to_lower_copy(name);
	} while (isNameInUse(lowerCaseName));

	m_namesInUse.insert(lowerCaseName);
	return name;
}

std::string NamespaceScope::allocateProcessName(const std::string &desiredName, bool clocked)
{
	HCL_ASSERT(!desiredName.empty());
	CodeFormatting &cf = m_ast.getCodeFormatting();

	auto &attempt = m_nextNameAttempt[desiredName];
	std::string name, lowerCaseName;
	do {
		name = cf.getProcessName(desiredName, clocked, attempt++);
		lowerCaseName = boost::to_lower_copy(name);
	} while (isNameInUse(lowerCaseName));

	m_namesInUse.insert(lowerCaseName);
	return name;
}

std::string NamespaceScope::allocateInstanceName(const std::string &desiredName)
{
	HCL_ASSERT(!desiredName.empty());
	CodeFormatting &cf = m_ast.getCodeFormatting();

	auto &attempt = m_nextNameAttempt[desiredName];
	std::string name, lowerCaseName;
	do {
		name = cf.getInstanceName(desiredName, attempt++);
		lowerCaseName = boost::to_lower_copy(name);
	} while (isNameInUse(lowerCaseName));

	m_namesInUse.insert(lowerCaseName);
	return name;
}


bool NamespaceScope::isNameInUse(const std::string &lowerCaseName) const
{
	if (m_namesInUse.find(lowerCaseName) != m_namesInUse.end()) return true;
	if (m_parent != nullptr)
		return m_parent->isNameInUse(lowerCaseName);
	return false;

}



}
