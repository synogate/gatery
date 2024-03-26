/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2024 Michael Offel, Andreas Ley

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


namespace gtry::scl
{

	template<Signal TData>
	class RepeatBuffer
	{
		public:
			RepeatBuffer() : m_area("scl_repeatBuffer") { }
			RepeatBuffer(const RepeatBuffer&) = delete;
			RepeatBuffer(RepeatBuffer&&) = default;
			explicit RepeatBuffer(size_t minDepth, const TData& ref = TData{}) : RepeatBuffer() { setup(minDepth, std::move(ref)); }
			//~RepeatBuffer() noexcept(false);

			void setup(size_t minDepth, const TData& ref = TData{});

			size_t depth();
			BitWidth wordWidth() const { return width(m_rdPeekData); }

			/// @note Do not modify while reading a packet. Does not latch on its own.
			void wrapAround(UInt last) { m_wrapAropundLast = zext(last); }

			void wrReset() { m_wrReset = '1'; }
			Bit wrIsLast() const { return m_wrIsLast; }
			void wrPush(TData data) { m_wrPushData = data; m_wrPush = '1'; }

			void rdReset() { m_rdReset = '1'; }
			Bit rdIsFirst() const { return m_rdIsFirst; }
			Bit rdIsLast() const { return m_rdIsLast; }
			TData rdPeek() const { return m_rdPeekData; }
			void rdPop() { m_rdPop = '1'; }

			void makeReadWrietIndependent();

			void noConflicts() { m_memory.noConflicts(); }
			void allowArbitraryPortRetiming() { m_memory.allowArbitraryPortRetiming(); }
		protected:
			Area m_area;

			Memory<TData> m_memory;
			
			UInt m_wrapAropundLast;

			UInt m_wrWrapAropundLast;
			Bit m_wrReset;
			Bit m_wrPush;
			TData m_wrPushData;
			Bit m_wrIsLast;

			UInt m_rdWrapAropundLast;
			Bit m_rdReset;
			Bit m_rdIsFirst;
			TData m_rdPeekData;
			Bit m_rdIsLast;
			Bit m_rdPop;
		private:
			bool m_hasSetup = false;
	};

	

	template<Signal TData>
	void RepeatBuffer<TData>::makeReadWrietIndependent() 
	{
		noConflicts();
		allowArbitraryPortRetiming();
		//m_wrWrapAropundLast = retimingBlocker(m_wrapAropundLast);
		//m_rdWrapAropundLast = retimingBlocker(m_wrapAropundLast);
	}

	template<Signal TData>
	void RepeatBuffer<TData>::setup(size_t minDepth, const TData& ref)
	{
		HCL_DESIGNCHECK_HINT(!m_hasSetup, "Setup was already called!");
		m_hasSetup = true;
		
		auto scope = m_area.enter();


		m_memory.setup(minDepth, ref);

		const BitWidth counterWidth = m_memory.addressWidth() + 1;
		m_wrapAropundLast = counterWidth;
		m_wrWrapAropundLast = counterWidth;
		m_rdWrapAropundLast = counterWidth;

		HCL_NAMED(m_wrWrapAropundLast);
		HCL_NAMED(m_wrReset);
		HCL_NAMED(m_wrPush);
		HCL_NAMED(m_wrIsLast);
		m_wrPushData = constructFrom(ref);
		HCL_NAMED(m_wrPushData);


		HCL_NAMED(m_rdWrapAropundLast);
		HCL_NAMED(m_rdReset);
		HCL_NAMED(m_rdPop);

		{
			UInt writePtr = counterWidth;
			IF (m_wrReset)
				writePtr = 0;

			HCL_NAMED(writePtr);

			IF (m_wrPush)
				m_memory[writePtr(0, -1_b)] = m_wrPushData;

			m_wrIsLast = writePtr == m_wrWrapAropundLast;

			IF (m_wrPush) {
				IF (m_wrIsLast)
					writePtr = 0;
				ELSE
					writePtr += 1;
			}

			writePtr = reg(writePtr, 0);
		}

		{
			UInt readPtr = counterWidth;
			IF (m_rdReset)
				readPtr = 0;

			m_rdIsFirst = readPtr == 0;

			HCL_NAMED(readPtr);
			m_rdPeekData = m_memory[readPtr(0, -1_b)];

			m_rdIsLast = readPtr == m_rdWrapAropundLast;

			IF (m_rdPop) {
				IF (m_rdIsLast)
					readPtr = 0;
				ELSE
					readPtr += 1;
			}

			readPtr = reg(readPtr, 0);
		}

		HCL_NAMED(m_rdPeekData);
		HCL_NAMED(m_rdIsFirst);
		HCL_NAMED(m_rdIsLast);

		m_wrWrapAropundLast = m_wrapAropundLast;
		m_rdWrapAropundLast = m_wrapAropundLast;
		m_wrapAropundLast = minDepth-1;

		m_wrReset = '0';
		m_wrPush = '0';

		m_rdReset = '0';
		m_rdPop = '0';

		m_wrPushData = dontCare(ref);
	}

}