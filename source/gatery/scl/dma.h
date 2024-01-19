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
#include <gatery/scl/memoryMap/MemoryMap.h>
#include <gatery/scl/axi/AxiDMA.h>
#include <gatery/scl/tilelink/TileLinkDMA.h>

/**
 * @brief This file contains helpers to set up dma's across different interfaces
*/
namespace gtry::scl {
	void tileLinkToAxiDMA(RvStream<TileLinkStreamFetch::Command>&& fetchCmd, RvStream<AxiToStreamCmd>&& depositCmd, TileLinkUB&& dataSource, Axi4& dataDest);
	void createDma(MemoryMap& map, TileLinkUB&& dataSource, scl::Axi4& dataDest, BitWidth beatsW, size_t bytesPerBurst);
}
