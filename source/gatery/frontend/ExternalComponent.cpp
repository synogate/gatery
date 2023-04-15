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
#include "gatery/pch.h"
#include "ExternalComponent.h"

#include <ranges>

namespace gtry 
{
	void ExternalComponent::setInput(size_t input, const Bit &bit)
	{
		HCL_DESIGNCHECK_HINT(input < getNumInputPorts(), "Invalid input idx");
		HCL_DESIGNCHECK_HINT(!m_inputPorts[input].isVector, "Input is not a bit");

		rewireInput(input, bit.readPort());
	}

	void ExternalComponent::setInput(size_t input, const BVec &bvec)
	{
		HCL_DESIGNCHECK_HINT(input < getNumInputPorts(), "Invalid input idx");
		HCL_DESIGNCHECK_HINT(m_inputPorts[input].isVector, "Input is not a bvec");
		HCL_DESIGNCHECK_HINT(m_inputPorts[input].instanceWidth == bvec.size(), "Input has wrong width");

		rewireInput(input, bvec.readPort());
	}

	Bit ExternalComponent::getOutputBit(size_t output)
	{
		HCL_DESIGNCHECK_HINT(output < getNumOutputPorts(), "Invalid output idx");
		HCL_DESIGNCHECK_HINT(!m_outputPorts[output].isVector, "Output is not a bit");

		return Bit(SignalReadPort(hlim::NodePort{ .node = this, .port = (size_t)output }));
	}

	BVec ExternalComponent::getOutputBVec(size_t output)
	{
		HCL_DESIGNCHECK_HINT(output < getNumOutputPorts(), "Invalid output idx");
		HCL_DESIGNCHECK_HINT(m_outputPorts[output].isVector, "Output is not a bvec");

		return BVec(SignalReadPort(hlim::NodePort{ .node = this, .port = (size_t)output }));
	}
}
