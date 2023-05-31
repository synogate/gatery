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
#include "CodeFormatting.h"

#include "../../utils/Range.h"

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

namespace gtry::vhdl {


void CodeFormatting::indent(std::ostream &stream, size_t depth) const
{
	for ([[maybe_unused]] auto i : utils::Range(depth))
		stream << m_indentation;
}

DefaultCodeFormatting::DefaultCodeFormatting()
{
	m_indentation = "	";
	m_fileHeader =
R"Delim(
--------------------------------------------------------------------
-- This file was auto generated from Gatery. DO NOT MODIFY. Any
-- changes made directly can not be brought back into the source
-- material and will be lost uppon regeneration.
--------------------------------------------------------------------
)Delim";

	m_filenameExtension = ".vhd";
}

std::string DefaultCodeFormatting::getNodeName(const hlim::BaseNode *node, size_t attempt) const
{
	std::string initialName = node->getName();
	if (initialName.empty())
		initialName = "unnamed";
	if (attempt == 0)
		return initialName;

	return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}

std::string DefaultCodeFormatting::getSignalName(const std::string &desiredName, SignalType type, size_t attempt) const
{
	std::string initialName = desiredName;
	if (initialName.empty())
		initialName = "unnamed";

	switch (type) {
		case SIG_ENTITY_INPUT: initialName = std::string("in_") + initialName; break;
		case SIG_ENTITY_OUTPUT: initialName = std::string("out_") + initialName; break;
		case SIG_CHILD_ENTITY_INPUT: initialName = std::string("c_in_") + initialName; break;
		case SIG_CHILD_ENTITY_OUTPUT: initialName = std::string("c_out_") + initialName; break;
		case SIG_REGISTER_INPUT: initialName = std::string("r_in_") + initialName; break;
		case SIG_REGISTER_OUTPUT: initialName = std::string("r_out_") + initialName; break;
		case SIG_LOCAL_SIGNAL: initialName = std::string("s_") + initialName; break;
		case SIG_LOCAL_VARIABLE: initialName = std::string("v_") + initialName; break;
		case SIG_CONSTANT: boost::to_upper(initialName); initialName = std::string("C_") + initialName; break;
	}

	if (attempt == 0)
		return initialName;

	return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}

std::string DefaultCodeFormatting::getPackageName(const std::string &desiredName, size_t attempt) const
{
	std::string initialName = desiredName;
	if (initialName.empty())
		initialName = "UnnamedPackage";
	if (attempt == 0)
		return initialName;

	return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}

std::string DefaultCodeFormatting::getEntityName(const std::string &desiredName, size_t attempt) const
{
	std::string initialName = desiredName;
	if (initialName.empty())
		initialName = "UnnamedEntity";
	if (attempt == 0)
		return initialName;

	return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}

std::string DefaultCodeFormatting::getBlockName(const std::string &desiredName, size_t attempt) const
{
	std::string initialName = desiredName;
	if (initialName.empty())
		initialName = "unnamedBlock";
	if (attempt == 0)
		return initialName;

	return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}

std::string DefaultCodeFormatting::getProcessName(const std::string &desiredName, bool clocked, size_t attempt) const
{
	std::string initialName = desiredName;
	if (initialName.empty())
		initialName = "unnamedProcess";
	if (attempt == 0)
		return initialName + (clocked?"_reg":"_comb");

	return (boost::format("%s_%d%s") % initialName % (attempt+1) % (clocked?"_reg":"_comb")).str();
}

std::string DefaultCodeFormatting::getClockName(const std::string &desiredName, size_t attempt) const
{
	std::string initialName = desiredName;
	if (initialName.empty())
		initialName = "unnamedClock";
	if (attempt == 0)
		return initialName;

	return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}

std::string DefaultCodeFormatting::getIoPinName(const std::string &desiredName, size_t attempt) const
{
	std::string initialName = desiredName;
	if (initialName.empty())
		initialName = "unnamedIoPin";
	if (attempt == 0)
		return initialName;

	return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}

std::string DefaultCodeFormatting::getInstanceName(const std::string &desiredName, size_t attempt) const
{
	std::string initialName = desiredName;
	if (initialName.empty())
		initialName = "unnamedInstance";
	if (attempt == 0)
		return initialName;

	return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}


void DefaultCodeFormatting::formatEntityComment(std::ostream &stream, const std::string &entityName, const std::string &comment) const
{
	stream
		<< "------------------------------------------------" << std::endl
		<< "--  Entity: " << entityName << std::endl
		<< "-- ";
	for (char c : comment) {
		switch (c) {
			case '\n':
				stream << std::endl << "-- ";
			break;
			case '\r':
			break;
			default:
				stream << c;
			break;
		}
	}
	stream << std::endl
		   << "------------------------------------------------" << std::endl << std::endl;
}

