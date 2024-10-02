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

namespace gtry::scl::usb
{
	struct PhyRxStatus
	{
		UInt lineState = 2_b;
		Bit sessEnd;
		Bit sessValid;
		Bit vbusValid;
		Bit rxActive;
		Bit rxError;
		Bit hostDisconnect;
		Bit id;
		Bit altInt;
	};

	struct PhyRxStream
	{
		Bit valid;
		Bit sop;
		UInt data = 8_b;

		Bit eop; // is not related to valid and is triggered some time after last data beat
		Bit error; // error is signaled during eop
	};

	struct PhyTxStream
	{
		Bit ready;

		Bit valid;	// must be asserted for the entire packet
		Bit error;
		UInt data = 8_b;
	};

	class Phy
	{
	public:
		enum class OpMode
		{
			FullSpeedFunction,
		};

		virtual ~Phy() = default;

		virtual Bit setup(OpMode mode = OpMode::FullSpeedFunction) = 0;

		virtual Clock& clock() = 0;
		virtual const PhyRxStatus& status() const = 0;
		virtual PhyTxStream& tx() = 0;
		virtual PhyRxStream& rx() = 0;

		virtual bool supportCrc() const { return false; }
	};
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::usb::PhyRxStatus, lineState, sessEnd, sessValid, vbusValid, rxActive, rxError, hostDisconnect, id, altInt);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::usb::PhyRxStream, valid, sop, eop, error, data);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::usb::PhyTxStream, ready, valid, error, data);
