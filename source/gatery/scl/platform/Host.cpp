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

#include <gatery/scl_pch.h>

#include "Host.h"

#include <gatery/scl/io/pci/PciToTileLink.h>
#include <gatery/scl/memoryMap/TileLinkMemoryMap.h>
#include <gatery/scl/sim/SimMemoryMap.h>

namespace gtry::scl
{


scl::pci::RequesterInterface Host::addHostMemory(BitWidth tlpStreamW)
{
	instantiateHostMemory();
	return m_exposedHostMemory->requesterInterface(tlpStreamW); //4 bits are enough to hold the number 10, which is the required logByteSize for 1kiB burst transfer
}

hlim::MemoryStorage &Host::simuHostMemory()
{
	HCL_DESIGNCHECK_HINT(m_exposedHostMemory, "No host memory was added!");
	return m_exposedHostMemory->memory();
}

void Host::instantiateHostMemory()
{
	if (!m_exposedHostMemory) {
		m_exposedHostMemory.emplace(std::optional<sim::RandomBlockDefinition>{}, 1ull << 48);
		m_exposedHostMemory->defaultHandlers();
		m_exposedHostMemory->updateHandler(scl::pci::TlpOpcode::memoryReadRequest64bit, std::make_unique<scl::sim::CompleterInChunks>(64, 2));

		auto clk = ClockScope::getClk();
		DesignScope::get()->getCircuit().addSimulationProcess([=,this]()->SimProcess { return m_exposedHostMemory->completeRequests(clk, 2); });
	}
}

std::tuple<FlatAddressSpaceDescription, AddressSpaceDescriptionHandle, std::unique_ptr<driver::MemoryMapInterface>> Host::addMemoryMap(scl::PackedMemoryMap &memoryMap)
{
	auto fromMemoryMap = scl::toTileLinkUL(memoryMap, 32_b, width(scl::pci::TlpAnswerInfo{}));

	scl::TileLinkUL cpuPort = constructFrom(*fromMemoryMap);
	HCL_NAMED(cpuPort);

	m_memoryMapTLModels.emplace_back();
	scl::TileLinkMasterModel &linkModel = m_memoryMapTLModels.back();
	linkModel.init("cpuBus", 
		fromMemoryMap->a->address.width(), 
		fromMemoryMap->a->data.width(),
		fromMemoryMap->a->size.width(), 
		fromMemoryMap->a->source.width());

	scl::TileLinkUL fromSimulation = constructFrom((scl::TileLinkUL&)linkModel.getLink());
	fromSimulation <<= (scl::TileLinkUL&)linkModel.getLink();

	HCL_NAMED(fromSimulation);

	{
		Area area("cpuBusSimulationOverride", true);

		fromMemoryMap->a <<= simOverrideDownstream<scl::TileLinkChannelA>(move(cpuPort.a), move(fromSimulation.a));

		auto [newD, newSimulationD] = simOverrideUpstream<scl::TileLinkChannelD>(move(*fromMemoryMap->d));
		*cpuPort.d <<= newD;
		*fromSimulation.d <<= newSimulationD;
	}

	exposeMemoryMapTL(cpuPort);

	auto [flatDesc, desc] = gtry::scl::exportAddressSpaceDescription(fromMemoryMap->addrSpaceDesc);

	return std::make_tuple(
			flatDesc, desc, 
			std::make_unique<scl::driver::SimulationFiberMapped32BitTileLink>(linkModel, ClockScope::getClk()));
}

void Host::exposeMemoryMapTL(scl::TileLinkUL &cpuPort)
{
	pinIn(cpuPort, "cpu_mm_tl");
}


}
