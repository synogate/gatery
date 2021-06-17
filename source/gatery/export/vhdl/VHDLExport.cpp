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
#include "VHDLExport.h"

#include "Package.h"
#include "Entity.h"

#include "../../utils/Range.h"
#include "../../utils/Enumerate.h"
#include "../../utils/Exceptions.h"

#include "../../hlim/coreNodes/Node_Arithmetic.h"
#include "../../hlim/coreNodes/Node_Compare.h"
#include "../../hlim/coreNodes/Node_Constant.h"
#include "../../hlim/coreNodes/Node_Logic.h"
#include "../../hlim/coreNodes/Node_Multiplexer.h"
#include "../../hlim/coreNodes/Node_Signal.h"
#include "../../hlim/coreNodes/Node_Register.h"
#include "../../hlim/coreNodes/Node_Rewire.h"


#include "../../simulation/Simulator.h"

#include "../../frontend/SynthesisTool.h"

#include <set>
#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <functional>
#include <list>
#include <map>

namespace gtry::vhdl {


VHDLExport::VHDLExport(std::filesystem::path destination)
{
    m_destination = std::move(destination);
    m_codeFormatting.reset(new DefaultCodeFormatting());
    m_synthesisTool.reset(new DefaultSynthesisTool());
}

VHDLExport::~VHDLExport()
{
}

VHDLExport &VHDLExport::targetSynthesisTool(SynthesisTool *synthesisTool)
{
    m_synthesisTool.reset(synthesisTool);
    return *this;
}


VHDLExport &VHDLExport::setFormatting(CodeFormatting *codeFormatting)
{
    m_codeFormatting.reset(codeFormatting);
    return *this;
}

CodeFormatting *VHDLExport::getFormatting()
{
    return m_codeFormatting.get();
}


void VHDLExport::operator()(const hlim::Circuit &circuit)
{
    m_ast.reset(new AST(m_codeFormatting.get(), m_synthesisTool.get()));
    m_ast->convert((hlim::Circuit &)circuit);
    m_ast->writeVHDL(m_destination);
}

void VHDLExport::recordTestbench(sim::Simulator &simulator, const std::string &name)
{
    m_testbenchRecorder.emplace(*this, m_ast.get(), simulator, m_destination, name);
    simulator.addCallbacks(&*m_testbenchRecorder);
}


    void VHDLExport::writeXdc(std::string_view filename)
    {
        std::fstream file((m_destination / filename).string().c_str(), std::fstream::out);
        file.exceptions(std::fstream::failbit | std::fstream::badbit);

        Entity* top = m_ast->getRootEntity();
        for (hlim::Clock* clk : top->getClocks())
        {
            auto&& name = top->getNamespaceScope().getName(clk);
            hlim::ClockRational freq = clk->getAbsoluteFrequency();
            double ns = double(freq.denominator() * 1'000'000'000) / freq.numerator();

            file << "create_clock -period " << std::fixed << std::setprecision(3) << ns << " [get_ports " << name << "]\n";
        }
    }
    void VHDLExport::writeProjectFile(std::string_view filename)
    {
        m_synthesisTool->writeVhdlProjectScript(*this, filename);
    }
}
