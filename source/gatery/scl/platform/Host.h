/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2024 Michael Offel, Andreas Ley

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

#include <gatery/scl/driver/MemoryMapEntry.h>
#include <gatery/scl/driver/MemoryMap.h>
#include <gatery/scl/driver/MemoryMapInterface.h>
#include <gatery/scl/memoryMap/PackedMemoryMap.h>
#include <gatery/scl/tilelink/TileLinkMasterModel.h>
#include <gatery/scl/sim/PcieHostModel.h>

#include <gatery/hlim/postprocessing/MemoryStorage.h>


#include <list>
#include <tuple>
#include <vector>
#include <memory>
#include <optional>

namespace gtry::scl
{
	class Host
	{
		public:
			virtual ~Host() = default;

			virtual scl::pci::RequesterInterface addHostMemory(BitWidth tlpStreamW);
			virtual hlim::MemoryStorage &simuHostMemory();

			virtual std::tuple<FlatAddressSpaceDescription, AddressSpaceDescriptionHandle, std::unique_ptr<driver::MemoryMapInterface>> addMemoryMap(scl::PackedMemoryMap &memoryMap);
		protected:
			virtual void instantiateHostMemory();
			virtual void exposeMemoryMapTL(scl::TileLinkUL &cpuPort);

			std::list<scl::TileLinkMasterModel> m_memoryMapTLModels;
			std::optional<scl::sim::PcieHostModel> m_exposedHostMemory;
	};


}
