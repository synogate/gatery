/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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
#include "../Node.h"

namespace gtry::hlim {


/**
 * @brief A sink for boolean signals that drive a clock's reset
 * @details Acts as the interfacing node to clocks, keeping the signal alive if bound to a clock.
 */
class Node_Signal2Rst : public Node<Node_Signal2Rst>
{
	public:
		Node_Signal2Rst();

		void connect(const NodePort &np);
		
		virtual std::string getTypeName() const override;
		virtual void assertValidity() const override;
		virtual std::string getInputName(size_t idx) const override;
		virtual std::string getOutputName(size_t idx) const override;

		virtual bool hasSideEffects() const override { return m_clocks[0] != nullptr; }
		virtual bool isCombinatorial(size_t port) const override { return true; }

		void setClock(Clock *clk);

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;
		virtual bool checkValidInputClocks(std::span<SignalClockDomain> inputClocks) const override { return true; }
	protected:
};

}