void DefaultCodeFormatting::formatBlockComment(std::ostream &stream, const std::string &blockName, const std::string &comment) const
{
	if (comment.empty()) return;
	indent(stream, 1);
	stream
		<< "------------------------------------------------" << std::endl;
	indent(stream, 1);
	stream
		<< "-- ";
	for (char c : comment) {
		switch (c) {
			case '\n':
				stream << std::endl;
				indent(stream, 1);
				stream
					<< "-- ";
			break;
			case '\r':
			break;
			default:
				stream << c;
			break;
		}
	}
	stream << std::endl;
	indent(stream, 1);
	stream
		<< "------------------------------------------------" << std::endl;
}

void DefaultCodeFormatting::formatProcessComment(std::ostream &stream, size_t indentation, const std::string &processName, const std::string &comment) const
{
	if (comment.empty()) return;
	indent(stream, indentation);
	stream
		<< "-- ";
	for (char c : comment) {
		switch (c) {
			case '\n':
				stream << std::endl;
				indent(stream, indentation);
				stream
					<< "-- ";
			break;
			case '\r':
			break;
			default:
				stream << c;
			break;
		}
	}
	stream << std::endl;
}

void DefaultCodeFormatting::formatCodeComment(std::ostream &stream, size_t indentation, const std::string &comment) const
{
	if (comment.empty()) return;

	bool insertHeader = true;
	for (char c : comment) {
		switch (c) {
			case '\n':
				insertHeader = true;
			break;
			case '\r':
			break;
			default:
				if (insertHeader) {
					stream << std::endl;
					indent(stream, indentation);
					stream << "-- ";
					insertHeader = false;
				}
				stream << c;
			break;
		}
	}
	stream << std::endl;
}


void DefaultCodeFormatting::formatConnectionType(std::ostream &stream, const VHDLSignalDeclaration &declaration) const
{
	formatDataType(stream, declaration.dataType);
	switch (declaration.dataType) {
		case VHDLDataType::BOOL:
		case VHDLDataType::BIT:
		case VHDLDataType::STD_LOGIC:
		case VHDLDataType::STD_ULOGIC:
		case VHDLDataType::VL_LOGIC:
		break;

		case VHDLDataType::VL_LOGIC_VECTOR:
		case VHDLDataType::BIT_VECTOR:
		case VHDLDataType::STD_LOGIC_VECTOR:
		case VHDLDataType::STD_ULOGIC_VECTOR:
		case VHDLDataType::UNSIGNED:
			if (declaration.width == 0)
				stream << "(-1 downto 0)";
			else
				stream << "("<<declaration.width-1 << " downto 0)";
		break;
	}
}

void DefaultCodeFormatting::formatDeclaration(std::ostream &stream, const VHDLSignalDeclaration &declaration) const
{
	stream << declaration.name << " : ";
	formatConnectionType(stream, declaration);
}

void DefaultCodeFormatting::formatDataType(std::ostream &stream, VHDLDataType dataType) const
{
	switch (dataType) {
		case VHDLDataType::BOOL:
			stream << "BOOLEAN";
		break;
		case VHDLDataType::BIT:
			stream << "BIT";
		break;
		case VHDLDataType::BIT_VECTOR:
			stream << "BIT_VECTOR";
		break;
		case VHDLDataType::STD_LOGIC:
			stream << "STD_LOGIC";
		break;
		case VHDLDataType::STD_ULOGIC:
			stream << "STD_ULOGIC";
		break;
		case VHDLDataType::STD_LOGIC_VECTOR:
			stream << "STD_LOGIC_VECTOR";
		break;
		case VHDLDataType::STD_ULOGIC_VECTOR:
			stream << "STD_ULOGIC_VECTOR";
		break;
		case VHDLDataType::UNSIGNED:
			stream << "UNSIGNED";
		break;
		default:
			HCL_ASSERT_HINT(false, "UNHANDLED_DATA_TYPE");
	}
}

