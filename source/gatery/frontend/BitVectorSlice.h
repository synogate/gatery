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

#include "BitWidth.h"
#include "Signal.h"

namespace gtry
{
	class UInt;

	class BitVectorSlice 
	{
	public:
		virtual ~BitVectorSlice() = default;
		BitWidth width() const { return BitWidth(m_width); }

		virtual SignalReadPort readPort(SignalReadPort rootPort) const = 0;
		SignalReadPort assign(SignalReadPort current, SignalReadPort next) const;

		void makeItABit() { m_isBit = true; }
	protected:
		virtual SignalReadPort assignLocal(SignalReadPort current, std::function<SignalReadPort(SignalReadPort)> childAssign) const = 0;

		std::shared_ptr<BitVectorSlice> m_parent;
		size_t m_width = 0;
		bool m_isBit = false;
	};

	struct Selection
	{
		int start = 0;
		int width = 0;
		bool untilEndOfSource = false;

		static Selection All();
		static Selection From(int start);
		static Selection Range(int start, int end);
		static Selection Range(size_t start, size_t end);
		static Selection RangeIncl(int start, int endIncl);

		static Selection Slice(size_t offset, size_t size);

		static Selection Symbol(int idx, BitWidth symbolWidth);
		static Selection Symbol(size_t idx, BitWidth symbolWidth) { return Symbol(int(idx), symbolWidth); }

		auto operator <=> (const Selection& rhs) const = default;
	};

	struct SymbolSelect
	{
		BitWidth symbolWidth;
		Selection operator [] (int idx) const { return Selection::Symbol(idx, symbolWidth); }
		Selection operator [] (size_t idx) const { return Selection::Symbol(int(idx), symbolWidth); }
	};

	class BitVectorSliceStatic : public BitVectorSlice
	{
	public:
		BitVectorSliceStatic(const Selection& s, BitWidth parentW, const std::shared_ptr<BitVectorSlice>& r);

		bool operator < (const BitVectorSliceStatic&) const;

		virtual SignalReadPort readPort(SignalReadPort rootPort) const;
	protected:
		virtual SignalReadPort assignLocal(SignalReadPort current, std::function<SignalReadPort(SignalReadPort)> childAssign) const;

		size_t m_offset = 0;
	};

	class BitVectorSliceDynamic : public BitVectorSlice
	{
	public:
		BitVectorSliceDynamic(const UInt& dynamicOffset, size_t maxOffset, size_t dynamicOffsetMul, BitWidth w, const std::shared_ptr<BitVectorSlice>& r);

		bool operator < (const BitVectorSliceDynamic&) const;

		virtual SignalReadPort readPort(SignalReadPort rootPort) const;
	protected:
		virtual SignalReadPort assignLocal(SignalReadPort current, std::function<SignalReadPort(SignalReadPort)> childAssign) const;

		hlim::RefCtdNodePort m_offsetDynamic;
		size_t m_maxDynamicIndex = 0;
		size_t m_offsetDynamicMul = 1;
	};
}
