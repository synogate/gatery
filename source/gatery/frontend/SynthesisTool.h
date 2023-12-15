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

#include "../hlim/Attributes.h"

#include <string>
#include <functional>
#include <vector>
#include <filesystem>

namespace gtry::vhdl {
	class VHDLExport;
}

namespace gtry::hlim {
	class Node_PathAttributes;
	class Circuit;
}


namespace gtry {

class SynthesisTool {
	public:
		virtual ~SynthesisTool() = default;

		virtual void prepareCircuit(hlim::Circuit &circuit)  { }

		virtual void resolveAttributes(const hlim::GroupAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs);
		virtual void resolveAttributes(const hlim::RegisterAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs) = 0;
		virtual void resolveAttributes(const hlim::SignalAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs) = 0;
		virtual void resolveAttributes(const hlim::MemoryAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs) = 0;

		virtual void writeConstraintFile(vhdl::VHDLExport &vhdlExport, const hlim::Circuit &circuit, std::string_view filename) = 0;
		virtual void writeClocksFile(vhdl::VHDLExport &vhdlExport, const hlim::Circuit &circuit, std::string_view filename) = 0;
		virtual void writeVhdlProjectScript(vhdl::VHDLExport &vhdlExport, std::string_view filename) = 0;
		virtual void writeStandAloneProject(vhdl::VHDLExport& vhdlExport, std::string_view filename) = 0;

		static std::vector<std::filesystem::path> sourceFiles(vhdl::VHDLExport& vhdlExport, bool synthesis, bool simulation);
protected:
		std::vector<std::string> m_vendors;

		void addUserDefinedAttributes(const hlim::Attributes &attribs, hlim::ResolvedAttributes &resolvedAttribs);
		void writeUserDefinedPathAttributes(std::ostream &stream, const hlim::PathAttributes &attribs, const std::string &start, const std::string &end);

		void forEachPathAttribute(vhdl::VHDLExport &vhdlExport, const hlim::Circuit &circuit, std::function<void(hlim::Node_PathAttributes*, std::string, std::string)> functor);
};


class DefaultSynthesisTool : public SynthesisTool {
	public:
		DefaultSynthesisTool();

		virtual void resolveAttributes(const hlim::RegisterAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs) override;
		virtual void resolveAttributes(const hlim::SignalAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs) override;
		virtual void resolveAttributes(const hlim::MemoryAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs) override;

		virtual void writeConstraintFile(vhdl::VHDLExport &vhdlExport, const hlim::Circuit &circuit, std::string_view filename) override;
		virtual void writeClocksFile(vhdl::VHDLExport &vhdlExport, const hlim::Circuit &circuit, std::string_view filename) override;
		virtual void writeVhdlProjectScript(vhdl::VHDLExport &vhdlExport, std::string_view filename) override;
		virtual void writeStandAloneProject(vhdl::VHDLExport& vhdlExport, std::string_view filename) override;
};

}