void DefaultCodeFormatting::formatDataTypeConversion(std::ostream &stream, VHDLDataType srcDataType, VHDLDataType dstDataType, std::string_view srcIdentifier) const
{
	switch (dstDataType) {
		case VHDLDataType::BOOL:
			switch (srcDataType) {
				case VHDLDataType::BOOL:
					stream << srcIdentifier;
				break;
				default:
					HCL_ASSERT_HINT(false, "Invalid conversion");
			}
		break;
		case VHDLDataType::BIT:
			switch (srcDataType) {
				case VHDLDataType::BIT:
					stream << srcIdentifier;
				break;
				case VHDLDataType::STD_LOGIC:
				case VHDLDataType::STD_ULOGIC:
					stream << "PORTMAP_TO_BIT(" << srcIdentifier << ')';
				break;
				case VHDLDataType::VL_LOGIC:
					stream << srcIdentifier;
				break;
				default:
					HCL_ASSERT_HINT(false, "Invalid conversion");
			}
		break;
		case VHDLDataType::BIT_VECTOR:
			switch (srcDataType) {
				case VHDLDataType::BIT_VECTOR:
					stream << srcIdentifier;
				break;
				case VHDLDataType::STD_LOGIC_VECTOR:
				case VHDLDataType::STD_ULOGIC_VECTOR:
					stream << "TO_BITVECTOR(" << srcIdentifier << ')';
				break;
				case VHDLDataType::UNSIGNED:
					stream << "TO_BITVECTOR(STD_LOGIC_VECTOR(" << srcIdentifier << "))";
				break;
				case VHDLDataType::VL_LOGIC:
					stream << srcIdentifier;
				break;
				default:
					HCL_ASSERT_HINT(false, "Invalid conversion");
			}
		break;
		case VHDLDataType::VL_LOGIC:
			switch (srcDataType) {
				case VHDLDataType::VL_LOGIC:
					stream << srcIdentifier;
				break;
				case VHDLDataType::BIT:
					stream << srcIdentifier;
				break;
				case VHDLDataType::STD_LOGIC:
				case VHDLDataType::STD_ULOGIC:
					stream << srcIdentifier;
					stream << srcIdentifier;
				break;
				default:
					HCL_ASSERT_HINT(false, "Invalid conversion");
			}
		break;
		case VHDLDataType::STD_LOGIC:
			switch (srcDataType) {
				case VHDLDataType::STD_LOGIC:
					stream << srcIdentifier;
				break;
				case VHDLDataType::BIT:
					stream << "PORTMAP_TO_STDLOGIC(" << srcIdentifier << ')';
				break;
				case VHDLDataType::STD_ULOGIC:
					stream << "STD_LOGIC(" << srcIdentifier << ')';
				break;
				case VHDLDataType::VL_LOGIC:
					stream << srcIdentifier;
				break;
				default:
					HCL_ASSERT_HINT(false, "Invalid conversion");
			}
		break;
		case VHDLDataType::STD_ULOGIC:
			switch (srcDataType) {
				case VHDLDataType::STD_ULOGIC:
					stream << srcIdentifier;
				break;
				case VHDLDataType::BIT:
					stream << "PORTMAP_TO_STDULOGIC(" << srcIdentifier << ')';
				break;
				case VHDLDataType::STD_LOGIC:
					stream << "STD_ULOGIC(" << srcIdentifier << ')';
				break;
				case VHDLDataType::VL_LOGIC:
					stream << srcIdentifier;
				break;
				default:
					HCL_ASSERT_HINT(false, "Invalid conversion");
			}
		break;
		case VHDLDataType::VL_LOGIC_VECTOR:
			switch (srcDataType) {
				case VHDLDataType::VL_LOGIC_VECTOR:
					stream << srcIdentifier;
				break;
				case VHDLDataType::BIT_VECTOR:
					stream << srcIdentifier;
				break;
				case VHDLDataType::STD_ULOGIC_VECTOR:
				case VHDLDataType::STD_LOGIC_VECTOR:
					stream << srcIdentifier;
				break;
				case VHDLDataType::UNSIGNED:
					stream << "STD_LOGIC_VECTOR(" << srcIdentifier << ")";
				break;
				default:
					HCL_ASSERT_HINT(false, "Invalid conversion");
			}
		break;
		case VHDLDataType::STD_LOGIC_VECTOR:
			switch (srcDataType) {
				case VHDLDataType::STD_LOGIC_VECTOR:
					stream << srcIdentifier;
				break;
				case VHDLDataType::BIT_VECTOR:
					stream << "TO_STDLOGICVECTOR(" << srcIdentifier << ')';
				break;
				case VHDLDataType::STD_ULOGIC_VECTOR:
				case VHDLDataType::UNSIGNED:
					stream << "STD_LOGIC_VECTOR(" << srcIdentifier << ')';
				break;
				case VHDLDataType::VL_LOGIC_VECTOR:
					stream << srcIdentifier;
				break;
				default:
					HCL_ASSERT_HINT(false, "Invalid conversion");
			}
		break;
		case VHDLDataType::STD_ULOGIC_VECTOR:
			switch (srcDataType) {
				case VHDLDataType::STD_ULOGIC_VECTOR:
					stream << srcIdentifier;
				break;
				case VHDLDataType::BIT_VECTOR:
					stream << "TO_STDULOGICVECTOR(STD_LOGIC_VECTOR(" << srcIdentifier << ")";
				break;
				case VHDLDataType::UNSIGNED:
					stream << "STD_ULOGIC_VECTOR(" << srcIdentifier << ')';
				break;
				case VHDLDataType::VL_LOGIC_VECTOR:
					stream << srcIdentifier;
				break;
				default:
					HCL_ASSERT_HINT(false, "Invalid conversion");
			}
		break;
		case VHDLDataType::UNSIGNED:
			switch (srcDataType) {
				case VHDLDataType::UNSIGNED:
					stream << srcIdentifier;
				break;
				case VHDLDataType::BIT_VECTOR:
					stream << "PORTMAP_TO_UNSIGNED(" << srcIdentifier << ')';
				break;
				case VHDLDataType::STD_LOGIC_VECTOR:
				case VHDLDataType::STD_ULOGIC_VECTOR:
					stream << "UNSIGNED(" << srcIdentifier << ')';
				break;
				case VHDLDataType::VL_LOGIC_VECTOR:
					stream<< srcIdentifier;
				break;
				default:
					HCL_ASSERT_HINT(false, "Invalid conversion");
			}
		break;
		default:
			HCL_ASSERT_HINT(false, "UNHANDLED_DATA_TYPE");
	}
}

