/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2026 Michael Offel, Andreas Ley

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
#include "ModLoadStore.h"

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class LoadStoreSubword : public Base
	{
	protected:

		auto selectMemCtrl()
		{
			return [this](const MemCtrl& in, const InstrOpcode& instr, const AluSum& sum, const OperandsDiff& diff)
				{
					MemCtrl ctrl = Base::selectMemCtrl()(in, instr, sum, diff);
					// override access based right aligned mask
					ctrl.memWriteMask = ctrl.memWriteMask.width().mask();

					size_t numBytes = ctrl.memWriteData.width().bytes();
					m_logSize.resize(utils::Log2(numBytes));
					BVec writeData = ctrl.memWriteData;
					for (size_t logSize = 0; logSize < m_logSize.size(); ++logSize)
					{
						m_logSize[logSize] = ctrl.memLogSize == logSize;

						IF(m_logSize[logSize])
						{
							// broadcast subword to all possible locations independent of address
							size_t parts = numBytes / (1ull << logSize);
							BVec subWord = writeData.part(parts, 0);
							for (size_t i = 1; i < parts; ++i)
								ctrl.memWriteData.part(parts, i) = subWord;

							// select mask based on address and size
							const UInt& subAddr = ctrl.memAddress(logSize, BitWidth{ m_logSize.size() - logSize });
							BVec& subMask = ctrl.memWriteMask.part(parts, subAddr);
							subMask = subMask.width().mask();
						}
					}

					return ctrl;
				};
		}

		auto shiftMemReadData()
		{
			return [this](const MemCtrl& ctrl, const ResultCollection& in)
				{
					ResultCollection results = in;
					auto it = std::ranges::find(results.result, "Mem", &ExecResult::name);
					if (it == results.result.end())
						throw std::runtime_error("no memory result to align");

					BitWidth wordW = it->value.width();
					std::vector<BVec> alignedData(utils::Log2(wordW.bytes()) + 1);
					for (size_t logSize = 0; logSize < alignedData.size(); ++logSize)
					{
						const UInt& subAddr = ctrl.memAddress(logSize, BitWidth{ m_logSize.size() - logSize });
						size_t parts = wordW.bytes() / (1ull << logSize);
						alignedData[logSize] = zext(it->value.part(parts, subAddr), wordW) | name(std::format("LoadStoreSubword_readdata_log{}", logSize));
					}

					it->value = mux(ctrl.memLogSize, alignedData);
					return results;
				};
		}

	private:
		std::vector<Bit> m_logSize;


	};
}
