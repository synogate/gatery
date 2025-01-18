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
#include <gatery/scl/stream/strm.h>

namespace gtry::scl
{
	enum class QSpiDir
	{
		send,
		dummy,
		receive
		// select_chip
	};

	struct QSpiCmd
	{
		BVec data;
		Enum<QSpiDir> direction;
	};

	/**
	 * @brief QSpiPhy is just an interface study and has never been used in real hw!
	 *			CS is controlled by eop
	 */
	class QSpiPhy
	{
	public:
		QSpiPhy& pinPrefix(std::string prefix) { m_pinPrefix = move(prefix); return *this; }

		virtual VPacketStream<BVec> generate(RvPacketStream<QSpiCmd> cmd, BitWidth ioW = 4_b)
		{
			Area ent{ "scl_qspi_controller", true };
			Counter phase(cmd->data.width() / ioW);

			IF(valid(cmd))
				phase.inc();
			ready(cmd) = phase.isLast();

			BVec out = muxWord(phase.value(), swapEndian(cmd->data, ioW)) | name("out");
			Bit outEn = cmd->direction == QSpiDir::send | name("outEn");
			Bit clkEn = valid(cmd) | name("clkEn");
			Bit cs = flagInstantSet(valid(cmd) & cmd->direction != QSpiDir::dummy, valid(cmd) & eop(cmd)) | name("cs");

			BVec in = cmd->data.width();
			in = reg(in);
			in <<= (int)ioW.bits();
			in.lower(ioW) = pin(out, outEn, clkEn, cs);
			in = name(in, "in");

			VPacketStream<BVec> ret{ in };
			eop(ret) = eop(cmd);
			valid(ret) = cmd->direction == QSpiDir::receive & phase.isLast();
			ret = name(ret, "ret");
			return ret;
		}

	protected:
		virtual BVec pin(BVec out, Bit outEn, Bit clkEn, Bit cs)
		{
			pinOut(ClockScope::getClk().clkSignal() & clkEn, m_pinPrefix + "CLK");
			pinOut(!cs, m_pinPrefix + "CSn");
			return (BVec)tristatePin(out, outEn).setName(m_pinPrefix + "DATA");
		}

		std::string m_pinPrefix = "QSPI_";
	};



}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::QSpiCmd, data, direction);
