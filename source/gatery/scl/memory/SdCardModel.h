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
#include <gatery/frontend.h>
#include "StorageDeviceModel.h"
#include "../io/SpiMaster.h"

namespace gtry::scl
{
	class SdCardModel : public SpiDeviceModel
	{
	public:
		SdCardModel& storage(std::unique_ptr<StorageDeviceModel> storage);

		uint8_t data(uint8_t data) override;
		void deselect() override;
		void reset() override;

	protected:
		virtual void onCommand(uint8_t cmd, uint32_t arg, uint8_t crc);
		virtual void dbgMsg(std::string msg, bool warning = false);

		void respond8(uint8_t r);
		void respond32(uint32_t r);

		enum class CardState
		{
			idle,
			transfer
		};

		CardState m_cardState = CardState::transfer; // start in transfer mode to skip lengthy init in simulation
		bool m_hadCmd8 = false;
		bool m_appCmd = false;

		std::vector<uint8_t> m_cmd;
		std::deque<uint8_t> m_response;
		std::function<uint8_t(uint8_t)> m_dataCallback;
		std::unique_ptr<StorageDeviceModel> m_storage;

		std::optional<uint32_t> m_eraseBlockFirst;
		std::optional<uint32_t> m_eraseBlockLast;
	};
}
