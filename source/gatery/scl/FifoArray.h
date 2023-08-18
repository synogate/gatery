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

	namespace internal {
		struct fifoPointer_t {
			Bit trick;
			UInt value;
			fifoPointer_t increment() {
				UInt incrementedPointer = cat(trick, value) + UInt{1}; HCL_NAMED(incrementedPointer);
				UInt newValue = incrementedPointer.lower(value.width());
				Bit newTrick = incrementedPointer.msb();
				return fifoPointer_t{ newTrick, newValue };
			}
		};
	}

	/**
	 * @brief This class describes an array of Fifo's who's data is stored in a common BRAM structure. 
	 * Its limitations are that only one fifo in the array can be pushed to or popped from at the time.
	 * This implementation is restricted to equal-sized partitions (each fifo holds the same amount of elements)
	*/
	template <Signal SigT>
	class FifoArray
	{
	public:
		FifoArray(size_t numberOfFifos, size_t elementsPerFifo, SigT dataSample);
		void push(UInt selectFifo);
		void pop(UInt selectFifo);
		inline SigT peek() { return final(m_popData); }
		inline void present(SigT data) { m_pushData = data; }

		inline Bit push_failed() { return final(m_push_failed); }
		inline Bit pop_failed() { return final(m_pop_failed); }

		bool m_hasSetup = false;
		
	protected:

		inline Bit isEmpty(internal::fifoPointer_t putPtr, internal::fifoPointer_t getPtr) { return putPtr.value == getPtr.value & putPtr.trick == getPtr.trick; }
		inline Bit isFull(internal::fifoPointer_t putPtr, internal::fifoPointer_t getPtr) { return putPtr.value == getPtr.value & putPtr.trick != getPtr.trick; }

		SigT m_pushData;
		SigT m_popData;

		Bit m_push_failed = dontCare(Bit{});
		Bit m_pop_failed = dontCare(Bit{});

		Memory<SigT> m_dataMem;
		Memory<internal::fifoPointer_t> m_putPtrMem;
		Memory<internal::fifoPointer_t> m_getPtrMem;
	private:
	
	};

	template<Signal SigT>
	FifoArray<SigT>::FifoArray(size_t numberOfFifos, size_t elementsPerFifo, SigT dataSample)
	{
		HCL_DESIGNCHECK_HINT(!m_hasSetup, "fifo array already initialized");
		m_hasSetup = true;
		
		HCL_DESIGNCHECK_HINT(numberOfFifos > 0, "cannot create a FifoArray with no fifos");
		HCL_DESIGNCHECK_HINT((numberOfFifos & (numberOfFifos - 1)) == 0, "The current implementation only supports powers of two");
	
		HCL_DESIGNCHECK_HINT(elementsPerFifo > 0, "cannot create a fifo with no elements");
		HCL_DESIGNCHECK_HINT((elementsPerFifo & (elementsPerFifo - 1)) == 0, "The current implementation only supports powers of two");
		
		internal::fifoPointer_t ptrSample{.value = BitWidth::count(elementsPerFifo)};

		m_popData = dontCare(dataSample); HCL_NAMED(m_popData);
		m_pushData = constructFrom(dataSample); HCL_NAMED(m_pushData);
	
		m_dataMem.setup(numberOfFifos * elementsPerFifo, dataSample);
		m_dataMem.setName("DataMemory");

		m_putPtrMem.setup(numberOfFifos, ptrSample);
		m_putPtrMem.setName("PutPointerMemory");
		m_putPtrMem.initZero();

		m_getPtrMem.setup(numberOfFifos, ptrSample);
		m_getPtrMem.setName("getPointerMemory");
		m_getPtrMem.initZero();
	}

	template<Signal SigT>
	inline void FifoArray<SigT>::push(UInt selectFifo)
	{
		Area area("push", true);
		internal::fifoPointer_t putPtr = m_putPtrMem[selectFifo]; HCL_NAMED(putPtr);
		internal::fifoPointer_t getPtr = m_getPtrMem[selectFifo]; HCL_NAMED(getPtr);

		m_push_failed = '1';
		IF(!isFull(putPtr, getPtr)) {
			m_dataMem[cat(selectFifo, putPtr.value)] = m_pushData;
			m_putPtrMem[selectFifo] = putPtr.increment();
			m_push_failed = '0';
		}
		//ELSE m_push_failed = '1';
	}

	template<Signal SigT>
	inline void FifoArray<SigT>::pop(UInt selectFifo)
	{
		Area area("scl_fifoArray_pop", true);
		HCL_NAMED(selectFifo);
		internal::fifoPointer_t putPtr = m_putPtrMem[selectFifo]; HCL_NAMED(putPtr);
		internal::fifoPointer_t getPtr = m_getPtrMem[selectFifo]; HCL_NAMED(getPtr);

		m_pop_failed = '1';
		m_popData = dontCare(m_popData);
		IF(!isEmpty(putPtr, getPtr)){
			m_popData = m_dataMem[cat(selectFifo, getPtr.value)];
			m_getPtrMem[selectFifo] = getPtr.increment();
			m_pop_failed = '0';
		}
		//ELSE m_pop_failed = '1';
	}
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::internal::fifoPointer_t, trick, value);
