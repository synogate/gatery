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
#include <span>

namespace gtry::scl::driver
{
	struct TileLinkAData
	{
		uint8_t opcode = 0;
		uint8_t param = 0;
		uint8_t size = 0;
		uint64_t source = 0;
		uint64_t address = 0;
		uint8_t mask = 0;
		uint64_t data = 0;
	};

	struct TileLinkSerializer
	{
		uint8_t addrW;
		uint8_t dataW;
		uint8_t sizeW;
		uint8_t sourceW;

		static TileLinkSerializer fromLink(const auto& link)
		{
			return {
				.addrW = uint8_t(link.a->address.width().bits()),
				.dataW = uint8_t(link.a->data.width().bits()),
				.sizeW = uint8_t(link.a->size.width().bits()),
				.sourceW = uint8_t(link.a->source.width().bits()),
			};
		}

		inline std::span<std::uint8_t> serialize(std::span<std::uint8_t> buffer, TileLinkAData req) const
		{
			size_t requiredW = 3 + 3 + sizeW + sourceW + addrW + dataW / 8 + dataW;
			size_t requiredBytes = (requiredW + 7) / 8;
			if (buffer.size() < requiredBytes)
				return {};
			buffer = buffer.subspan(0, requiredBytes);
			for (auto& it : buffer)
				it = 0;

			uint8_t offset = 0;
			auto append = [&](uint64_t value, uint8_t width) {
				while (width)
				{
					const uint8_t step = std::min<uint8_t>(width, 8 - offset % 8);
					buffer[offset / 8] |= uint8_t(value << (offset % 8));
					offset += step;
					value >>= step;
					width -= step;
				}
			};

			append(req.opcode, 3);
			append(req.param, 3);
			append(req.size, sizeW);
			append(req.source, sourceW);
			append(req.address, addrW);
			append(req.mask, dataW / 8);
			append(req.data, dataW);
			return buffer;
		}

		inline std::span<std::uint8_t> serializeGet(std::span<std::uint8_t> buffer, uint64_t address, uint8_t logByteSize, uint64_t source = 0)
		{
			return serialize(buffer, { .opcode = 4, .size = logByteSize, .source = source, .address = address, .mask = 1, .data = 0xA5 });
		}

	};



	
}

