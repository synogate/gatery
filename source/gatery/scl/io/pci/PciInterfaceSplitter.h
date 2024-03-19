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
#include <gatery/scl/io/pci/pci.h>

namespace gtry::scl::pci {
	class PciInterfaceSplitter {
	public:
		PciInterfaceSplitter() = default;
		PciInterfaceSplitter(CompleterInterface& compInt, RequesterInterface& reqInt, TlpPacketStream<EmptyBits, BarInfo>&& rx);

		PciInterfaceSplitter& rx(TlpPacketStream<EmptyBits, BarInfo>&& rx) { m_rx = move(rx); return *this; }
		TlpPacketStream<EmptyBits>& tx() { HCL_DESIGNCHECK(m_tx); return *m_tx; }

		TlpPacketStream<EmptyBits, BarInfo> completerRequest() { HCL_DESIGNCHECK(m_completerRequest); return move(*m_completerRequest); }
		TlpPacketStream<EmptyBits> requesterCompletion() { HCL_DESIGNCHECK(m_requesterCompletion); return move(*m_requesterCompletion); }

		PciInterfaceSplitter& completerCompletion(TlpPacketStream<EmptyBits>&& completerCompletion) { m_completerCompletion = move(completerCompletion); return *this; }
		PciInterfaceSplitter& requesterRequest(TlpPacketStream<EmptyBits>&& requesterRequest) { m_requesterRequest = move(requesterRequest); return *this; }

		void generate();

		//RequesterInterface& requesterInterface() { HCL_DESIGNCHECK(m_requesterInterface); return *m_requesterInterface; }
		//CompleterInterface& completerInterface() { HCL_DESIGNCHECK(m_completerInterface); return *m_completerInterface; }
	private:
		void splitRx();
		void mergeTx();
		std::optional<TlpPacketStream<EmptyBits, BarInfo>> m_rx;
		std::optional<TlpPacketStream<EmptyBits, BarInfo>> m_completerRequest;

		std::optional<TlpPacketStream<EmptyBits>> m_tx;
		std::optional<TlpPacketStream<EmptyBits>> m_completerCompletion;
		std::optional<TlpPacketStream<EmptyBits>> m_requesterRequest;
		std::optional<TlpPacketStream<EmptyBits>> m_requesterCompletion;
	};
}
