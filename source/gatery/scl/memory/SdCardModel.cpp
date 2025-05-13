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
#include "gatery/scl_pch.h"
#include "SdCardModel.h"

using namespace gtry::scl;

SdCardModel& gtry::scl::SdCardModel::storage(std::unique_ptr<StorageDeviceModel> storage)
{
	m_storage = std::move(storage);
	return *this;
}

uint8_t gtry::scl::SdCardModel::data(uint8_t data)
{
	if (m_dataCallback)
		return m_dataCallback(data);

	if(data != 0xFF || !m_cmd.empty())
		m_cmd.push_back(data);

	uint8_t ret = 0xFF;
	if (!m_response.empty())
	{
		ret = m_response.front();
		m_response.pop_front();
	}

	if (m_cmd.size() == 6)
	{
		uint8_t cmd = m_cmd[0];
		uint32_t arg = (m_cmd[1] << 24) | (m_cmd[2] << 16) | (m_cmd[3] << 8) | m_cmd[4];
		uint8_t crc = m_cmd[5];
		m_cmd.clear();
		onCommand(cmd, arg, crc);
	}

	return ret;
}

void SdCardModel::deselect()
{
	m_response.clear();
	m_cmd.clear();
	m_dataCallback = nullptr;
}

void SdCardModel::reset()
{
	deselect();
	m_cardState = CardState::idle;
	m_hadCmd8 = false;
	m_appCmd = false;
}

void gtry::scl::SdCardModel::onCommand(uint8_t cmd, uint32_t arg, uint8_t crc)
{
	std::ostringstream msg;
	msg << "CMD" << (cmd & 0x3F) << " " << std::hex << arg;
	dbgMsg(msg.str());

	if(!m_appCmd)
	{
		switch (cmd & 0x3F)
		{
		case 0:
			reset();
			respond8(1);
			break;
		case 8:
			if (m_cardState != CardState::idle)
			{
				respond8(0x41);
				break;
			}
			m_hadCmd8 = true;
			respond8(1);
			respond32(arg);
			respond8(0);
			break;
		case 9: // SEND_CSD
			// this is the implementation for SPI mode. in SD Mode this command works differently.
			if (m_cardState != CardState::transfer)
			{
				respond8(0x41);
				break;
			}
			respond8(0); // OK
			respond8(0xFE); // data token

			for(size_t i = 0; i < 6; ++i)
				respond8(0);

			respond32(uint32_t(m_storage->numSectors() / 1024 - 1));
			break;
		case 13: // SEND_STATUS
			respond8(m_cardState == CardState::idle ? 1 : 0);
			respond8(0); // R2 in SPI mode
			break;
		case 17: // read block
			if (m_cardState != CardState::transfer)
			{
				respond8(0x41);
				break;
			}
			if (arg >= m_storage->numSectors())
			{
				respond8(0x20); // address error
				break;
			}

			respond8(0); // OK
			respond8(0xFE); // data token
			for (std::byte b : m_storage->sectorRead(arg))
				respond8(uint8_t(b));
			respond8(0xAA); // CRC is bad on some cards in SPI mode
			respond8(0x55);
			break;
		case 24: // write block
			if (m_cardState != CardState::transfer)
			{
				respond8(0x41);
				break;
			}
			if (arg >= m_storage->numSectors())
			{
				respond8(0x20); // address error
				break;
			}

			respond8(0); // OK

			m_dataCallback = [this, arg, dataMark = false, sectorData = std::vector<std::byte>()](uint8_t data) mutable -> uint8_t {
				if (!m_response.empty())
				{
					uint8_t ret = m_response.front();
					m_response.pop_front();
					return ret;
				}

				if (!dataMark)
				{
					if (data == 0xFE)
					{
						dataMark = true;
						return 0xFF;
					}
					return 0xFF;
				}

				sectorData.push_back(std::byte(data));
				if (sectorData.size() == 512)
					m_storage->sectorWrite(arg, sectorData);

				if (sectorData.size() == 512 + 2)
					respond8(5); // accept block

				if (sectorData.size() == 512 + 3)
				{
					m_dataCallback = nullptr;
					return 0; // emulate write busy
				}

				return 0xFF;
			};
			break;
		case 32: // ERASE_WR_BLK_START
			if (m_cardState != CardState::transfer)
			{
				respond8(0x41);
				break;
			}
			if (arg >= m_storage->numSectors())
			{
				respond8(0x20); // address error
				break;
			}

			m_eraseBlockFirst = arg;
			m_eraseBlockLast.reset();
			respond8(0); // OK
			break;

		case 33: // ERASE_WR_BLK_END
			if (m_cardState != CardState::transfer)
			{
				respond8(0x41);
				break;
			}
			if (arg >= m_storage->numSectors())
			{
				respond8(0x20); // address error
				break;
			}
			if (!m_eraseBlockFirst || m_eraseBlockLast)
			{
				respond8(0x10); // erase sequence error
				break;
			}
			if (arg < *m_eraseBlockFirst)
			{
				respond8(0x40); // parameter error
				break;
			}

			m_eraseBlockLast = arg;
			respond8(0); // OK
			break;

		case 38: // ERASE
			if (m_cardState != CardState::transfer)
			{
				respond8(0x41);
				break;
			}
			if (!m_eraseBlockFirst || !m_eraseBlockLast)
			{
				respond8(0x10); // erase sequence error
				break;
			}
			for (uint32_t i = *m_eraseBlockFirst; i <= *m_eraseBlockLast; ++i)
				m_storage->sectorErase(i);
			m_eraseBlockFirst.reset();
			m_eraseBlockLast.reset();
			respond8(0); // OK
			respond32(0); // emulate some erase time
			break;

		case 55:
			m_appCmd = true;
			respond8(1);
			break;
		default:
			respond8(0x41);
			dbgMsg("unhandled command", true);
			break;
		}
	}
	else
	{
		m_appCmd = false;

		switch (cmd & 0x3F)
		{
		case 41:
			if (!m_hadCmd8)
			{
				respond8(0x41);
				dbgMsg("ACMD41 without CMD8 (not supported for SDHC/SDXC)", true);
				break;
			}
			m_cardState = CardState::transfer;
			respond8(0);
			break;
		default:
			respond8(0x41);
			dbgMsg("unhandled app command", true);
			break;
		}
	}
}

void gtry::scl::SdCardModel::dbgMsg(std::string msg, bool warning)
{
	sim::SimulationContext* ctx = sim::SimulationContext::current();
	if (ctx)
	{
		if (warning)
			ctx->onWarning(nullptr, "SDCard: " + move(msg));
		else
			ctx->onDebugMessage(nullptr, "SDCard: " + move(msg));
	}
}

void gtry::scl::SdCardModel::respond8(uint8_t r)
{
	m_response.push_back(r);
}

void gtry::scl::SdCardModel::respond32(uint32_t r)
{
	m_response.push_back(r >> 24);
	m_response.push_back(r >> 16);
	m_response.push_back(r >> 8);
	m_response.push_back(r);
}
