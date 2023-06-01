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
#include "../Node.h"

namespace gtry::hlim 
{
	class Node_Shift : public Node<Node_Shift>
	{
	public:
		enum class dir {
			left, right
		};

		enum class fill {
			zero,	   // fill with '0'
			one,		// fill with '1'
			last,	   // arithmetic fill
			rotate	  // fill with shift out bits
		};

		enum Input {
			INPUT_OPERAND = 0,
			INPUT_AMOUNT = 1,
			NUM_INPUTS
		};

		Node_Shift(dir _direction, fill _fill);

		void connectOperand(const NodePort& port);
		void connectAmount(const NodePort& port);

		virtual void simulateEvaluate(sim::SimulatorCallbacks& simCallbacks, sim::DefaultBitVectorState& state, const size_t* internalOffsets, const size_t* inputOffsets, const size_t* outputOffsets) const override;

		virtual std::string getTypeName() const override;
		virtual void assertValidity() const override;
		virtual std::string getInputName(size_t idx) const override;
		virtual std::string getOutputName(size_t idx) const override;

		dir getDirection() const { return m_direction; }
		fill getFillMode() const { return m_fill; }

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;
	protected:
		const dir m_direction;
		const fill m_fill;
	};
}
