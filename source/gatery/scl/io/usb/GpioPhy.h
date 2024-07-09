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
#include "SimuPhy.h"
#include "CrcHandler.h"
#include <gatery/scl/io/RecoverDataDifferential.h>

#include "../../Counter.h"
#include "../../stream/Stream.h"

namespace gtry::scl::usb
{
	class GpioPhy : public Phy, public SimuBusBase
	{
	public:
		GpioPhy();

		virtual Bit setup(OpMode mode = OpMode::FullSpeedFunction) override;

		virtual Clock& clock() override;
		virtual const PhyRxStatus& status() const override;
		virtual PhyTxStream& tx() override;
		virtual PhyRxStream& rx() override;

		virtual bool supportCrc() const { return true; }

		// simulation helper

		virtual SimProcess deviceReset() override;
		virtual SimProcess send(std::span<const std::byte> data) override;
		virtual SimFunction<std::vector<std::byte>> receive(size_t timeoutCycles) override;

		SimProcess send(std::span<const std::byte> packet, hlim::ClockRational baudRate);
		SimProcess send(uint8_t byte, size_t& bitStuffCounter, hlim::ClockRational baudRate = { 1, 12'000'000 });

		enum Symbol { J, K, SE0, undefined };
		virtual Symbol lineState() const;
		virtual void lineState(Symbol state);

	protected:
		virtual void generateTx(Bit& en, Bit& p, Bit& n);
		virtual void generateRx(const VStream<Bit, SingleEnded>& in);
		virtual void generateCrc();
		virtual RvPacketStream<Bit> generateTxCrcAppend(RvPacketStream<Bit> in);

		virtual std::tuple<Bit, Bit> pin(std::tuple<Bit, Bit> out, Bit en);

	private:
		Clock m_clock;
		PhyRxStatus m_status;
		PhyTxStream m_tx;
		PhyRxStream m_rx;

		Bit m_se0;
		Bit m_crcEn;
		Bit m_crcIn;
		Bit m_crcOut;
		Bit m_crcMatch;
		Bit m_crcReset;
		Bit m_crcShiftOut;
		Enum<CombinedBitCrc::Mode> m_crcMode;

		std::optional<std::tuple<TristatePin, TristatePin>> m_pins;
	};
}
