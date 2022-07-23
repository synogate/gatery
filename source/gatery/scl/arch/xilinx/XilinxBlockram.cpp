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

#include "XilinxBlockram.h"
#include "XilinxDevice.h"

#include <gatery/hlim/postprocessing/MemoryDetector.h>
#include <gatery/hlim/supportNodes/Node_MemPort.h>

namespace gtry::scl::arch::xilinx {

XilinxBlockram::XilinxBlockram(const XilinxDevice &xilinxDevice) : m_xilinxDevice(xilinxDevice)
{
	m_desc.sizeCategory = MemoryCapabilities::SizeCategory::MEDIUM;
	m_desc.inputRegs = true;
	m_desc.outputRegs = 0;

	m_desc.supportsDualClock = false; // not yet implemented
	m_desc.supportsPowerOnInitialization = false; // not yet implemented
}

}