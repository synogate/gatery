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

#include "../../hlim/NodeGroup.h"
#include "../../hlim/supportNodes/Node_External.h"

#include "VHDLSignalDeclaration.h"

#include <filesystem>

#include <string>
#include <ostream>
#include <functional>

namespace gtry::vhdl {


/**
 * @todo write docs
 */
class CodeFormatting
{
	public:
		virtual ~CodeFormatting() = default;

		inline const std::string &getIndentation() const { return m_indentation; }
		inline const std::string &getFileHeader() const { return m_fileHeader; }
		inline const std::string &getFilenameExtension() const { return m_filenameExtension; }
		void indent(std::ostream &stream, size_t depth) const;
		virtual void formatEntityComment(std::ostream &stream, const std::string &entityName, const std::string &comment) const = 0;
		virtual void formatBlockComment(std::ostream &stream, const std::string &blockName, const std::string &comment) const = 0;
		virtual void formatProcessComment(std::ostream &stream, size_t indentation, const std::string &processName, const std::string &comment) const = 0;
		virtual void formatCodeComment(std::ostream &stream, size_t indentation, const std::string &comment) const = 0;

		virtual void formatConnectionType(std::ostream &stream, const VHDLSignalDeclaration &declaration) const = 0;
		virtual void formatDeclaration(std::ostream &stream, const VHDLSignalDeclaration &declaration) const = 0;
		
		virtual void formatDataType(std::ostream &stream, VHDLDataType dataType) const = 0;

		virtual void formatDecimalFlavor(std::ostream &stream, hlim::GenericParameter::DecimalFlavor flavor) const = 0;
		virtual void formatBitFlavor(std::ostream &stream, hlim::GenericParameter::BitFlavor flavor) const = 0;
		virtual void formatBitVectorFlavor(std::ostream &stream, hlim::GenericParameter::BitVectorFlavor flavor) const = 0;

		virtual void formatGenericParameterType(std::ostream &stream, const hlim::GenericParameter &param) const = 0;

		enum SignalType {
			SIG_ENTITY_INPUT,
			SIG_ENTITY_OUTPUT,
			SIG_CHILD_ENTITY_INPUT,
			SIG_CHILD_ENTITY_OUTPUT,
			SIG_REGISTER_INPUT,
			SIG_REGISTER_OUTPUT,
			SIG_LOCAL_SIGNAL,
			SIG_LOCAL_VARIABLE,
			SIG_CONSTANT,
		};

		virtual std::string getNodeName(const hlim::BaseNode *node, size_t attempt) const = 0;
		virtual std::string getSignalName(const std::string &desiredName, SignalType type, size_t attempt) const = 0;
		virtual std::string getPackageName(const std::string &desiredName, size_t attempt) const = 0;
		virtual std::string getEntityName(const std::string &desiredName, size_t attempt) const = 0;
		virtual std::string getBlockName(const std::string &desiredName, size_t attempt) const = 0;
		virtual std::string getProcessName(const std::string &desiredName, bool clocked, size_t attempt) const = 0;
		virtual std::string getClockName(const std::string &desiredName, size_t attempt) const = 0;
		virtual std::string getIoPinName(const std::string &desiredName, size_t attempt) const = 0;
		virtual std::string getInstanceName(const std::string &desiredName, size_t attempt) const = 0;
	protected:
		std::string m_indentation;
		std::string m_fileHeader;
		std::string m_filenameExtension;
};



class DefaultCodeFormatting : public CodeFormatting
{
	public:
		DefaultCodeFormatting();

		virtual std::string getNodeName(const hlim::BaseNode *node, size_t attempt) const override;
		virtual std::string getSignalName(const std::string &desiredName, SignalType type, size_t attempt) const override;
		virtual std::string getPackageName(const std::string &desiredName, size_t attempt) const override;
		virtual std::string getEntityName(const std::string &desiredName, size_t attempt) const override;
		virtual std::string getBlockName(const std::string &desiredName, size_t attempt) const override;
		virtual std::string getProcessName(const std::string &desiredName, bool clocked, size_t attempt) const override;
		virtual std::string getClockName(const std::string &desiredName, size_t attempt) const override;
		virtual std::string getIoPinName(const std::string &desiredName, size_t attempt) const override;
		virtual std::string getInstanceName(const std::string &desiredName, size_t attempt) const override;

		virtual void formatEntityComment(std::ostream &stream, const std::string &entityName, const std::string &comment) const override;
		virtual void formatBlockComment(std::ostream &stream, const std::string &blockName, const std::string &comment) const override;
		virtual void formatProcessComment(std::ostream &stream, size_t indentation, const std::string &processName, const std::string &comment) const override;
		virtual void formatCodeComment(std::ostream &stream, size_t indentation, const std::string &comment) const override;

		virtual void formatConnectionType(std::ostream &stream, const VHDLSignalDeclaration &declaration) const override;
		virtual void formatDeclaration(std::ostream &stream, const VHDLSignalDeclaration &declaration) const override;
		virtual void formatDataType(std::ostream &stream, VHDLDataType dataType) const override;

		virtual void formatDecimalFlavor(std::ostream &stream, hlim::GenericParameter::DecimalFlavor flavor) const override;
		virtual void formatBitFlavor(std::ostream &stream, hlim::GenericParameter::BitFlavor flavor) const override;
		virtual void formatBitVectorFlavor(std::ostream &stream, hlim::GenericParameter::BitVectorFlavor flavor) const override;
		
		virtual void formatGenericParameterType(std::ostream &stream, const hlim::GenericParameter &param) const override;
};


}
