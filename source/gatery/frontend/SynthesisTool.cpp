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

#include "SynthesisTool.h"

#include "../export/vhdl/VHDLExport.h"
#include "../export/vhdl/Entity.h"
#include "../export/vhdl/Package.h"
#include "../export/vhdl/BaseGrouping.h"
#include "../export/vhdl/AST.h"

#include "../hlim/supportNodes/Node_PathAttributes.h"

#include <fstream>

namespace gtry {


void SynthesisTool::addUserDefinedAttributes(const hlim::RegisterAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	for (const auto &vendor : m_vendors) {
		auto it = attribs.userDefinedVendorAttributes.find(vendor);
		if (it != attribs.userDefinedVendorAttributes.end()) {
			const auto &userDefinedList = it->second;
			for (const auto &e : userDefinedList)
				resolvedAttribs.insert(e);
		}
	}
}

void SynthesisTool::addUserDefinedAttributes(const hlim::SignalAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	for (const auto &vendor : m_vendors) {
		auto it = attribs.userDefinedVendorAttributes.find(vendor);
		if (it != attribs.userDefinedVendorAttributes.end()) {
			const auto &userDefinedList = it->second;
			for (const auto &e : userDefinedList)
				resolvedAttribs.insert(e);
		}
	}
}

void SynthesisTool::writeUserDefinedPathAttributes(std::fstream &stream, const hlim::PathAttributes &attribs, const std::string &start, const std::string &end)
{
	for (const auto &vendor : m_vendors) {
		auto it = attribs.userDefinedVendorAttributes.find(vendor);
		if (it != attribs.userDefinedVendorAttributes.end()) {
			const auto &userDefinedList = it->second;
			for (const auto &e : userDefinedList) {

				std::string invocation = e.first;
				
				size_t pos;
				while ((pos = invocation.find("$src")) != std::string::npos)
					invocation.replace(pos, 4, start);

				while ((pos = invocation.find("$end")) != std::string::npos)
					invocation.replace(pos, 4, end);

				stream << invocation;
			}
		}
	}
}

void SynthesisTool::forEachPathAttribute(vhdl::VHDLExport &vhdlExport, const hlim::Circuit &circuit, std::function<void(hlim::Node_PathAttributes*, std::string, std::string)> functor)
{
    for (auto &n : circuit.getNodes()) {
        if (auto *pa = dynamic_cast<hlim::Node_PathAttributes*>(n.get())) {
            auto start = pa->getNonSignalDriver(0);
            auto end = pa->getNonSignalDriver(1);

            HCL_ASSERT_HINT(start.node, "Path attribute with unconnected start node");
            HCL_ASSERT_HINT(end.node, "Path attribute with unconnected end node");

            std::vector<vhdl::BaseGrouping*> startNodeReversePath;
            HCL_ASSERT_HINT(vhdlExport.getAST()->findLocalDeclaration(start, startNodeReversePath), "Could not locate path attribute start node or did not result in a signal");

            std::vector<vhdl::BaseGrouping*> endNodeReversePath;
            HCL_ASSERT_HINT(vhdlExport.getAST()->findLocalDeclaration(start, endNodeReversePath), "Could not locate path attribute end node or did not result in a signal");

            auto path2vhdl = [](hlim::NodePort np, const std::vector<vhdl::BaseGrouping*> &revPath)->std::string {
                std::stringstream identifier;
                for (int i = (int)revPath.size()-2; i >= 0; i--) {
                    auto *grouping = revPath[i];
                    identifier << grouping->getInstanceName() << '/';
                }
                identifier << revPath.front()->getNamespaceScope().getName(np);
                return identifier.str();
            };

            std::string startIdentifier = path2vhdl(start, startNodeReversePath);
            std::string endIdentifier = path2vhdl(end, endNodeReversePath);

            functor(pa, std::move(startIdentifier), std::move(endIdentifier));
        }
    }
}



DefaultSynthesisTool::DefaultSynthesisTool()
{
	m_vendors = {
		"all",
	};
}

void DefaultSynthesisTool::resolveAttributes(const hlim::RegisterAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	addUserDefinedAttributes(attribs, resolvedAttribs);
}

void DefaultSynthesisTool::resolveAttributes(const hlim::SignalAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	addUserDefinedAttributes(attribs, resolvedAttribs);
}

void DefaultSynthesisTool::writeConstraintFile(vhdl::VHDLExport &vhdlExport, const hlim::Circuit &circuit, std::string_view filename)
{
	std::fstream file((vhdlExport.getDestination() / filename).string().c_str(), std::fstream::out);
	file.exceptions(std::fstream::failbit | std::fstream::badbit);

	file << "# List of constraints:" << std::endl;

	forEachPathAttribute(vhdlExport, circuit, [&](hlim::Node_PathAttributes* pa, std::string start, std::string end) {
		const auto &startConType = hlim::getOutputConnectionType(pa->getDriver(0));
		const auto &endConType = hlim::getOutputConnectionType(pa->getDriver(1));


		const auto &attribs = pa->getAttribs();
		
		if (attribs.falsePath)
			file << "false path: " << start << " --- " << end << '\n';

		if (attribs.multiCycle)
			file << "multi cycle(" << attribs.multiCycle << "): " << start << " --- " << end << '\n';

		writeUserDefinedPathAttributes(file, attribs, start, end);
	});
}

void DefaultSynthesisTool::writeClocksFile(vhdl::VHDLExport &vhdlExport, const hlim::Circuit &circuit, std::string_view filename)
{
	std::fstream file((vhdlExport.getDestination() / filename).string().c_str(), std::fstream::out);
	file.exceptions(std::fstream::failbit | std::fstream::badbit);

	file << "# List of clocks:" << std::endl;

	vhdl::Entity* top = vhdlExport.getAST()->getRootEntity();
	for (hlim::Clock* clk : top->getClocks()) {
		auto&& name = top->getNamespaceScope().getName(clk);
		hlim::ClockRational freq = clk->getAbsoluteFrequency();
		double ns = double(freq.denominator() * 1'000'000'000) / freq.numerator();

		file << "clock: " << name << " period " << std::fixed << std::setprecision(3) << ns << " ns\n";
	}
}

void DefaultSynthesisTool::writeVhdlProjectScript(vhdl::VHDLExport &vhdlExport, std::string_view filename)
{
	std::fstream file((vhdlExport.getDestination() / filename).string().c_str(), std::fstream::out);
	file.exceptions(std::fstream::failbit | std::fstream::badbit);

	file << "# List of source files in dependency order:" << std::endl;

	std::vector<std::filesystem::path> files;

	for (auto&& package : vhdlExport.getAST()->getPackages())
		files.emplace_back(vhdlExport.getAST()->getFilename("", package->getName()));

	for (auto&& entity : vhdlExport.getAST()->getDependencySortedEntities())
		files.emplace_back(vhdlExport.getAST()->getFilename("", entity->getName()));

	for (auto& f : files)
		file << f.string() << '\n';

	file << "# List of constraints:" << std::endl;
	if (!vhdlExport.getConstraintsFilename().empty())
		file << vhdlExport.getConstraintsFilename() << std::endl;
	if (!vhdlExport.getClocksFilename().empty())
		file << vhdlExport.getClocksFilename() << std::endl;
}



}