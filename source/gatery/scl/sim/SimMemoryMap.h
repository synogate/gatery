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

#include "../driver/MemoryMapInterface.h"


namespace gtry {
	class Clock;

	namespace sim {
		class Simulator;
	}
}


namespace gtry::scl {
	class TileLinkMasterModel;
}

namespace gtry::scl::driver {

class SimulationFiberMapped32BitTileLink : public MemoryMapInterface {
	public:
		SimulationFiberMapped32BitTileLink(scl::TileLinkMasterModel &linkModel, const Clock &clock);

		virtual uint8_t readU8(size_t addr) const override;
		virtual void writeU8(size_t addr, uint8_t data) override;

		virtual uint16_t readU16(size_t addr) const override;
		virtual void writeU16(size_t addr, uint16_t data) override;

		virtual uint32_t readU32(size_t addr) const override;
		virtual void writeU32(size_t addr, uint32_t data) override;

		virtual uint64_t readU64(size_t addr) const override;
		virtual void writeU64(size_t addr, uint64_t data) override;

		virtual void readBlock(void *dst, size_t addr, size_t size) const override;
		virtual void writeBlock(const void *src, size_t addr, size_t size) override;
	protected:
		scl::TileLinkMasterModel &m_linkModel;
		const Clock &m_clock;
};


class SimulationMapped32BitTileLink : public MemoryMapInterface {
	public:
		SimulationMapped32BitTileLink(scl::TileLinkMasterModel &linkModel, const Clock &clock, gtry::sim::Simulator &simulator);

		virtual uint8_t readU8(size_t addr) const override;
		virtual void writeU8(size_t addr, uint8_t data) override;

		virtual uint16_t readU16(size_t addr) const override;
		virtual void writeU16(size_t addr, uint16_t data) override;

		virtual uint32_t readU32(size_t addr) const override;
		virtual void writeU32(size_t addr, uint32_t data) override;

		virtual uint64_t readU64(size_t addr) const override;
		virtual void writeU64(size_t addr, uint64_t data) override;

		virtual void readBlock(void *dst, size_t addr, size_t size) const override;
		virtual void writeBlock(const void *src, size_t addr, size_t size) override;
	protected:
		scl::TileLinkMasterModel &m_linkModel;
		const Clock &m_clock;
		gtry::sim::Simulator &m_simulator;
};


}
