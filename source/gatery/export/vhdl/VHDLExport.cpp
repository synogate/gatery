#include "VHDLExport.h"
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
#include "InterfacePackage.h"
#include "Entity.h"

#include "TestbenchRecorder.h"
#include "FileBasedTestbenchRecorder.h"

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

#include "../../hlim/supportNodes/Node_PathAttributes.h"


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
    m_destinationTestbench = getDestination();
    m_codeFormatting.reset(new DefaultCodeFormatting());
    m_synthesisTool.reset(new DefaultSynthesisTool());
}


VHDLExport::VHDLExport(std::filesystem::path destination, std::filesystem::path destinationTestbench)
{
    m_destination = std::move(destination);
    m_destinationTestbench = std::move(destinationTestbench);
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

VHDLExport &VHDLExport::writeClocksFile(std::string filename)
{
    m_clocksFilename = std::move(filename);
    return *this;
}

VHDLExport &VHDLExport::writeConstraintsFile(std::string filename)
{
    m_constraintsFilename = std::move(filename);
    return *this;
}

VHDLExport &VHDLExport::writeProjectFile(std::string filename)
{
    m_projectFilename = std::move(filename);
    return *this;
}

VHDLExport& VHDLExport::writeStandAloneProjectFile(std::string filename)
{
    m_standAloneProjectFilename = std::move(filename);
    return *this;
}




CodeFormatting *VHDLExport::getFormatting()
{
    return m_codeFormatting.get();
}

void VHDLExport::operator()(hlim::Circuit &circuit)
{
    std::filesystem::create_directories(getDestination());
    std::filesystem::create_directories(m_destinationTestbench);

    m_synthesisTool->prepareCircuit(circuit);

    m_ast.reset(new AST(m_codeFormatting.get(), m_synthesisTool.get()));
    if (!m_interfacePackageContent.empty())
        m_ast->generateInterfacePackage(m_interfacePackageContent);

    m_ast->convert((hlim::Circuit &)circuit);
    m_ast->writeVHDL(m_destination);

    for (auto &e : m_testbenchRecorderSettings) {
        if (e.inlineTestData)
            m_testbenchRecorder.push_back(std::make_unique<TestbenchRecorder>(*this, m_ast.get(), *e.simulator, m_destinationTestbench, e.name));
        else
            m_testbenchRecorder.push_back(std::make_unique<FileBasedTestbenchRecorder>(*this, m_ast.get(), *e.simulator, m_destinationTestbench, e.name));
            
        e.simulator->addCallbacks(m_testbenchRecorder.back().get());
    }
    
    if (!m_constraintsFilename.empty())
        m_synthesisTool->writeConstraintFile(*this, circuit, m_constraintsFilename);

    if (!m_clocksFilename.empty())
        m_synthesisTool->writeClocksFile(*this, circuit, m_clocksFilename);

    if (!m_projectFilename.empty())
        m_synthesisTool->writeVhdlProjectScript(*this, m_projectFilename);

    if (!m_standAloneProjectFilename.empty())
        m_synthesisTool->writeStandAloneProject(*this, m_standAloneProjectFilename);
}

bool VHDLExport::isSingleFileExport()
{
    return m_destination.has_extension();
}

std::filesystem::path VHDLExport::getDestination()
{
    if (isSingleFileExport())
    {
        std::filesystem::path dir = m_destination.parent_path();
        if (dir.empty())
            dir = ".";
        return dir;
    }
    return m_destination;
}

void VHDLExport::addTestbenchRecorder(sim::Simulator &simulator, const std::string &name, bool inlineTestData)
{
    m_testbenchRecorderSettings.emplace_back(TestbenchRecorderSettings{&simulator, name, inlineTestData});
}



}



            // create_generated_clock  for derived clocks?

            // "An auto-generated clock is not created if a user-def ined clock (primar y or generated) is also defined on the same netlist object, that is, on the same definition point (net or pin)."

            // CLOCK_DELAY_GROUP

            //set_property CLOCK_DELAY_GROUP my_group [get_nets {clockA, clockB, clockC}]

            // set_false_path -through [get_pins design_1_i/rst_processing_system7_0_100M/U0/ext_reset_in]
            // set_multicycle_path 2 -setup -start -from [get_clocks Cpu_ss_clk_100M] -to [get_clocks cpussclks_coresight_clk_50M]
            // set_multicycle_path 1 -hold -start -from [get_clocks Cpu_ss_clk_100M] -to [get_clocks cpussclks_coresight_clk_50M]

            // set_max_delay between synchronizer regs?

            /*
                Same clock domain or between synchronous clock domains with same period and no phase-shift
                    set_multicycle_path N –setup –from CLK1 –to CLK2
                    set_multicycle_path N-1 –hold –from CLK1 –to CLK2
                    
                Between SLOW-to FAST synchronous clock domains
                    set_multicycle_path N –setup –from CLK1 –to CLK2
                    set_multicycle_path N-1 –hold -end –from CLK1 –to CLK2
                    
                Between FAST-to SLOW synchronous clock domains
                    set_multicycle_path N –setup -start –from CLK1 –to CLK2
                    set_multicycle_path N-1 –hold –from CLK1 –to CLK2
            */


            /*
    	        Vivado:

                    # get net of signal, must be KEEP
                    set net [get_nets some_entity_inst/s_counter[0]] 

                    # get driver pin
                    set pin [get_pin -of_object $net  -filter {DIRECTION == OUT} ] 

                    # get driver (hopefully flip flop)
                    set cell [get_cells -of_object $pin]
                    
                    # set multicycle
                    set_multicycle_path N –setup -start –from $cell –to ????
            */
