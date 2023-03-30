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

#include "NamespaceScope.h"
#include <gatery/utils/StableContainers.h>

#include <vector>
#include <map>
#include <memory>
#include <tuple>

namespace gtry::hlim {
	class Node_Constant;
}

namespace gtry::vhdl {
	
class Entity;
class Process;
class Block;


/**
 * @todo write docs
 */
class BaseGrouping
{
	public:
		BaseGrouping(AST &ast, BaseGrouping *parent, NamespaceScope *parentNamespace);
		virtual ~BaseGrouping();
		
		inline AST &getAST() { return m_ast; }
		inline NamespaceScope &getNamespaceScope() { return m_namespaceScope; }
		inline const NamespaceScope &getNamespaceScope() const { return m_namespaceScope; }
		inline BaseGrouping *getParent() { return m_parent; }
		inline const BaseGrouping *getParent() const { return m_parent; }
		bool isChildOf(const BaseGrouping *other) const;

		virtual std::string getInstanceName() = 0;
		
		virtual void extractSignals() = 0;
		virtual void allocateNames() = 0;

		virtual bool findLocalDeclaration(hlim::NodePort driver, std::vector<BaseGrouping*> &reversePath);

		inline const utils::StableSet<hlim::NodePort> &getLocalSignals() const { return m_localSignals; }
		inline const utils::StableSet<hlim::NodePort> &getInputs() const { return m_inputs; }
		inline const utils::StableSet<hlim::NodePort> &getOutputs() const { return m_outputs; }
		inline const utils::StableSet<hlim::Clock *> &getClocks() const { return m_inputClocks; }
		inline const utils::StableSet<hlim::Clock *> &getResets() const { return m_inputResets; }
		inline const utils::StableSet<hlim::Node_Pin *> &getIoPins() const { return m_ioPins; }
	protected:
		AST &m_ast;
		NamespaceScope m_namespaceScope;
		BaseGrouping *m_parent;
		std::string m_name;
		std::string m_comment;
		
		utils::StableSet<hlim::NodePort> m_constants;
		utils::StableSet<hlim::NodePort> m_localSignals;
		utils::UnstableMap<hlim::NodePort, hlim::Node_Constant*> m_localSignalDefaultValues;
		utils::StableSet<hlim::NodePort> m_inputs;
		utils::StableSet<hlim::NodePort> m_outputs;
		utils::StableSet<hlim::Clock *> m_inputClocks;
		utils::StableSet<hlim::Clock *> m_inputResets;
		utils::StableSet<hlim::Node_Pin*> m_ioPins;

		
		bool isProducedExternally(hlim::NodePort nodePort);
		bool isConsumedExternally(hlim::NodePort nodePort);
		std::tuple<bool,bool,bool> isConsumedInternallyHigherLower(hlim::NodePort nodePort);
		
		std::string findNearestDesiredName(hlim::NodePort nodePort);

		void verifySignalsDisjoint();

		void formatConstant(std::ostream &stream, const hlim::Node_Constant *constant, VHDLDataType targetType);
		void declareLocalSignals(std::ostream &stream, bool asVariables, unsigned indentation);
};


}
