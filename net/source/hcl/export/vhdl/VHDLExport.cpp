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

    file << "#!/bin/sh" << std::endl;
    for (auto &package : m_ast->getPackages())
        file << "ghdl -a --std=08 --ieee=synopsys " << m_ast->getFilename("", package->getName()) << std::endl;;

    for (auto entity : sortedEntites)
        file << "ghdl -a --std=08 --ieee=synopsys " << m_ast->getFilename("", entity->getName()) << std::endl;;

    if (m_testbenchRecorder) {
        file << "ghdl -a --std=08 --ieee=synopsys " << m_ast->getFilename("", m_testbenchRecorder->getName()) << std::endl;;

        file << "ghdl -e --std=08 --ieee=synopsys " << m_testbenchRecorder->getName() << std::endl;
        file << "ghdl -r " << m_testbenchRecorder->getName() << " --vcd=signals.vcd --wave=signals.ghw" << std::endl;
    }
}


}
