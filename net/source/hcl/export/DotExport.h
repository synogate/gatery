#pragma once

#include "../hlim/Circuit.h"

#include <filesystem>

namespace hcl::core {

/**
 * @brief Export circuit into a .dot file for visualization
 */
class DotExport
{
    public:
        /**
         * @brief Construct with default settings, which can be overriden before invoking the export
         * @param destination Path to the resulting .dot file.
         */
        DotExport(std::filesystem::path destination);

        /**
         * @brief Invokes the export
         * @param circuit The circuit to export into the .dot file.
         */
        void operator()(const hlim::Circuit &circuit);

        /**
         * @brief Executes graphviz on the .dot file to produce an svg.
         * @details Requires prior invocation of the export.
         * @param destination Path to the resulting .svg file.
         */
        void runGraphViz(std::filesystem::path destination);
    protected:
        std::filesystem::path m_destination;
};

void visualize(const hlim::Circuit &circuit, const std::string &filename);

}
