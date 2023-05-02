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
#pragma once

#include <filesystem>

namespace gtry::hlim {

class Circuit;
class NodeGroup;
class Subnet;
class ConstSubnet;
class SignalDelay;

}

namespace gtry {

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
		void operator()(const hlim::Circuit &circuit, hlim::NodeGroup *nodeGroup = nullptr);
		
		void operator()(const hlim::Circuit &circuit, const hlim::ConstSubnet &subnet);

		void operator()(const hlim::Circuit &circuit, const hlim::ConstSubnet &subnet, const hlim::SignalDelay &signalDelays);

		void mergeCombinatoryNodes() { m_mergeCombinatoryNodes = true; }

		/**
		 * @brief Executes graphviz on the .dot file to produce an svg.
		 * @details Requires prior invocation of the export.
		 * @param destination Path to the resulting .svg file.
		 */
		void runGraphViz(std::filesystem::path destination);
	protected:
		std::filesystem::path m_destination;
		bool m_mergeCombinatoryNodes = false;

		void writeDotFile(const hlim::Circuit &circuit, const hlim::ConstSubnet &subnet, hlim::NodeGroup *nodeGroup, const hlim::SignalDelay *signalDelays);
		void writeMergedDotFile(const hlim::Circuit &circuit, const hlim::ConstSubnet &subnet);

};

void visualize(const hlim::Circuit &circuit, const std::string &filename, hlim::NodeGroup *nodeGroup = nullptr);
void visualize(const hlim::Circuit &circuit, const std::string &filename, const hlim::ConstSubnet &subnet);

}
