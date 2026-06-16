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
#include <gatery/frontend/Bit.h>
#include <gatery/frontend/ExternalModule.h>
#include <gatery/scl/stream/Stream.h>

namespace gtry::scl::arch::colognechip
{
	static constexpr const uint8_t CFG_NONE = 0x00;
	static constexpr const uint8_t CFG_DONE = 0x01;
	static constexpr const uint8_t CFG_STOP = 0x02;
	static constexpr const uint8_t CFG_RECONFIG = 0x04;
	static constexpr const uint8_t CFG_CPE_CFG = 0x08;
	static constexpr const uint8_t CFG_CPE_RESET = 0x10;
	static constexpr const uint8_t CFG_FILL_RAM = 0x20;
	static constexpr const uint8_t CFG_SERDES = 0x40;

	enum class CmdPath
	{
		reset	= 0x01,
		top		= 0x02,
		right	= 0x04,
		forward = 0x08,
		enable	= 0x10,
	};

	struct CmdChgStatus
	{
		uint8_t configurationDone : 1;
		uint8_t stopConfiguration : 1;
		uint8_t reconfigurationEnable : 1;
		uint8_t enableCPEConfiguration : 1;
		uint8_t CPEReset : 1;
		uint8_t fillRAMEnable : 1;
		uint8_t : 0;

		uint8_t configurationMode : 4;
		uint8_t selectConfigurationMode : 1;
		uint8_t : 0;

		uint8_t enableGPIOBankS1 : 1;
		uint8_t enableGPIOBankS2 : 1;
		uint8_t : 1;
		uint8_t enableGPIOBankS3 : 1; // CFG
		uint8_t enableGPIOBankE1 : 1;
		uint8_t enableGPIOBankE2 : 1;
		uint8_t : 0;

		uint8_t enableGPIOBankN1 : 1;
		uint8_t enableGPIOBankN2 : 1;
		uint8_t : 2;
		uint8_t enableGPIOBankW1 : 1;
		uint8_t enableGPIOBankW2 : 1;
		uint8_t : 0;
	};


	class BitstreamWriter
	{
	public:
		BitstreamWriter& operator() (const CmdChgStatus& cmd, uint8_t len = 0);
		BitstreamWriter& operator() (const CmdPath& cmd);

		std::vector<uint8_t> finish();

	protected:
		void write(uint8_t cmd, std::span<const uint8_t> data);

	private:
		static uint16_t crc(uint16_t state, uint8_t data);

		std::vector<uint8_t> m_data;
	};
}
