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

#include "MemoryMapInterface.h"
#include "MemoryMap.h"

#include <boost/format.hpp>
#include <bit>

#include <concepts>
#include <bit>


/**
 * @addtogroup gtry_scl_driver
 * @{
 */

namespace gtry::scl::driver {


template<typename T>
concept PayloadCallback = requires (T t, MemoryMapInterface &interface_, DynamicMemoryMap<void> map) {
	{ t(interface_, map) };
};

template<typename Payload, IsStaticMemoryMapEntryHandle Addr> requires (std::is_trivially_copyable_v<Payload> && !PayloadCallback<Payload>)
void writeToStream(MemoryMapInterface &interface_, Addr streamLocation, const Payload &payload, bool assumeNoBackpressure = false)
{
	if (!assumeNoBackpressure)
		while (interface_.readUInt(streamLocation.template get<"valid">())) ;

	interface_.write(streamLocation.template get<"payload">().addr()/8, payload);

	interface_.writeUInt(streamLocation.template get<"valid">(), 1);
}

template<IsStaticMemoryMapEntryHandle Addr, PayloadCallback PayloadWriter>
void writeToStream(MemoryMapInterface &interface_, Addr streamLocation, PayloadWriter payloadWriter, bool assumeNoBackpressure = false)
{
	if (!assumeNoBackpressure)
		while (interface_.readUInt(streamLocation.template get<"valid">())) ;

	payloadWriter(interface_, streamLocation.template get<"payload">());

	interface_.writeUInt(streamLocation.template get<"valid">(), 1);
}


template<typename Payload, IsStaticMemoryMapEntryHandle Addr> requires (std::is_trivially_copyable_v<Payload> && !PayloadCallback<Payload>)
Payload readFromStream(MemoryMapInterface &interface_, Addr streamLocation, bool assumeValid = false)
{
	if (!assumeValid)
		while (!interface_.readUInt(streamLocation.template get<"valid">())) ;

	Payload payload = interface_.read<Payload>(streamLocation.template get<"payload">().addr()/8);

	interface_.writeUInt(streamLocation.template get<"ready">(), 1);

	return payload;
}

template<IsStaticMemoryMapEntryHandle Addr, PayloadCallback PayloadReader>
void readFromStream(MemoryMapInterface &interface_, Addr streamLocation, PayloadReader payloadReader, bool assumeValid = false)
{
	if (!assumeValid)
		while (!interface_.readUInt(streamLocation.template get<"valid">())) ;

	payloadReader(interface_, streamLocation.template get<"payload">());

	interface_.writeUInt(streamLocation.template get<"ready">(), 1);
}




enum class TileLinkAOpCode {
	PutFullData = 0,	// UL
	PutPartialData = 1,	// UL
	ArithmeticData = 2,	// UH
	LogicalData = 3,	// UH
	Get = 4,			// UL
	Intent = 5,			// UH
	Acquire = 6,		// C
};

enum class TileLinkDOpCode {
	AccessAck = 0,		// UL
	AccessAckData = 1,	// UL
	HintAck = 2,		// UH
	Grant = 4,			// C
	GrantData = 5,		// C
	ReleaseAck = 6		// C
};

//#define CHECK_TILELINK_RESULTS

template<IsStaticMemoryMapEntryHandle Addr>
void writeToTileLink(MemoryMapInterface &interface_, Addr streamLocation, size_t tilelinkStartAddr, std::span<const std::byte> byteData, bool assumeNoBackpressure = false)
{
	if (byteData.empty()) return;

	auto payload = streamLocation.template get<"a">().template get<"payload">();
	size_t busWidth = payload.template get<"data">().width() / 8;

	if (tilelinkStartAddr % busWidth != 0)
		throw std::runtime_error("Unaligned writes not implemented yet!");

	if (byteData.size() % busWidth != 0)
		throw std::runtime_error("Partial writes not implemented yet!");

	size_t writeSize = busWidth;
	size_t logSize = 63 - std::countl_zero((std::uint64_t) writeSize);
	size_t writeMask = ~0ull;

	interface_.writeUInt(payload.template get<"opcode">(), (size_t)TileLinkAOpCode::PutFullData);
	interface_.writeUInt(payload.template get<"param">(), 0);
	interface_.writeUInt(payload.template get<"size">(), logSize);
	interface_.writeUInt(payload.template get<"mask">(), writeMask);
	
	for (size_t i = 0; i < byteData.size(); i += busWidth) {
		writeToStream(interface_, streamLocation.template get<"a">(), [tilelinkStartAddr, busWidth, byteData, i](MemoryMapInterface &interface_, auto payload) {
			interface_.writeUInt(payload.template get<"address">(), tilelinkStartAddr + i);
			interface_.write(payload.template get<"data">().addr()/8, byteData.subspan(i, busWidth));
		}, assumeNoBackpressure);

		// Consume away the acks
		// todo: don't wait for it
		readFromStream(interface_, streamLocation.template get<"d">(), [tilelinkStartAddr, busWidth, byteData, i](MemoryMapInterface &interface_, auto payload) {
#ifdef CHECK_TILELINK_RESULTS
			if (interface_.readUInt(payload.template get<"opcode">()) != (size_t) TileLinkDOpCode::AccessAck)
				throw std::runtime_error((std::string("Expected a AccessAck but got ") + std::to_string(interface_.readUInt(payload.template get<"opcode">()))).c_str());
			if (interface_.readUInt(payload.template get<"error">()) != 0)
				throw std::runtime_error((boost::format("Writing to tilelink address 0x%x produced error %d") % (tilelinkStartAddr + i) % interface_.readUInt(payload.template get<"error">())).str().c_str());
#endif
		});
	}
}

template<IsStaticMemoryMapEntryHandle Addr, typename Data> requires (std::is_trivially_copyable_v<Data>)
void writeToTileLink(MemoryMapInterface &interface_, Addr streamLocation, size_t tilelinkStartAddr, const Data &data, bool assumeNoBackpressure = false)
{
	auto byteData = std::as_bytes(std::span{data});
	writeToTileLink(interface_, streamLocation, tilelinkStartAddr, byteData, assumeNoBackpressure);
}

template<IsStaticMemoryMapEntryHandle Addr>
void readFromTileLink(MemoryMapInterface &interface_, Addr streamLocation, size_t tilelinkStartAddr, std::span<std::byte> byteData, bool assumeNoBackpressure = false)
{
	if (byteData.empty()) return;

	auto payload = streamLocation.template get<"a">().template get<"payload">();
	size_t busWidth = payload.template get<"data">().width() / 8;

	if (tilelinkStartAddr % busWidth != 0)
		throw std::runtime_error("Unaligned writes not implemented yet!");

	if (byteData.size() % busWidth != 0)
		throw std::runtime_error("Partial writes not implemented yet!");

	size_t readSize = busWidth;
	size_t logSize = 63 - std::countl_zero((std::uint64_t) readSize);
	size_t readMask = ~0ull;
	interface_.writeUInt(payload.template get<"opcode">(), (size_t)TileLinkAOpCode::Get);
	interface_.writeUInt(payload.template get<"param">(), 0);
	interface_.writeUInt(payload.template get<"size">(), logSize);
	interface_.writeUInt(payload.template get<"mask">(), readMask);

	for (size_t i = 0; i < byteData.size(); i += busWidth) {
		writeToStream(interface_, streamLocation.template get<"a">(), [tilelinkStartAddr, busWidth, byteData, i](MemoryMapInterface &interface_, auto payload) {
			interface_.writeUInt(payload.template get<"address">(), tilelinkStartAddr + i);
		}, assumeNoBackpressure);

		readFromStream(interface_, streamLocation.template get<"d">(), [tilelinkStartAddr, busWidth, byteData, i](MemoryMapInterface &interface_, auto payload) {
#ifdef CHECK_TILELINK_RESULTS
			if (interface_.readUInt(payload.template get<"opcode">()) != (size_t) TileLinkDOpCode::AccessAckData)
				throw std::runtime_error((std::string("Expected a AccessAckData but got ") + std::to_string(interface_.readUInt(payload.template get<"opcode">()))).c_str());
			if (interface_.readUInt(payload.template get<"error">()) != 0)
				throw std::runtime_error((boost::format("Reading from tilelink address 0x%x produced error %d") % (tilelinkStartAddr + i) % interface_.readUInt(payload.template get<"error">())).str().c_str());
#endif
			interface_.read(payload.template get<"data">().addr()/8, byteData.subspan(i, busWidth));
		});
	}
}

template<IsStaticMemoryMapEntryHandle Addr, typename Data> requires (std::is_trivially_copyable_v<Data>)
Data readFromTileLink(MemoryMapInterface &interface_, Addr streamLocation, size_t tilelinkStartAddr, bool assumeNoBackpressure = false)
{
	Data result;
	auto byteData = std::as_writable_bytes(std::span{result});
	readFromTileLink(interface_, streamLocation, tilelinkStartAddr, byteData, assumeNoBackpressure);
	return result;
}


template<IsStaticMemoryMapEntryHandle Addr>
void clearTileLinkDChannel(MemoryMapInterface &interface_, Addr streamLocation)
{
	while (interface_.readUInt(streamLocation.template get<"d">().template get<"valid">()))
		interface_.writeUInt(streamLocation.template get<"d">().template get<"ready">(), 1);
}

}

/**@}*/
