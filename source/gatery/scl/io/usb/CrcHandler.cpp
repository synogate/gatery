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
#include "CrcHandler.h"

#include "../../crc.h"

void gtry::scl::usb::CrcHandler::checkRxAppendTx(PhyTxStream& _tx, const PhyRxStream& _rx)
{
	auto scope = Area{ "CrcHandlerCheckRxAppendTx" }.enter();

	appendTx(_tx);
	checkRx(_rx);
}

void gtry::scl::usb::CrcHandler::checkRx(const PhyRxStream& _rx)
{
	rx = _rx;

	scl::CrcState crc5{
		.params = scl::CrcParams::init(scl::CrcWellKnownParams::CRC_5_USB)
	};
	crc5.remainder = 5_b;
	crc5 = gtry::reg(crc5);

	scl::CrcState crc16{
		.params = scl::CrcParams::init(scl::CrcWellKnownParams::CRC_16_USB)
	};
	crc16.remainder = 16_b;
	crc16 = gtry::reg(crc16);

	Bit isToken, isData;
	isToken = reg(isToken);
	isData = reg(isData);
	HCL_NAMED(isToken);
	HCL_NAMED(isData);

	IF(rx.eop)
	{
		UInt sum5 = crc5.checksum();
		HCL_NAMED(sum5);
		IF(isToken & sum5 != "5b11001")
			rx.error = '1';

		UInt sum16 = crc16.checksum();
		HCL_NAMED(sum16);
		IF(isData & sum16 != "x4FFE")
			rx.error = '1';
		
		isToken = '0';
		isData = '0';
	}

	IF(rx.valid)
	{
		IF(rx.sop)
		{
			crc5.init();
			crc16.init();

			isToken = rx.data.lower(2_b) == 1;
			isData = rx.data.lower(2_b) == 3;

			IF(rx.data.lower(4_b) != ~rx.data.upper(4_b))
				rx.error = '1'; // PID is transfered twice for error checking
		}
		ELSE // PID (first byte) is not part of crc
		{
			crc5.update(rx.data);
			crc16.update(rx.data);
		}
	}


	// remember errors until eop
	Bit rxError;
	rxError = reg(rxError, '0');
	IF(rxError)
		rx.error = '1';
	IF(rx.valid & rx.error)
		rxError = '1';
	IF(rx.eop)
		rxError = '0';
}

void gtry::scl::usb::CrcHandler::appendTx(PhyTxStream& _tx)
{
	auto scope = Area{ "CrcHandlerAppendTx" }.enter();

	scl::CrcState crc16{
		.params = scl::CrcParams::init(scl::CrcWellKnownParams::CRC_16_USB)
	};
	crc16.remainder = 16_b;
	crc16 = gtry::reg(crc16);
	UInt checksum = crc16.checksum();
	HCL_NAMED(checksum);
	setName(tx, "tx0");

	enum class TxState
	{
		waitSop,
		data,
		checksum_low,
		checksum_high,
	};
	Reg<Enum<TxState>> state{ TxState::waitSop };
	state.setName("state");

	IF(state.current() == TxState::waitSop)
	{
		IF(tx.ready & tx.valid)
		{
			IF(tx.data.lower(2_b) == 3) // DATAx pid
				state = TxState::data;
			crc16.init();
		}
	}

	IF(state.current() == TxState::data)
	{
		IF(tx.valid & tx.ready)
			crc16.update(tx.data);

		IF(!tx.valid)
		{
			tx.valid = '1';
			tx.data = checksum.lower(8_b);

			state = TxState::checksum_low;
			IF(tx.ready)
				state = TxState::checksum_high;
		}
	}

	IF(state.current() == TxState::checksum_low)
	{
		tx.valid = '1';
		tx.data = checksum.lower(8_b);

		IF(tx.ready)
			state = TxState::checksum_high;
	}

	IF(state.current() == TxState::checksum_high)
	{
		tx.valid = '1';
		tx.data = checksum.upper(8_b);

		IF(tx.ready)
			state = TxState::waitSop;
	}
	setName(tx, "tx1");

	// add register stage
	tx.ready = _tx.ready | !_tx.valid;
	IF(tx.ready)
	{
		_tx.valid = tx.valid;
		_tx.data = tx.data;
		_tx.error = tx.error;
	}

	_tx.valid = reg(_tx.valid, '0');
	_tx.data = reg(_tx.data);
	_tx.error = reg(_tx.error);

}

gtry::scl::usb::CombinedBitCrc::CombinedBitCrc(Bit in, Enum<Mode> mode, Bit reset, Bit shiftOut)
{
	HCL_NAMED(m_state);
	m_state = reg(m_state);
	m_state |= reset;

	Bit out = m_state.lsb();
	IF(mode == crc5)
		out = m_state[16 - 5];
	m_out = !out;
	HCL_NAMED(m_out);

	Bit div = in ^ out;
	div &= !shiftOut;
	HCL_NAMED(div);
	m_state = cat(div, m_state.upper(-1_b));
	m_state[0] ^= div;
	m_state[13] ^= div;

	m_match5 = m_state.upper(5_b) == 6;
	HCL_NAMED(m_match5);
	m_match16 = m_state == 0xB001;
	HCL_NAMED(m_match16);
	m_match = mux(mode == crc5, { m_match16, m_match5 });
	HCL_NAMED(m_match);

	m_ent.leave();
}
