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

#include <gatery/hlim/postprocessing/MemoryDetector.h>

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
		
		virtual void allocateInputAndOutputNames() override;

		void buildFrom(hlim::NodeGroup *memNodeGrp);
	protected:		
		hlim::MemoryGroup *m_memGrp;


		void formatReadPortOutputSignalName(std::ostream &stream, const hlim::MemoryGroup::ReadPort &readPort, size_t regIndex) const;
		void formatReadPortInputRegisterSignalName(std::ostream &stream, const hlim::NodePort &driver) const;

		virtual void writeLocalSignalsVHDL(std::ostream &stream) override;
		virtual void writeStatementsVHDL(std::ostream &stream, unsigned indent) override;

		void generateReadPortConflictDetectionExpression(std::ostream &stream, const hlim::MemoryGroup::ReadPort &readPort);

		void openResetCondition(std::ostream &stream, unsigned indent, const std::string_view resetName, const RegisterConfig &reset);
		void exportResetOutputReg(std::ostream &stream, unsigned indent, const hlim::MemoryGroup::ReadPort &readPort, size_t regIndex);
		void exportWrite(std::ostream &stream, unsigned indent, const hlim::MemoryGroup::WritePort &writePort);
		void exportRead(std::ostream &stream, unsigned indent, const hlim::MemoryGroup::ReadPort &readPort, bool registeredInputs, bool useEnablePort);
		void exportReadInputReg(std::ostream &stream, unsigned indent, const hlim::MemoryGroup::ReadPort &readPort);
		void exportReadOutputReg(std::ostream &stream, unsigned indent, const hlim::MemoryGroup::ReadPort &readPort, size_t regIndex);
		void exportReadConflictOverride(std::ostream &stream, unsigned indent, const hlim::MemoryGroup::ReadPort &readPort);

		GenericMemoryPorts groupPorts() const;
};

}
