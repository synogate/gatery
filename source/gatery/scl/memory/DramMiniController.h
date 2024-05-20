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
#include <gatery/frontend.h>
#include "SdramCommand.h"
#include "../tilelink/tilelink.h"

namespace gtry::scl::sdram
{
	struct MiniControllerConfig
	{
		BitWidth sourceW = 0_b;
	};

	struct PhyInterface
	{
		CommandBus cmd;

		Bit odt;
		Bit dqWriteValid;
		Bit dqReadValid;

		BVec dqIn; // captured data on dq lines
	};

	scl::TileLinkUL miniController(PhyInterface& dramIo, MiniControllerConfig cfg = {});
	scl::TileLinkUL miniControllerMappedMemory(PhyInterface& dramIo, BitWidth sourceW = 0_b);
	void miniControllerSimulation(PhyInterface& dramIo);

	struct PhyGateMateDDR2Config
	{
		std::string pinPrefix = "ddr2_";
		BitWidth addrW;
		BitWidth dqW;
	};

	PhyInterface phyGateMateDDR2(PhyGateMateDDR2Config cfg);

}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::sdram::MiniControllerConfig, sourceW);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::sdram::PhyInterface, cmd, dqWriteValid, dqReadValid, dqIn);
