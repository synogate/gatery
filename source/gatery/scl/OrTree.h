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


namespace gtry::scl {
	/**
	 * @brief This class implements a simple to use Or-tree. It allows for a large amount of non-backpressured
	 * compounds to be multiplexed efficiently, when only one is "valid" at a time, by setting all others
	 * to zero and creating an or-tree. The validity of the inputs is decided by the conditional scope 
	 * in which the attach function is called. During simulation, an assert will be thrown if multiple inputs are
	 * simulatenously valid.
	*/
	template<Signal SigT>
	class OrTree
	{
	public:
		void attach(const SigT& in);
		SigT generate(size_t placeRegisterMask = 0);

		static Vector<SigT> orReduce(const Vector<SigT>& in, size_t& placeRegisterMask);
	private:
		Area m_area = Area{ "scl_orTree" };
		bool m_generated = false;
		Vector<SigT> m_inputs;
		Vector<Bit> m_inputConditions;
	};
}


namespace gtry::scl 
{
	template<Signal SigT>
	void OrTree<SigT>::attach(const SigT& in) {
		m_area.enter();
		HCL_DESIGNCHECK(!m_generated);
		m_inputs.push_back(in);
		m_inputConditions.emplace_back(SignalReadPort{ ConditionalScope::get()->getFullCondition() });
	}

	template<Signal SigT>
	SigT OrTree<SigT>::generate(size_t placeRegisterMask) {
		m_area.enter();
		HCL_DESIGNCHECK(!m_generated);
		HCL_DESIGNCHECK(m_inputs.size() != 0);
		m_generated = true;

		sim_assert(bitcount(m_inputConditions) <= 1) << "multiple input conditions were simultaneously true, or-tree is not valid in these conditions";

		for (size_t i = 0; i < m_inputs.size(); i++) {
			IF(!m_inputConditions[i]) {
				m_inputs[i] = allZeros(m_inputs[i]); //cannot do (&=Bit) because it's a compound
			}
		}

		size_t currentRegisterMask = placeRegisterMask;
		SigT ret = orReduce(m_inputs, currentRegisterMask).front();

		//if there were not enough stages to accommodate the requested register mask, place more trailing registers
		while (currentRegisterMask != 0) { 
			if ((currentRegisterMask & 1))
				ret = reg(ret);
			currentRegisterMask >>= 1;
		}

		return ret;
	}

	template<Signal SigT>
	Vector<SigT> OrTree<SigT>::orReduce(const Vector<SigT>& in, size_t& placeRegisterMask) {
		if (in.size() == 1)
			return in;

		const size_t outSize = (in.size() + 1) / 2;
		Vector<SigT> ret(outSize);
		for (size_t i = 0; i < outSize; i++) {
			if ((2 * i + 1) == in.size())
				ret[i] = in.back();
			else {
				ret[i] = constructFrom(in.front());
				unpack(pack(in[2 * i]) | pack(in[2 * i + 1]), ret[i]);
			}
		}

		if ((placeRegisterMask & 1)) {
			ret = reg(ret);
		}

		return orReduce(ret, placeRegisterMask >>= 1);
	}
}
