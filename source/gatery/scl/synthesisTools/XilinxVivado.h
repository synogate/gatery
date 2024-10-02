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

#include <gatery/frontend/SynthesisTool.h>

#include <vector>
#include <string>


namespace gtry {

/**
 * @addtogroup gtry_synthesisTools
 * @{
 */

class XilinxVivado : public SynthesisTool {
	public:
		XilinxVivado();

		virtual void prepareCircuit(hlim::Circuit &circuit) override;

		virtual void resolveAttributes(const hlim::RegisterAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs) override;
		virtual void resolveAttributes(const hlim::SignalAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs) override;
		virtual void resolveAttributes(const hlim::MemoryAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs) override;

		virtual void writeClocksFile(vhdl::VHDLExport &vhdlExport, const hlim::Circuit &circuit, std::string_view filename) override;
		virtual void writeConstraintFile(vhdl::VHDLExport &vhdlExport, const hlim::Circuit &circuit, std::string_view filename) override;
		virtual void writeVhdlProjectScript(vhdl::VHDLExport &vhdlExport, std::string_view filename) override;
		virtual void writeStandAloneProject(vhdl::VHDLExport& vhdlExport, std::string_view filename) override;
};

/**@}*/

}
