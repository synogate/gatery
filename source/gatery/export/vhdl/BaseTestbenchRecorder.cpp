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

#include "BaseTestbenchRecorder.h"

#include "AST.h"
#include "Entity.h"

#include "../../simulation/Simulator.h"
#include "../../hlim/coreNodes/Node_Pin.h"

namespace gtry::vhdl {

BaseTestbenchRecorder::BaseTestbenchRecorder(AST *ast, sim::Simulator &simulator, std::string name) : m_ast(ast), m_simulator(simulator), m_name(std::move(name))
{
}

BaseTestbenchRecorder::~BaseTestbenchRecorder()
{
}

void BaseTestbenchRecorder::declareSignals(std::ostream &stream, const std::set<hlim::Clock*> &allClocks, const std::set<hlim::Clock*> &allResets, const std::set<hlim::Node_Pin*> &allIOPins)
{
    auto *rootEntity = m_ast->getRootEntity();
    CodeFormatting &cf = m_ast->getCodeFormatting();

    for (auto clock : allClocks) {
        stream << "    SIGNAL " << rootEntity->getNamespaceScope().getName(clock) << " : STD_LOGIC;" << std::endl;
    }
    for (auto clock : allResets) {
        stream << "    SIGNAL " << rootEntity->getNamespaceScope().getResetName(clock) << " : STD_LOGIC;" << std::endl;
    }

    for (auto ioPin : allIOPins) {
        const std::string &name = rootEntity->getNamespaceScope().getName(ioPin);
        auto conType = ioPin->getConnectionType();

        stream << "    SIGNAL " << name << " : ";
        cf.formatConnectionType(stream, conType);
        stream << ';' << std::endl;
    }
}

void BaseTestbenchRecorder::writePortmap(std::ostream &stream, const std::set<hlim::Clock*> &allClocks, const std::set<hlim::Clock*> &allResets, const std::set<hlim::Node_Pin*> &allIOPins)
{
    auto *rootEntity = m_ast->getRootEntity();
    CodeFormatting &cf = m_ast->getCodeFormatting();

    std::vector<std::string> portmapList;

    for (auto &s : allClocks) {
        std::stringstream line;
        line << rootEntity->getNamespaceScope().getName(s) << " => ";
        line << rootEntity->getNamespaceScope().getName(s);
        portmapList.push_back(line.str());
    }
    for (auto &s : allResets) {
        std::stringstream line;
        line << rootEntity->getNamespaceScope().getResetName(s) << " => ";
        line << rootEntity->getNamespaceScope().getResetName(s);
        portmapList.push_back(line.str());
    }
    for (auto &s : allIOPins) {
        std::stringstream line;
        line << rootEntity->getNamespaceScope().getName(s) << " => ";
        line << rootEntity->getNamespaceScope().getName(s);
        portmapList.push_back(line.str());
    }

    for (auto i : utils::Range(portmapList.size())) {
        cf.indent(stream, 2);
        stream << portmapList[i];
        if (i+1 < portmapList.size())
            stream << ",";
        stream << std::endl;
    }

}

void BaseTestbenchRecorder::initClocks(std::ostream &stream, const std::set<hlim::Clock*> &allClocks, const std::set<hlim::Clock*> &allResets)
{
    auto *rootEntity = m_ast->getRootEntity();
    CodeFormatting &cf = m_ast->getCodeFormatting();

    for (auto &s : allClocks) {
        auto val = m_simulator.getValueOfClock(s);
        cf.indent(stream, 2);
        stream << rootEntity->getNamespaceScope().getName(s) << " <= ";

        if (!val[sim::DefaultConfig::DEFINED])
            stream << "'X';" << std::endl;
        else
            if (val[sim::DefaultConfig::VALUE])
                stream << "'1';" << std::endl;
            else
                stream << "'0';" << std::endl;
    }

    for (auto &s : allResets) {
        auto val = m_simulator.getValueOfReset(s);
        cf.indent(stream, 2);
        stream << rootEntity->getNamespaceScope().getResetName(s) << " <= ";

        if (!val[sim::DefaultConfig::DEFINED])
            stream << "'X';" << std::endl;
        else
            if (val[sim::DefaultConfig::VALUE])
                stream << "'1';" << std::endl;
            else
                stream << "'0';" << std::endl;
    }

}



}
