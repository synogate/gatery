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

/*
 * Do not include the regular gatery headers since this is meant to compile stand-alone in driver/userspace application code. 
 */

#include "../MemoryMapInterface.h"

#include <span>

/**
 * @addtogroup gtry_scl_driver
 * @{
 */


namespace gtry::scl::driver::lnx {

class PCIDeviceFunction;

class UserSpaceMapped32BitEndpoint : public MemoryMapInterface {
	public:
		UserSpaceMapped32BitEndpoint(const PCIDeviceFunction &function, size_t size);
		virtual ~UserSpaceMapped32BitEndpoint();

		virtual uint8_t readU8(size_t addr) const override final;
		virtual void writeU8(size_t addr, uint8_t data) override final;

		virtual uint16_t readU16(size_t addr) const override final;
		virtual void writeU16(size_t addr, uint16_t data) override final;

		virtual uint32_t readU32(size_t addr) const override final;
		virtual void writeU32(size_t addr, uint32_t data) override final;

		virtual uint64_t readU64(size_t addr) const override final;
		virtual void writeU64(size_t addr, uint64_t data) override final;

		virtual void readBlock(void *dst, size_t addr, size_t size) const override final;
		virtual void writeBlock(const void *src, size_t addr, size_t size) override final;
	protected:
		std::span<volatile uint32_t> m_mappedRegisters;
};


}

/**@}*/