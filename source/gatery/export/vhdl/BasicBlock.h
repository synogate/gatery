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

#include "BaseGrouping.h"

#include <vector>
#include <memory>
#include <compare>

namespace gtry::vhdl {

class Entity;
class Process;
class Block;


struct ConcurrentStatement
{
	enum Type {
		TYPE_ENTITY_INSTANTIATION,
		TYPE_EXT_NODE_INSTANTIATION,
		TYPE_BLOCK,
		TYPE_PROCESS,
	};
	Type type;
	union {
		size_t entityIdx;
		size_t externalNodeIdx;
		Block *block;
		Process *process;
	} ref;
	size_t sortIdx = 0;

	inline bool operator<(const ConcurrentStatement &rhs) const { return sortIdx < rhs.sortIdx; }
};



struct ShiftRegStorage
{
	NodeInternalStorageSignal ref;
	size_t delay;
	hlim::ConnectionType type;
};

/**
 * @todo write docs
 */
class BasicBlock : public BaseGrouping
{
	public:
		BasicBlock(AST &ast, BasicBlock *parent, NamespaceScope *parentNamespace);
		virtual ~BasicBlock();

		virtual void extractSignals() override;
		virtual void allocateNames() override;
		
		virtual bool findLocalDeclaration(hlim::NodePort driver, std::vector<BaseGrouping*> &reversePath) override;

		inline const std::vector<Entity*> &getSubEntities() const { return m_entities; }
		inline const std::vector<std::string> &getSubEntityInstanceNames() const { return m_entityInstanceNames; }
		inline const std::vector<hlim::Node_External*> getExternalNodes() const { return m_externalNodes; }
		void addNeededLibraries(std::set<std::string> &libs) const;
	protected:
		void collectInstantiations(hlim::NodeGroup *nodeGroup, bool reccursive);
		void processifyNodes(const std::string &desiredProcessName, hlim::NodeGroup *nodeGroup, bool reccursive);
		void routeChildIOUpwards(BaseGrouping *child);
		virtual void writeStatementsVHDL(std::ostream &stream, unsigned indent);


		std::vector<ShiftRegStorage> m_shiftRegStorage;
		std::vector<std::unique_ptr<Process>> m_processes;
		std::vector<Entity*> m_entities;
		std::vector<std::string> m_entityInstanceNames;
		std::vector<hlim::Node_External*> m_externalNodes;
		std::vector<std::string> m_externalNodeInstanceNames;

		std::vector<ConcurrentStatement> m_statements;

		void handleEntityInstantiation(hlim::NodeGroup *nodeGroup);
		void handleExternalNodeInstantiaton(hlim::Node_External *externalNode);
		void handleSFUInstantiaton(hlim::NodeGroup *sfu);
};


}
