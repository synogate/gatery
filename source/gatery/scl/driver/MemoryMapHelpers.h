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

#include <concepts>

/**
 * @addtogroup gtry_scl_driver
 * @{
 */

namespace gtry::scl::driver {


//template<typename T>
//concept PayloadCallback = requires (T t, MemoryMapInterface &interface__, DynamicMemoryMap<void> map) {
//	{ t(interface__, map) };
//};

template<typename Payload, typename Addr> requires (std::is_trivially_copyable_v<Payload>)
void writeToStream(MemoryMapInterface &interface__, Addr streamLocation, const Payload &payload)
{
	while (interface__.readUInt(streamLocation.template get<"valid">())) ;

	interface__.write(streamLocation.template get<"payload">().addr()/8, payload);

	interface__.writeUInt(streamLocation.template get<"valid">(), 1);
}

template<typename Addr, typename PayloadWriter>
void writeToStreamCallback(MemoryMapInterface &interface__, Addr streamLocation, PayloadWriter payloadWriter)
{
	while (interface__.readUInt(streamLocation.template get<"valid">())) ;

	payloadWriter(interface__, streamLocation.template get<"payload">());

	interface__.writeUInt(streamLocation.template get<"valid">(), 1);
}


template<typename Payload, typename Addr> requires (std::is_trivially_copyable_v<Payload>)
Payload readFromStream(MemoryMapInterface &interface__, Addr streamLocation)
{
	while (!interface__.readUInt(streamLocation.template get<"valid">())) ;

	Payload payload = interface__.read<Payload>(streamLocation.template get<"payload">().addr()/8);

	interface__.writeUInt(streamLocation.template get<"ready">(), 1);

	return payload;
}

template<typename Addr, typename PayloadReader>
void readFromStreamCallback(MemoryMapInterface &interface__, Addr streamLocation, PayloadReader payloadReader)
{
	while (!interface__.readUInt(streamLocation.template get<"valid">())) ;

	payloadReader(interface__, streamLocation.template get<"payload">());

	interface__.writeUInt(streamLocation.template get<"ready">(), 1);
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

template<typename Addr>
void writeToTileLink(MemoryMapInterface &interface__, Addr streamLocation, size_t tilelinkStartAddr, std::span<const std::byte> byteData)
{
	auto payload = streamLocation.template get<"a">().template get<"payload">();
	size_t busWidth = payload.template get<"data">().width() / 8;

	if (tilelinkStartAddr % busWidth != 0)
		throw std::runtime_error("Unaligned writes not implemented yet!");

	if (byteData.size() % busWidth != 0)
		throw std::runtime_error("Partial writes not implemented yet!");

	size_t writeSize = busWidth;
	size_t logSize = 63 - std::countl_zero((std::uint64_t) writeSize);
	size_t writeMask = ~0ull;

	interface__.writeUInt(payload.template get<"opcode">(), (size_t)TileLinkAOpCode::PutFullData);
	interface__.writeUInt(payload.template get<"param">(), 0);
	interface__.writeUInt(payload.template get<"size">(), logSize);
	interface__.writeUInt(payload.template get<"mask">(), writeMask);
	
	for (size_t i = 0; i < byteData.size(); i += busWidth) {
		writeToStreamCallback(interface__, streamLocation.template get<"a">(), [tilelinkStartAddr, busWidth, byteData, i](MemoryMapInterface &interface__, auto payload) {
			interface__.writeUInt(payload.template get<"address">(), tilelinkStartAddr + i);
			interface__.write(payload.template get<"data">().addr()/8, byteData.subspan(i, busWidth));
		});

		// Consume away the acks
		// todo: don't wait for it
		readFromStreamCallback(interface__, streamLocation.template get<"d">(), [tilelinkStartAddr, busWidth, byteData, i](MemoryMapInterface &interface__, auto payload) {
			if (interface__.readUInt(payload.template get<"opcode">()) != (size_t) TileLinkDOpCode::AccessAck)
				throw std::runtime_error((std::string("Expected a AccessAck but got ") + std::to_string(interface__.readUInt(payload.template get<"opcode">()))).c_str());
			if (interface__.readUInt(payload.template get<"error">()) != 0)
				throw std::runtime_error((boost::format("Writing to tilelink address 0x%x produced error %d") % (tilelinkStartAddr + i) % interface__.readUInt(payload.template get<"error">())).str().c_str());
		});
	}
}

template<typename Addr, typename Data> requires (std::is_trivially_copyable_v<Data>)
void writeToTileLink(MemoryMapInterface &interface__, Addr streamLocation, size_t tilelinkStartAddr, const Data &data)
{
	auto byteData = std::as_bytes(std::span{data});
	writeToTileLink(interface__, streamLocation, tilelinkStartAddr, byteData);
}

template<typename Addr>
void readFromTileLink(MemoryMapInterface &interface__, Addr streamLocation, size_t tilelinkStartAddr, std::span<std::byte> byteData)
{
	auto payload = streamLocation.template get<"a">().template get<"payload">();
	size_t busWidth = payload.template get<"data">().width() / 8;

	if (tilelinkStartAddr % busWidth != 0)
		throw std::runtime_error("Unaligned writes not implemented yet!");

	if (byteData.size() % busWidth != 0)
		throw std::runtime_error("Partial writes not implemented yet!");

	size_t readSize = busWidth;
	size_t logSize = 63 - std::countl_zero((std::uint64_t) readSize);
	size_t readMask = ~0ull;
	interface__.writeUInt(payload.template get<"opcode">(), (size_t)TileLinkAOpCode::Get);
	interface__.writeUInt(payload.template get<"param">(), 0);
	interface__.writeUInt(payload.template get<"size">(), logSize);
	interface__.writeUInt(payload.template get<"mask">(), readMask);

	for (size_t i = 0; i < byteData.size(); i += busWidth) {
		writeToStreamCallback(interface__, streamLocation.template get<"a">(), [tilelinkStartAddr, busWidth, byteData, i](MemoryMapInterface &interface__, auto payload) {
			interface__.writeUInt(payload.template get<"address">(), tilelinkStartAddr + i);
		});

		readFromStreamCallback(interface__, streamLocation.template get<"d">(), [tilelinkStartAddr, busWidth, byteData, i](MemoryMapInterface &interface__, auto payload) {
			if (interface__.readUInt(payload.template get<"opcode">()) != (size_t) TileLinkDOpCode::AccessAckData)
				throw std::runtime_error((std::string("Expected a AccessAckData but got ") + std::to_string(interface__.readUInt(payload.template get<"opcode">()))).c_str());
			if (interface__.readUInt(payload.template get<"error">()) != 0)
				throw std::runtime_error((boost::format("Reading from tilelink address 0x%x produced error %d") % (tilelinkStartAddr + i) % interface__.readUInt(payload.template get<"error">())).str().c_str());

			interface__.read(payload.template get<"data">().addr()/8, byteData.subspan(i, busWidth));
		});
	}
}

template<typename Addr, typename Data> requires (std::is_trivially_copyable_v<Data>)
Data readFromTileLink(MemoryMapInterface &interface__, Addr streamLocation, size_t tilelinkStartAddr)
{
	Data result;
	auto byteData = std::as_writable_bytes(std::span{result});
	readFromTileLink(interface__, streamLocation, tilelinkStartAddr, byteData);
	return result;
}




template<typename Addr>
void clearTileLinkDChannel(MemoryMapInterface &interface__, Addr streamLocation)
{
	while (interface__.readUInt(streamLocation.template get<"d">().template get<"valid">()))
		interface__.writeUInt(streamLocation.template get<"d">().template get<"ready">(), 1);
}



}


/**@}*/
