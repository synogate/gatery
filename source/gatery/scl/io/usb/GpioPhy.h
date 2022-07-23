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

#include "../../stream/Stream.h"

namespace gtry::scl::usb
{
	class GpioPhy : public Phy
	{
	public:
		GpioPhy();

		virtual Bit setup(OpMode mode = OpMode::FullSpeedFunction) override;

		virtual Clock& clock() override;
		virtual const PhyRxStatus& status() const override;
		virtual PhyTxStream& tx() override;
		virtual PhyRxStream& rx() override;

	protected:
		virtual void generateTx(Bit& en, Bit& p, Bit& n);
		virtual void generateRx(const VStream<UInt>& in);

	private:
		Clock m_clock;
		PhyRxStatus m_status;
		PhyTxStream m_tx;
		PhyRxStream m_rx;
	};
}
