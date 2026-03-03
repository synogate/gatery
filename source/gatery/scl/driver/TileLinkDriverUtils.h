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

		std::span<uint8_t> data;
	};

	struct TileLinkDData
	{
		uint8_t opcode = 0;
		uint8_t param = 0;
		uint8_t size = 0;
		uint64_t source = 0;
		uint64_t sink = 0;
		bool error = false;

		std::span<uint8_t> data;

		bool operator == (const TileLinkDData& o) const {
			if (opcode != o.opcode ||
				param != o.param ||
				size != o.size ||
				source != o.source ||
				sink != o.sink ||
				error != o.error ||
				data.size() != o.data.size())
				return false;

			for (size_t i = 0; i < data.size(); ++i)
				if (data[i] != o.data[i])
					return false;

			return true;
		};
	};

	struct TileLinkSerializer
	{
		uint8_t addrW;
		uint8_t dataW;
		uint8_t sizeW;
		uint8_t sourceW;
		uint8_t sinkW = 0;

		static TileLinkSerializer fromLink(const auto& link)
		{
			return {
				.addrW = uint8_t(link.a->address.width().bits()),
				.dataW = uint8_t(link.a->data.width().bits()),
				.sizeW = uint8_t(link.a->size.width().bits()),
				.sourceW = uint8_t(link.a->source.width().bits()),
				.sinkW = uint8_t((*link.d)->sink.width().bits()),
			};
		}

		inline std::span<std::uint8_t> serialize(std::span<std::uint8_t> buffer, TileLinkAData req) const
		{
			size_t requiredW = 3 + 3 + sizeW + sourceW + addrW + dataW / 8;
			size_t requiredBytes = (requiredW + 7) / 8 + req.data.size();
			if (buffer.size() < requiredBytes)
				return {};
			buffer = buffer.subspan(0, requiredBytes);
			for (auto& it : buffer)
				it = 0;

			uint8_t offset = 0;
			appendToBuffer(req.opcode, 3, buffer, offset);
			appendToBuffer(req.param, 3, buffer, offset);
			appendToBuffer(req.size, sizeW, buffer, offset);
			appendToBuffer(req.source, sourceW, buffer, offset);
			appendToBuffer(req.address, addrW, buffer, offset);
			appendToBuffer(req.mask, dataW / 8, buffer, offset);
			std::copy(req.data.begin(), req.data.end(), buffer.begin() + (offset + 7) / 8);
			return buffer;
		}

		inline std::span<std::uint8_t> serialize(std::span<std::uint8_t> buffer, TileLinkDData req) const
		{
			size_t requiredW = 3 + 3 + sizeW + sourceW + sinkW + 1;
			size_t requiredBytes = (requiredW + 7) / 8 + req.data.size();
			if (buffer.size() < requiredBytes)
				return {};
			buffer = buffer.subspan(0, requiredBytes);
			for (auto& it : buffer)
				it = 0;

			uint8_t offset = 0;
			appendToBuffer(req.opcode, 3, buffer, offset);
			appendToBuffer(req.param, 3, buffer, offset);
			appendToBuffer(req.size, sizeW, buffer, offset);
			appendToBuffer(req.source, sourceW, buffer, offset);
			appendToBuffer(req.sink, sinkW, buffer, offset);
			appendToBuffer(req.error ? 1 : 0, 1, buffer, offset);
			std::copy(req.data.begin(), req.data.end(), buffer.begin() + (offset + 7) / 8);
			return buffer;
		}

		inline TileLinkAData deserializeA(std::span<std::uint8_t> packet) const
		{
			TileLinkAData ret;

			uint8_t offset = 0;
			ret.opcode = (uint8_t)extractFromBuffer(3, packet, offset);
			ret.param = (uint8_t)extractFromBuffer(3, packet, offset);
			ret.size = (uint8_t)extractFromBuffer(sizeW, packet, offset);
			ret.source = extractFromBuffer(sourceW, packet, offset);
			ret.address = extractFromBuffer(addrW, packet, offset);
			ret.mask = (uint8_t)extractFromBuffer(dataW / 8, packet, offset);

			uint8_t dataByteOffset = (offset + 7) / 8;
			ret.data = packet.subspan(dataByteOffset);
			return ret;
		}

		inline TileLinkDData deserializeD(std::span<std::uint8_t> packet) const
		{
			TileLinkDData ret;

			uint8_t offset = 0;
			ret.opcode = (uint8_t)extractFromBuffer(3, packet, offset);
			ret.param = (uint8_t)extractFromBuffer(3, packet, offset);
			ret.size = (uint8_t)extractFromBuffer(sizeW, packet, offset);
			ret.source = (uint8_t)extractFromBuffer(sourceW, packet, offset);
			ret.sink = (uint8_t)extractFromBuffer(sinkW, packet, offset);
			ret.error = extractFromBuffer(1, packet, offset) != 0;

			uint8_t dataByteOffset = (offset + 7) / 8;
			ret.data = packet.subspan(dataByteOffset);
			return ret;
		}

		inline std::span<std::uint8_t> serializeGet(std::span<std::uint8_t> buffer, uint64_t address, uint8_t logByteSize, uint64_t source = 0)
		{
			return serialize(buffer, { .opcode = 4, .size = logByteSize, .source = source, .address = address, .mask = 1 });
		}

	private:
		static void appendToBuffer(uint64_t value, uint8_t width, std::span<uint8_t> buffer, uint8_t& offset)
		{
			while (width)
			{
				const uint8_t step = std::min<uint8_t>(width, 8 - offset % 8);
				buffer[offset / 8] |= uint8_t(value << (offset % 8));
				offset += step;
				value >>= step;
				width -= step;
			}
		}

		static uint64_t extractFromBuffer(uint8_t width, std::span<const uint8_t> buffer, uint8_t& offset)
		{
			uint64_t ret = 0;
			uint64_t dstOfs = 0;
			while (width && offset / 8 < buffer.size())
			{
				const uint8_t step = std::min<uint8_t>(width, 8 - offset % 8);
				uint64_t chunk = (buffer[offset / 8] >> offset % 8) & ((1 << step) - 1);
				ret |= chunk << dstOfs;
				width -= step;
				dstOfs += step;
				offset += step;
			}
			return ret;
		}
	};
}
