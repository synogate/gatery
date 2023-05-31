/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

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
#include <gatery/scl/tilelink/tilelink.h>
#include <gatery/scl/Avalon.h>


namespace gtry::scl {

	/**
	 * @brief Translates an Avalon Slave into a TileLink Slave (to be connected to a tilelink master)
	 * @details The bridge strips and stores source/txids from requests and reattaches them to responses.
	 * The bridge can be backpressured but never propagates backpressure to the Avalon Slave. 
	 * Instead, it buffers responses in an internal fifo, the capacity of which can be controlled by 
	 * meta data in amm.
	 */
	TileLinkUL tileLinkBridge(AvalonMM& amm, BitWidth sourceW);

}


