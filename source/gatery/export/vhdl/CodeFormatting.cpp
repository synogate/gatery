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


void CodeFormatting::indent(std::ostream &stream, unsigned depth) const
{
    for ([[maybe_unused]] auto i : utils::Range(depth))
        stream << m_indentation;
}

DefaultCodeFormatting::DefaultCodeFormatting()
{
    m_indentation = "    ";
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

std::string DefaultCodeFormatting::getNodeName(const hlim::BaseNode *node, unsigned attempt) const
{
    std::string initialName = node->getName();
    if (initialName.empty())
        initialName = "unnamed";
    if (attempt == 0)
        return initialName;

    return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}

std::string DefaultCodeFormatting::getSignalName(const std::string &desiredName, SignalType type, unsigned attempt) const
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

std::string DefaultCodeFormatting::getPackageName(const std::string &desiredName, unsigned attempt) const
{
    std::string initialName = desiredName;
    if (initialName.empty())
        initialName = "UnnamedPackage";
    if (attempt == 0)
        return initialName;

    return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}

std::string DefaultCodeFormatting::getEntityName(const std::string &desiredName, unsigned attempt) const
{
    std::string initialName = desiredName;
    if (initialName.empty())
        initialName = "UnnamedEntity";
    if (attempt == 0)
        return initialName;

    return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}

std::string DefaultCodeFormatting::getBlockName(const std::string &desiredName, unsigned attempt) const
{
    std::string initialName = desiredName;
    if (initialName.empty())
        initialName = "unnamedBlock";
    if (attempt == 0)
        return initialName;

    return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}

std::string DefaultCodeFormatting::getProcessName(const std::string &desiredName, bool clocked, unsigned attempt) const
{
    std::string initialName = desiredName;
    if (initialName.empty())
        initialName = "unnamedProcess";
    if (attempt == 0)
        return initialName + (clocked?"_reg":"_comb");

    return (boost::format("%s_%d%s") % initialName % (attempt+1) % (clocked?"_reg":"_comb")).str();
}

std::string DefaultCodeFormatting::getClockName(const std::string &desiredName, unsigned attempt) const
{
    std::string initialName = desiredName;
    if (initialName.empty())
        initialName = "unnamedClock";
    if (attempt == 0)
        return initialName;

    return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}

std::string DefaultCodeFormatting::getIoPinName(const std::string &desiredName, unsigned attempt) const
{
    std::string initialName = desiredName;
    if (initialName.empty())
        initialName = "unnamedIoPin";
    if (attempt == 0)
        return initialName;

    return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}

std::string DefaultCodeFormatting::getInstanceName(const std::string &desiredName, unsigned attempt) const
{
    std::string initialName = desiredName;
    if (initialName.empty())
        initialName = "unnamedInstance";
    if (attempt == 0)
        return initialName;

    return (boost::format("%s_%d") % initialName % (attempt+1)).str();
}


void DefaultCodeFormatting::formatEntityComment(std::ostream &stream, const std::string &entityName, const std::string &comment)
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

void DefaultCodeFormatting::formatBlockComment(std::ostream &stream, const std::string &blockName, const std::string &comment)
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

void DefaultCodeFormatting::formatProcessComment(std::ostream &stream, unsigned indentation, const std::string &processName, const std::string &comment)
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

void DefaultCodeFormatting::formatCodeComment(std::ostream &stream, unsigned indentation, const std::string &comment)
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


void DefaultCodeFormatting::formatConnectionType(std::ostream &stream, const hlim::ConnectionType &connectionType, bool useSLV)
{
    switch (connectionType.interpretation) {
        case hlim::ConnectionType::BOOL:
            stream << "STD_LOGIC";
        break;
        case hlim::ConnectionType::BITVEC:
            if (useSLV) {
                stream << "STD_LOGIC_VECTOR";
            } else {
                stream << "UNSIGNED";
            }
            if (connectionType.width == 0)
                stream << "(-1 downto 0)";
            else
                stream << "("<<connectionType.width-1 << " downto 0)";
        break;
        default:
            stream << "UNHANDLED_DATA_TYPE";
    };
}


/*
void DefaultCodeFormatting::addExternalNodeHandler(ExternalNodeHandler nodeHandler)
{
    m_externalNodeHandlers.push_back(std::move(nodeHandler));
}

void DefaultCodeFormatting::instantiateExternal(std::ostream &stream, const hlim::Node_External *node, unsigned indent, const std::vector<std::string> &inputSignalNames, const std::vector<std::string> &outputSignalNames, const std::vector<std::string> &clockNames) const
{
    for (const auto &handler : m_externalNodeHandlers)
        if (handler(this, stream, node, indent, inputSignalNames, outputSignalNames, clockNames))
            return;
}
*/

}
