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

#include "MemoryBuffer.h"
#include "DMADeviceMemoryBuffer.h"
#include "../MemoryMapHelpers.h"

#include <memory>

#include <cstddef>
#include <span>
#include <vector>
#include <cstdint>

/**
 * @addtogroup gtry_scl_driver
 * @{
 */

namespace gtry::scl::driver {

	template<IsStaticMemoryMapEntryHandle Addr>
	class DMAFetchDepositToAxi : public DeviceDMAController {
		public:
			DMAFetchDepositToAxi(Addr addr, MemoryMapInterface &interface_, std::uint64_t beatSize) : m_beatSize(beatSize), m_interface(interface_) { 
				m_canDownload = false;

				auto axiReport = addr.template get<"axiReport">();
				m_bitsPerBurst = m_interface.readUInt(axiReport.template get<"bitsPerBurst">());
			}

			virtual void uploadContinuousChunk(PhysicalAddr hostAddr, PhysicalAddr deviceAddr, size_t size) override {
				if (size == 0) return;

				Addr addr;
				auto axiReport = addr.template get<"axiReport">();
				auto depositCmdStream = addr.template get<"depositCmd">();
				auto fetchCmdStream = addr.template get<"fetchCmd">();

				if (size % m_beatSize != 0)
					throw std::runtime_error("Transfer size must be a multiple of the beat size!");

				size_t beatCmdWidth = fetchCmdStream.template get<"payload">().template get<"beats">().width();
				if (beatCmdWidth < 64)
					if (size / m_beatSize >= (1ull << beatCmdWidth))
						throw std::runtime_error("Transfer size exceeds hardware capabilities!");

				std::uint64_t burstsBefore = m_interface.readUInt(axiReport.template get<"burstCount">());
				std::uint64_t numBursts = (size * 8 + m_bitsPerBurst-1) / m_bitsPerBurst;
				std::uint64_t expectedBurstsAfter = burstsBefore + numBursts;
				if (axiReport.template get<"burstCount">().width() < 64)
					expectedBurstsAfter %= 1ull << axiReport.template get<"burstCount">().width();

				writeToStream(m_interface, depositCmdStream, [&](MemoryMapInterface &interface_, auto payload) {
					interface_.writeUInt(payload.template get<"startAddress">(), deviceAddr);
					interface_.writeUInt(payload.template get<"endAddress">(), deviceAddr + size);
				});

				writeToStream(m_interface, fetchCmdStream, [&](MemoryMapInterface &interface_, auto payload) {
					interface_.writeUInt(payload.template get<"address">(), hostAddr);
					interface_.writeUInt(payload.template get<"beats">(), size / m_beatSize);
				});

				while (m_interface.readUInt(axiReport.template get<"burstCount">()) != expectedBurstsAfter) ;
			}

			virtual void downloadContinuousChunk(PhysicalAddr hostAddr, PhysicalAddr deviceAddr, size_t size) const override {
				throw std::runtime_error("Downloading not possible!");
			}

		protected:
			std::uint64_t m_beatSize;
			size_t m_bitsPerBurst;
			MemoryMapInterface &m_interface;
	};


}

/**@}*/