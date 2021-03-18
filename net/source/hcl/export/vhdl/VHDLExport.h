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
