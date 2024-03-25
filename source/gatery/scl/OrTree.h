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
	template<Signal SigT>
	class OrTree
	{
	public:
		OrTree(size_t placeRegisterMask = 0) : m_placeRegisterMask(placeRegisterMask) {}
		void attach(const SigT& in);
		SigT out();
	private:
		Vector<SigT> OrReduce(const Vector<SigT>& in, size_t& placeRegisterMask);

		Area m_area = Area{ "scl_orTree" };
		size_t m_placeRegisterMask;
		bool m_generated = false;
		Vector<SigT> m_inputs;
		Vector<Bit> m_inputConditions;
	};


}


namespace gtry::scl {
	template<Signal SigT>
	inline void OrTree<SigT>::attach(const SigT& in) {
		m_area.enter();
		HCL_DESIGNCHECK(!m_generated);
		m_inputs.push_back(in);
		m_inputConditions.emplace_back(SignalReadPort{ ConditionalScope::get()->getFullCondition() });
	}

	template<Signal SigT>
	inline SigT OrTree<SigT>::out() {
		m_area.enter();
		HCL_DESIGNCHECK(!m_generated);
		HCL_DESIGNCHECK(m_inputs.size() != 0); // not a good way to do this
		m_generated = true;

		UInt total = ConstUInt(0, BitWidth::count(m_inputs.size()));
		for (size_t i = 0; i < m_inputs.size(); i++) {
			total += m_inputConditions[i];
			IF(!m_inputConditions[i]) {
				m_inputs[i] = allZeros(m_inputs[i]); //cannot do (&=Bit) because it's a compound, maybe do an overload?
			}
		}
		sim_assert(total <= 1) << "multiple conditions were simultaneously true, or tree is not valid in these conditions";

		SigT ret = OrReduce(m_inputs, m_placeRegisterMask).front();
		while (m_placeRegisterMask != 0) { //if there were not enough stages to accommodate the requested register mask
			if (m_placeRegisterMask & 1)
				ret = reg(ret);
			m_placeRegisterMask >>= 1;
		}
		return ret;
	}

	template<Signal SigT>
	inline Vector<SigT> OrTree<SigT>::OrReduce(const Vector<SigT>& in, size_t& placeRegisterMask) {
		const size_t inSize = in.size();
		if (inSize == 1)
			return in;

		const size_t outSize = (inSize + 1) / 2;
		Vector<SigT> ret(outSize);
		for (size_t i = 0; i < outSize; i++) {
			if ((2 * i + 1) == inSize)
				ret[i] = in.back();
			else {
				ret[i] = constructFrom(in.front());
				unpack(pack(in[2 * i]) | pack(in[2 * i + 1]), ret[i]);
			}
		}

		if (placeRegisterMask & 1) {
			ret = reg(ret);
		}
		return OrReduce(ret, placeRegisterMask >>= 1);
	}
}
