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
 * Do not include the regular gatery stuff since this is meant to compile stand-alone in driver/userspace application code. 
 */

#include "MemoryMap.h"

#include <stdint.h>
#include <stddef.h>
#include <span>

/**
 * @addtogroup gtry_scl_driver
 * @{
 */

namespace gtry::scl::driver {

class MemoryMapInterface {
	public:
		virtual ~MemoryMapInterface() = default;

		template<IsStaticMemoryMapEntryHandle Addr>
		inline uint64_t readUInt(Addr addr) const;

		template<IsStaticMemoryMapEntryHandle Addr>
		inline void writeUInt(Addr addr, uint64_t data);

		virtual uint8_t readU8(size_t addr) const { uint8_t v; readBlock(&v, addr, sizeof(v)); return v; }
		virtual void writeU8(size_t addr, uint8_t data) { writeBlock(&data, addr, sizeof(data)); }

		virtual uint16_t readU16(size_t addr) const { uint16_t v; readBlock(&v, addr, sizeof(v)); return v; }
		virtual void writeU16(size_t addr, uint16_t data) { writeBlock(&data, addr, sizeof(data)); }

		virtual uint32_t readU32(size_t addr) const { uint32_t v; readBlock(&v, addr, sizeof(v)); return v; }
		virtual void writeU32(size_t addr, uint32_t data) { writeBlock(&data, addr, sizeof(data)); }

		virtual uint64_t readU64(size_t addr) const { uint64_t v; readBlock(&v, addr, sizeof(v)); return v; }
		virtual void writeU64(size_t addr, uint64_t data) { writeBlock(&data, addr, sizeof(data)); }

		virtual void readBlock(void *dst, size_t addr, size_t size) const = 0;
		virtual void writeBlock(const void *src, size_t addr, size_t size) = 0;

		inline void readBlock(size_t addr, std::span<uint8_t> dst) const { readBlock(dst.data(), addr, dst.size()); }
		inline void writeBlock(size_t addr, std::span<const uint8_t> src) { writeBlock(src.data(), addr, src.size()); }
		inline void readBlock(size_t addr, std::span<std::byte> dst) const { readBlock(dst.data(), addr, dst.size()); }
		inline void writeBlock(size_t addr, std::span<const std::byte> src) { writeBlock(src.data(), addr, src.size()); }

		template<typename T> requires (std::is_trivially_copyable_v<T>)
		inline T read(size_t addr) const { T result; readBlock(&result, addr, sizeof(result)); return result; }

		template<typename T> requires (std::is_trivially_copyable_v<T>)
		inline void write(size_t addr, const T &src) { writeBlock(&src, addr, sizeof(src)); }

		template<typename T> requires (std::is_trivially_copyable_v<T>)
		inline void read(size_t addr, std::span<T> dst) { readBlock(dst.data(), addr, dst.size() * sizeof(T)); }

		template<typename T> requires (std::is_trivially_copyable_v<T>)
		inline void write(size_t addr, std::span<const T> src) { writeBlock(src.data(), addr, src.size() * sizeof(T)); }

};

template<IsStaticMemoryMapEntryHandle Addr>
uint64_t MemoryMapInterface::readUInt(Addr addr) const
{
	if (sizeof(size_t) < addr.width()/8)
		throw std::runtime_error("Field too large!");
	if (addr.width() <= 8) return readU8(addr.addr()/8);
	if (addr.width() <= 16) return readU16(addr.addr()/8);
	if (addr.width() <= 32) return readU32(addr.addr()/8);
	if (addr.width() <= 64) return readU64(addr.addr()/8);
	return ~0ull;
}

template<IsStaticMemoryMapEntryHandle Addr>
void MemoryMapInterface::writeUInt(Addr addr, uint64_t data)
{
	if (sizeof(size_t) < addr.width()/8)
		throw std::runtime_error("Field too large!");
	if (addr.width() <= 8) { writeU8(addr.addr()/8, (uint8_t) data); return; }
	if (addr.width() <= 16) { writeU16(addr.addr()/8, (uint16_t) data); return; }
	if (addr.width() <= 32) { writeU32(addr.addr()/8, (uint32_t) data); return; }
	if (addr.width() <= 64) { writeU64(addr.addr()/8, (uint64_t) data); return; }
}



}

/**@}*/
