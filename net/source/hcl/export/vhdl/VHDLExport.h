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

        void operator()(const hlim::Circuit &circuit);

        void recordTestbench(sim::Simulator &simulator, const std::string &name);

        void writeGHDLScript(const std::string &name);
    protected:
        std::filesystem::path m_destination;
        std::unique_ptr<CodeFormatting> m_codeFormatting;
        std::optional<TestbenchRecorder> m_testbenchRecorder;
        std::unique_ptr<AST> m_ast;
};

}
