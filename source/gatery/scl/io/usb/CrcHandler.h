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
#include "Phy.h"

namespace gtry::scl::usb
{
	struct CrcHandler
	{
		PhyTxStream tx;
		PhyRxStream rx;

		void checkRxAppendTx(PhyTxStream& tx, const PhyRxStream& rx);
		void checkRx(const PhyRxStream& rx);
		void appendTx(PhyTxStream& tx);

	};

	class CombinedBitCrc
	{
	public:
		enum Mode {
			crc5, crc16
		};

		CombinedBitCrc(Bit in, Enum<Mode> mode, Bit reset, Bit shiftOut);

		const Bit& out() const { return m_out; }
		const Bit& match() const { return m_match; } // depending on mode
		const Bit& match5() const { return m_match5; }
		const Bit& match16() const { return m_match16; }

	private:
		Area m_ent = Area{ "CombinedBitCrc", true };
		UInt m_state = 16_b;
		Bit m_match5;
		Bit m_match16;
		Bit m_match;
		Bit m_out;
	};
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::usb::CrcHandler, tx, rx);
