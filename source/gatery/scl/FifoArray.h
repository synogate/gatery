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
	 * @brief This class describes an array of Fifo's whose data is stored in a common BRAM structure. 
	 * Its limitations are that only one fifo in the array can be pushed to or popped from at a time.
	 * This implementation is restricted to equal-sized partitions (each fifo holds the same amount of elements)
	 * and both the number of fifos and the number of elements must be a power of 2.
	*/
	template <Signal SigT>
	class FifoArray
	{
	public:

		inline FifoArray() {}
		explicit inline FifoArray(size_t numberOfFifos, size_t elementsPerFifo, SigT dataSample){ setup(numberOfFifos, elementsPerFifo, dataSample);}
		/**
		 * @brief This functions sets up the fifo array.
		 * @param numberOfFifos The number of fifos you would like to use (currently only supports powers of 2)
		 * @param elementsPerFifo The number of elements per fifo you would like to store ( currently only supports powers of 2)
		 * @param dataSample An initialized signal of the nature and size of the data you would like to store.
		*/
		void setup(size_t numberOfFifos, size_t elementsPerFifo, SigT dataSample);

		/**
		* @brief Selects which fifo in the fifoArray we will push to.
		* @param selectFifo The Fifo index in the array.
		*/
		inline void selectPush(UInt selectFifo){ m_pushFifoSelector = selecFifo; }
		/**
		* @brief Pushes data into the selected fifo (must use the selectPush function first)
		* @param data The data to be pushed.
		*/
		inline void push(SigT data) { m_mustPush = '1'; m_pushData = data; }
		/**
		* @brief Returns High if the selected fifo is full ( must use the selectPush function first)
		*/
		inline Bit full() {  return m_full; }

		/**
		* @brief Selects which fifo in the fifoArray we will pop from.
		* @param selectFifo The Fifo index in the array.
		*/
		inline void selectPop(UInt selectFifo) { m_popFifoSelector = selectFifo;  }

		/**
		 * @brief Gives access to the poppable data (must use the selectPop function first)
		 * @return 
		*/
		inline SigT peek() { return m_popData; }
		/**
		* @brief Pops data from the selected fifo (must use the selectPop function first)
		*/
		inline void pop() { m_mustPop = '1'; }
		/**
		* @brief Returns High if the selected fifo is empty ( must use the selectPop function first)
		*/
		inline Bit empty() { return m_empty; }

		/**
		 * @brief Generates the fifo.
		*/
		void generate();


		
	protected:
		Area m_area{ "scl_fifoArray" };

		bool m_hasSetup = false;
		bool m_hasGenerate = false;

		inline Bit isEmpty(internal::fifoPointer_t putPtr, internal::fifoPointer_t getPtr) { return putPtr.value == getPtr.value & putPtr.trick == getPtr.trick; }
		inline Bit isFull(internal::fifoPointer_t putPtr, internal::fifoPointer_t getPtr) { return putPtr.value == getPtr.value & putPtr.trick != getPtr.trick; }

		SigT	m_pushData;
		UInt	m_pushFifoSelector;
		Bit		m_full;
		Bit		m_mustPush = '0';

		SigT	m_popData;
		UInt	m_popFifoSelector;
		Bit		m_empty;
		Bit		m_mustPop = '0';

		size_t m_numberOfFifos;
		size_t m_elementsPerFifo;

		Memory<SigT> m_dataMem;
		Memory<internal::fifoPointer_t> m_putPtrMem;
		Memory<internal::fifoPointer_t> m_getPtrMem;
	};


	template<Signal SigT>
	void FifoArray<SigT>::setup(size_t numberOfFifos, size_t elementsPerFifo, SigT dataSample)
	{
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(!m_hasSetup, "fifo array already initialized");
		m_hasSetup = true;
		
		HCL_DESIGNCHECK_HINT(numberOfFifos > 0, "cannot create a FifoArray with no fifos");
		HCL_DESIGNCHECK_HINT((numberOfFifos & (numberOfFifos - 1)) == 0, "The current implementation only supports powers of two");
	
		HCL_DESIGNCHECK_HINT(elementsPerFifo > 0, "cannot create a fifo with no elements");
		HCL_DESIGNCHECK_HINT((elementsPerFifo & (elementsPerFifo - 1)) == 0, "The current implementation only supports powers of two");
		
		m_pushData = constructFrom(dataSample); HCL_NAMED(m_pushData);
		m_popData = dontCare(dataSample); HCL_NAMED(m_popData);

		m_pushFifoSelector = BitWidth::count(elementsPerFifo);
		m_popFifoSelector = BitWidth::count(elementsPerFifo);

		m_numberOfFifos = numberOfFifos;
		m_elementsPerFifo = elementsPerFifo;
	}


	template<Signal SigT>
	void FifoArray<SigT>::generate()
	{
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasSetup, "fifo has not been set up yet");
		HCL_DESIGNCHECK_HINT(!m_hasGenerate, "fifo has already been generated");
		m_hasGenerate = true;

		/* memories generate */
		m_dataMem.setup(m_numberOfFifos * m_elementsPerFifo, m_pushData);
		m_dataMem.setName("DataMemory");

		internal::fifoPointer_t ptrSample{.value = BitWidth::count(elementsPerFifo)};

		m_putPtrMem.setup(m_numberOfFifos, ptrSample);
		m_putPtrMem.setName("PutPointerMemory");
		m_putPtrMem.initZero();

		m_getPtrMem.setup(m_numberOfFifos, ptrSample);
		m_getPtrMem.setName("getPointerMemory");
		m_getPtrMem.initZero();

		/* push section */
		internal::fifoPointer_t putPtr = m_putPtrMem[m_pushFifoSelector]; setName(putPtr, "putPtr_push");
		internal::fifoPointer_t getPtr = m_getPtrMem[m_pushFifoSelector]; setName(getPtr, "getPtr_push");

		m_full = isFull(putPtr, getPtr);

		IF(m_mustPush & !m_full) {
			m_dataMem[cat(selectFifo, putPtr.value)] = m_pushData;
			m_putPtrMem[selectFifo] = putPtr.increment();
		}

		/* pop section */
		putPtr = m_putPtrMem[m_popFifoSelector]; setName(putPtr, "putPtr_pop");
		getPtr = m_getPtrMem[m_popFifoSelector]; setName(getPtr, "getPtr_pop");	

		m_empty = isEmpty(putPtr, getPtr);

		IF(!m_empty) {
			m_popData = m_dataMem[cat(selectFifo, getPtr.value)];
			IF(m_mustPop)
				m_getPtrMem[selectFifo] = getPtr.increment();
		}
	}
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::internal::fifoPointer_t, trick, value);
