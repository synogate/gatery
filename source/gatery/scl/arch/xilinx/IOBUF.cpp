/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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
#include "gatery/scl_pch.h"
#include "IOBUF.h"

#include <gatery/hlim/coreNodes/Node_Register.h>
#include <gatery/hlim/coreNodes/Node_Clk2Signal.h>
#include <gatery/hlim/coreNodes/Node_ClkRst2Signal.h>
#include <gatery/hlim/NodeGroup.h>

#include <gatery/scl/io/ddr.h>

namespace gtry::scl::arch::xilinx {

IOBUF::IOBUF()
{
	m_libraryName = "UNISIM";
	m_packageName = "VCOMPONENTS";
	m_name = "IOBUF";

	resizeIOPorts(IN_COUNT, OUT_COUNT);

	declInputBit(IN_I, "I");
	declInputBit(IN_T, "T");
	declInputBit(IN_IO_I, "IO");

	declOutputBit(OUT_O, "O");
	declOutputBit(OUT_IO_O, "IO");

	declBidirPort(IN_IO_I, OUT_IO_O);
}

void IOBUF::setDriveStrength(DriveStrength driveStrength)
{
	switch (driveStrength) {
		case DS_2mA: m_genericParameters["DRIVE"] = 2; break;
		case DS_4mA: m_genericParameters["DRIVE"] = 4; break;
		case DS_6mA: m_genericParameters["DRIVE"] = 6; break;
		case DS_8mA: m_genericParameters["DRIVE"] = 8; break;
		case DS_12mA: m_genericParameters["DRIVE"] = 12; break;
		case DS_16mA: m_genericParameters["DRIVE"] = 16; break;
		case DS_24mA: m_genericParameters["DRIVE"] = 24; break;
	}
}

std::unique_ptr<hlim::BaseNode> IOBUF::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> res(new IOBUF());
	copyBaseToClone(res.get());

	return res;
}




}
