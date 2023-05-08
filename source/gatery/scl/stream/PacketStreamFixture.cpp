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

#include "PacketStreamFixture.h"

namespace gtry::scl
{

    void StreamTransferFixture::transfers(size_t numTransfers)
    {
        HCL_ASSERT(m_groups == 0);
        m_transfers = numTransfers;
    }
    void StreamTransferFixture::groups(size_t numGroups)
    {
        HCL_ASSERT(m_groups == 0);
        m_groups = numGroups;
    }

    void StreamTransferFixture::simulateSendData(scl::RvStream<UInt>& stream, size_t group)
    {
        addSimulationProcess([=, this, &stream]()->SimProcess {
            std::mt19937 rng{ std::random_device{}() };
            for(size_t i = 0; i < m_transfers; ++i)
            {
                simu(valid(stream)) = '0';
                simu(*stream).invalidate();

                while((rng() & 1) == 0)
                    co_await AfterClk(m_clock);

                simu(valid(stream)) = '1';
                simu(*stream) = i + group * m_transfers;

                co_await scl::performTransferWait(stream, m_clock);
            }
            simu(valid(stream)) = '0';
            simu(*stream).invalidate();
        });
    }
}
