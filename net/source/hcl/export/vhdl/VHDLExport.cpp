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

#include <set>
#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <functional>
#include <list>
#include <map>

namespace hcl::core::vhdl {


VHDLExport::VHDLExport(std::filesystem::path destination)
{
    m_destination = std::move(destination);
    m_codeFormatting.reset(new DefaultCodeFormatting());
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
    m_ast.reset(new AST(m_codeFormatting.get()));
    m_ast->convert((hlim::Circuit &)circuit);
    m_ast->writeVHDL(m_destination);
}

void VHDLExport::recordTestbench(sim::Simulator &simulator, const std::string &name)
{
    m_testbenchRecorder.emplace(*this, m_ast.get(), simulator, m_destination, name);
    simulator.addCallbacks(&*m_testbenchRecorder);
}

void VHDLExport::writeGHDLScript(const std::string &name)
{
    std::fstream file((m_destination / name).string().c_str(), std::fstream::out);
    file.exceptions(std::fstream::failbit | std::fstream::badbit);

    auto sortedEntites = m_ast->getDependencySortedEntities();

    //file << "#!/bin/sh" << std::endl;
    for (auto &package : m_ast->getPackages())
        file << "ghdl -a --std=08 --ieee=synopsys " << m_ast->getFilename("", package->getName()) << std::endl;;

    for (auto entity : sortedEntites)
        file << "ghdl -a --std=08 --ieee=synopsys " << m_ast->getFilename("", entity->getName()) << std::endl;;

    if (m_testbenchRecorder) {
        file << "ghdl -a --std=08 --ieee=synopsys " << m_ast->getFilename("", m_testbenchRecorder->getName()) << std::endl;;

        file << "ghdl -e --std=08 --ieee=synopsys " << m_testbenchRecorder->getName() << std::endl;
        file << "ghdl -r --std=08 " << m_testbenchRecorder->getName() << " --ieee-asserts=disable --vcd=signals.vcd --wave=signals.ghw" << std::endl;
    }
}

    void VHDLExport::writeVivadoScript(std::string_view filename)
    {
        std::fstream file((m_destination / filename).string().c_str(), std::fstream::out);
        file.exceptions(std::fstream::failbit | std::fstream::badbit);

        std::vector<std::filesystem::path> files;

        for (auto&& package : m_ast->getPackages())
            files.emplace_back(m_ast->getFilename("", package->getName()));

        for (auto&& entity : m_ast->getDependencySortedEntities())
            files.emplace_back(m_ast->getFilename("", entity->getName()));

        for (auto& f : files)
        {
            file << "read_vhdl -vhdl2008 ";
            if (!m_library.empty())
                file << "-library " << m_library << ' ';
            file << f.string() << '\n';
        }

        writeXdc("clocks.xdc");

        file << R"(

read_xdc clocks.xdc

# reset_run synth_1
# launch_runs impl_1

# set run settings -> more options to "-mode out_of_context" for virtual pins

)";
    }

    void VHDLExport::writeXdc(std::string_view filename)
    {
        std::fstream file((m_destination / filename).string().c_str(), std::fstream::out);
        file.exceptions(std::fstream::failbit | std::fstream::badbit);

        Entity* top = m_ast->getRootEntity();
        for (core::hlim::Clock* clk : top->getClocks())
        {
            auto&& name = top->getNamespaceScope().getName(clk);
            core::hlim::ClockRational freq = clk->getAbsoluteFrequency();
            double ns = double(freq.denominator() * 1'000'000'000) / freq.numerator();

            file << "create_clock -period " << std::fixed << std::setprecision(3) << ns << " [get_ports " << name << "]\n";
        }
    }
}
