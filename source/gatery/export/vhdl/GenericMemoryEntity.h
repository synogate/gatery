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

#include "Entity.h"

namespace gtry::hlim {
	class MemoryGroup;
}

namespace gtry::vhdl {

struct RegisterConfig;
struct GenericMemoryPorts;

/**
 * @todo write docs
 */
class GenericMemoryEntity : public Entity
{
	public:
		GenericMemoryEntity(AST &ast, const std::string &desiredName, BasicBlock *parent);
		
		void buildFrom(hlim::NodeGroup *memNodeGrp);
	protected:		
		hlim::MemoryGroup *m_memGrp;		

		virtual void writeLocalSignalsVHDL(std::ostream &stream) override;
		virtual void writeStatementsVHDL(std::ostream &stream, unsigned indent) override;

		void writeResets(std::ostream &stream, unsigned indent, const std::string_view resetName, const std::pair<RegisterConfig, GenericMemoryPorts> &clockReset);
		void writeWritePorts(std::ostream &stream, unsigned indent, const std::pair<RegisterConfig, GenericMemoryPorts> &clockReset);
		void writeReadPorts(std::ostream &stream, unsigned indent, const std::pair<RegisterConfig, GenericMemoryPorts> &clockReset, bool asyncMode);
};

}
