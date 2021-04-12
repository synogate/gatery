/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

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

#include "CodeFormatting.h"

#include "TestbenchRecorder.h"
#include "AST.h"

#include "../../hlim/Circuit.h"

#include <filesystem>
#include <memory>
#include <optional>

namespace hcl::core::sim {
    class Simulator;
}

namespace hcl::core::vhdl {

/**
 * @todo write docs
 */
class VHDLExport
{
    public:
        VHDLExport(std::filesystem::path destination);

        VHDLExport &setFormatting(CodeFormatting *codeFormatting);
        CodeFormatting *getFormatting();

        VHDLExport& setLibrary(std::string name) { m_library = std::move(name); return *this; }
        std::string_view getName() const { return m_library; }

        void operator()(const hlim::Circuit &circuit);

        void recordTestbench(sim::Simulator &simulator, const std::string &name);

        void writeGHDLScript(const std::string &name);
        void writeVivadoScript(std::string_view filename);
        void writeXdc(std::string_view filename);
    protected:
        std::filesystem::path m_destination;
        std::unique_ptr<CodeFormatting> m_codeFormatting;
        std::optional<TestbenchRecorder> m_testbenchRecorder;
        std::unique_ptr<AST> m_ast;
        std::string m_library;
};

}