void DefaultCodeFormatting::formatDecimalFlavor(std::ostream &stream, hlim::GenericParameter::DecimalFlavor flavor) const
{
	switch (flavor) {
		case hlim::GenericParameter::DecimalFlavor::INTEGER: stream << "INTEGER"; break;
		case hlim::GenericParameter::DecimalFlavor::NATURAL: stream << "NATURAL"; break;
		case hlim::GenericParameter::DecimalFlavor::POSITIVE: stream << "POSITIVE"; break;
		default: HCL_ASSERT_HINT(false, "Unhandled case!");
	}
}

void DefaultCodeFormatting::formatBitFlavor(std::ostream &stream, hlim::GenericParameter::BitFlavor flavor) const
{
	switch (flavor) {
		case hlim::GenericParameter::BitFlavor::BIT: stream << "BIT"; break;
		case hlim::GenericParameter::BitFlavor::STD_LOGIC: stream << "STD_LOGIC"; break;
		case hlim::GenericParameter::BitFlavor::STD_ULOGIC: stream << "STD_ULOGIC"; break;
		default: HCL_ASSERT_HINT(false, "Unhandled case!");
	}
}

void DefaultCodeFormatting::formatBitVectorFlavor(std::ostream &stream, hlim::GenericParameter::BitFlavor flavor) const
{
	switch (flavor) {
		case hlim::GenericParameter::BitFlavor::BIT: stream << "BIT_VECTOR"; break;
		case hlim::GenericParameter::BitFlavor::STD_LOGIC: stream << "STD_LOGIC_VECTOR"; break;
		case hlim::GenericParameter::BitFlavor::STD_ULOGIC: stream << "STD_ULOGIC_VECTOR"; break;
		default: HCL_ASSERT_HINT(false, "Unhandled case!");
	}
}

void DefaultCodeFormatting::formatGenericParameterType(std::ostream &stream, const hlim::GenericParameter &param) const
{
	if (param.isDecimal()) formatDecimalFlavor(stream, param.decimalFlavor());
	else if (param.isReal()) stream << "REAL";
	else if (param.isString()) stream << "STRING";
	else if (param.isBoolean()) stream << "BOOLEAN";
	else if (param.isBit()) formatBitFlavor(stream, param.bitFlavor());
	else if (param.isBitVector()) formatBitVectorFlavor(stream, param.bitFlavor());
	else HCL_ASSERT_HINT(false, "Unhandled case!");
}


/*
void DefaultCodeFormatting::addExternalNodeHandler(ExternalNodeHandler nodeHandler)
{
	m_externalNodeHandlers.push_back(std::move(nodeHandler));
}

void DefaultCodeFormatting::instantiateExternal(std::ostream &stream, const hlim::Node_External *node, size_t indent, const std::vector<std::string> &inputSignalNames, const std::vector<std::string> &outputSignalNames, const std::vector<std::string> &clockNames) const
{
	for (const auto &handler : m_externalNodeHandlers)
		if (handler(this, stream, node, indent, inputSignalNames, outputSignalNames, clockNames))
			return;
}
*/

}
