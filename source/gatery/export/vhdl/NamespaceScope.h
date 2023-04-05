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
#pragma once

#include "../../hlim/Node.h"
#include "../../hlim/SignalGroup.h"

#include <gatery/utils/StableContainers.h>

#include "CodeFormatting.h"
#include "VHDLSignalDeclaration.h"

#include <boost/algorithm/string.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>


namespace gtry::vhdl {

class AST;

struct NodeInternalStorageSignal
{
	hlim::BaseNode *node;
	size_t signalIdx;

	inline bool operator==(const NodeInternalStorageSignal &rhs) const { return node == rhs.node && signalIdx == rhs.signalIdx; }
};

}

template<>
struct gtry::utils::StableCompare<gtry::vhdl::NodeInternalStorageSignal>
{
	bool operator()(const gtry::vhdl::NodeInternalStorageSignal &lhs, const gtry::vhdl::NodeInternalStorageSignal &rhs) const {
		if (lhs.node == nullptr) {
			return rhs.node != nullptr;
		} else {
			if (rhs.node == nullptr) return false;
			if (lhs.node->getId() < rhs.node->getId())
				return true;
			if (lhs.node->getId() > rhs.node->getId())
				return false;
			return lhs.signalIdx < rhs.signalIdx;
		}
	}
};

namespace gtry::vhdl {

struct TypeDefinition
{
	std::vector<hlim::SignalGroup*> signalGroups;
	std::string typeName;
	std::string desiredTypeName;

	bool compatibleWith(hlim::SignalGroup *signalGroup);
};

/**
 * @todo write docs
 */
class NamespaceScope
{
	public:
		NamespaceScope(AST &ast, NamespaceScope *parent);
		virtual ~NamespaceScope() { }

		std::string allocateName(hlim::NodePort nodePort, const std::string &desiredName, VHDLDataType dataType, CodeFormatting::SignalType type);
		const VHDLSignalDeclaration &get(const hlim::NodePort nodePort) const;

		std::string allocateName(NodeInternalStorageSignal nodePort, const std::string &desiredName);
		const std::string &getName(NodeInternalStorageSignal nodePort) const;

		std::string allocateName(hlim::Clock *clock, const std::string &desiredName);
		const VHDLSignalDeclaration &getClock(const hlim::Clock *clock) const;

		std::string allocateResetName(hlim::Clock *clock, const std::string &desiredName);
		const VHDLSignalDeclaration &getReset(const hlim::Clock *clock) const;

		std::string allocateName(hlim::Node_Pin *ioPin, const std::string &desiredName, VHDLDataType dataType);
		const VHDLSignalDeclaration &get(const hlim::Node_Pin *ioPin) const;

		std::string allocateSupportFileName(const std::string &desiredName);
		std::string allocatePackageName(const std::string &desiredName);
		std::string allocateEntityName(const std::string &desiredName);
		std::string allocateBlockName(const std::string &desiredName);
		std::string allocateProcessName(const std::string &desiredName, bool clocked);
		std::string allocateInstanceName(const std::string &desiredName);
	protected:
		bool isNameInUse(const std::string &lowerCaseName) const;
		AST &m_ast;
		NamespaceScope *m_parent = nullptr;

		std::set<std::string> m_namesInUse;
		std::map<std::string, size_t> m_nextNameAttempt;
		utils::StableMap<hlim::NodePort, VHDLSignalDeclaration> m_nodeNames;
		utils::StableMap<NodeInternalStorageSignal, std::string> m_nodeStorageNames;
		utils::StableMap<hlim::Clock*, VHDLSignalDeclaration> m_clockNames;
		utils::StableMap<hlim::Clock*, VHDLSignalDeclaration> m_resetNames;
		utils::StableMap<hlim::Node_Pin*, VHDLSignalDeclaration> m_ioPinNames;
		std::vector<TypeDefinition> m_typeDefinitions;
};


}
