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

namespace gtry::scl::pci 
{
	/**
	 * @brief	Splits the TLP rx/tx streams into our vendor independent interface of 
	 *			completer request/response (CompleterInterface) and requester request/response (RequesterInterface).
	 *				rx -> (completer request | requester completion)
	 *				(completer completion | requester request) -> tx
	 */
	TlpPacketStream<EmptyBits> interfaceSplitter(CompleterInterface&& compInt, RequesterInterface&& reqInt, TlpPacketStream<EmptyBits, BarInfo>&& rx);

	void						interfaceSplitterRx(TlpPacketStream<EmptyBits, BarInfo>&& completerRequest, TlpPacketStream<EmptyBits>&& requesterCompletion, TlpPacketStream<EmptyBits, BarInfo>&& rx);
	TlpPacketStream<EmptyBits>	interfaceSplitterTx(TlpPacketStream<EmptyBits>&& completerCompletion, TlpPacketStream<EmptyBits>&& requesterRequest);
}
