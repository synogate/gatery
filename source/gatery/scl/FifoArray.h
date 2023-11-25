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

		FifoArray() {}
		FifoArray(const FifoArray&) = delete;
		FifoArray(FifoArray&&) = default;
		FifoArray(size_t numberOfFifos, size_t elementsPerFifo, SigT dataSample){ setup(numberOfFifos, elementsPerFifo, dataSample);}
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
		void selectPush(UInt selectFifo){ 
			HCL_DESIGNCHECK_HINT(!m_hasGenerate, "cannot call selectPush after generating the fifo array");
			m_pushFifoSelector = selectFifo; 
		}
		/**
		* @brief Pushes data into the selected fifo (must use the selectPush function first)
		* @param data The data to be pushed.
		*/
		void push(SigT data) { 
			HCL_DESIGNCHECK_HINT(!m_hasGenerate, "cannot call push after generating the fifo array");
			m_mustPush = '1'; m_pushData = data; 
		}

		/**
		* @brief Returns High if the selected fifo is full ( must use the selectPush function first)
		*/
		const Bit& full() const { return m_full; }

		/**
		* @brief Selects which fifo in the fifoArray we will pop from.
		* @param selectFifo The Fifo index in the array.
		*/
		void selectPop(UInt selectFifo) { 
			HCL_DESIGNCHECK_HINT(!m_hasGenerate, "cannot call selectPop after generating the fifo array");
			m_popFifoSelector = selectFifo;  }

		/**
		 * @brief Gives access to the poppable data (must use the selectPop function first)
		 * @return 
		*/
		const SigT& peek() const { return m_popData; }
		/**
		* @brief Pops data from the selected fifo (must use the selectPop function first)
		*/
		void pop() { m_mustPop = '1'; }
		/**
		* @brief Returns High if the selected fifo is empty ( must use the selectPop function first)
		*/
		const Bit& empty() const { return m_empty; }

		const UInt& size() const { return m_size; }

		/**
		 * @brief Generates the fifo.
		*/
		void generate();


		
	protected:
		Area m_area{ "scl_fifoArray" };

		bool m_hasSetup = false;
		bool m_hasGenerate = false;

		Bit isEmpty(internal::fifoPointer_t putPtr, internal::fifoPointer_t getPtr) { return putPtr.value == getPtr.value & putPtr.trick == getPtr.trick; }
		Bit isFull(internal::fifoPointer_t putPtr, internal::fifoPointer_t getPtr)	{ return putPtr.value == getPtr.value & putPtr.trick != getPtr.trick; }
		UInt size(internal::fifoPointer_t putPtr, internal::fifoPointer_t getPtr)	{ return cat(putPtr.trick, putPtr.value) - cat(getPtr.trick, getPtr.value); }

		SigT	m_pushData;
		UInt	m_pushFifoSelector;
		Bit		m_full;
		Bit		m_mustPush = '0';
		//Bit		m_isPushing = '0';

		SigT	m_popData;
		UInt	m_popFifoSelector;
		Bit		m_empty;
		UInt	m_size;
		Bit		m_mustPop = '0';
		//Bit		m_isPopping = '0';

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
		
		m_pushData = dontCare(dataSample); HCL_NAMED(m_pushData);
		m_popData = constructFrom(dataSample); HCL_NAMED(m_popData);

		m_pushFifoSelector = BitWidth::count(numberOfFifos);
		m_popFifoSelector = BitWidth::count(numberOfFifos);
		m_size = BitWidth::last(elementsPerFifo);

		m_numberOfFifos = numberOfFifos;
		m_elementsPerFifo = elementsPerFifo;
	}


	template<Signal SigT>
	void FifoArray<SigT>::generate()
	{
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasSetup, "The fifo array has not been set up yet");
		HCL_DESIGNCHECK_HINT(!m_hasGenerate, "The fifo array has already been generated");
		m_hasGenerate = true;

		/* memories generation */
		m_dataMem.setup(m_numberOfFifos * m_elementsPerFifo, m_pushData);
		m_dataMem.setName("DataMemory");

		internal::fifoPointer_t ptrSample{.value = BitWidth::count(m_elementsPerFifo)};

		m_putPtrMem.setup(m_numberOfFifos, ptrSample);
		m_putPtrMem.setName("putPointerMemory");
		m_putPtrMem.setType(MemType::DONT_CARE, 0);
		m_putPtrMem.initZero();
		
		m_getPtrMem.setup(m_numberOfFifos, ptrSample);
		m_getPtrMem.setName("getPointerMemory");
		m_getPtrMem.setType(MemType::DONT_CARE, 0);
		m_getPtrMem.initZero();


		HCL_NAMED(m_popFifoSelector);
		internal::fifoPointer_t popPutPtr = m_putPtrMem[m_popFifoSelector]; setName(popPutPtr, "putPtr_pop");
		internal::fifoPointer_t popGetPtr = m_getPtrMem[m_popFifoSelector]; setName(popGetPtr, "getPtr_pop");	

		HCL_NAMED(m_pushFifoSelector);
		internal::fifoPointer_t pushPutPtr = m_putPtrMem[m_pushFifoSelector]; setName(pushPutPtr, "putPtr_push");
		internal::fifoPointer_t pushGetPtr = m_getPtrMem[m_pushFifoSelector]; setName(pushGetPtr, "getPtr_push");

		m_empty = isEmpty(popPutPtr, popGetPtr);
		m_size = size(popPutPtr, popGetPtr);

		m_popData = dontCare(m_popData);
		IF(!m_empty) {
			m_popData = m_dataMem[cat(m_popFifoSelector, popGetPtr.value)];
			IF(m_mustPop)
				m_getPtrMem[m_popFifoSelector] = popGetPtr.increment();
		}

		HCL_NAMED(m_empty);
		HCL_NAMED(m_mustPop);

		m_full = isFull(pushPutPtr, pushGetPtr); HCL_NAMED(m_full);

		IF(m_mustPush & !m_full) {
			m_dataMem[cat(m_pushFifoSelector, pushPutPtr.value)] = m_pushData;
			m_putPtrMem[m_pushFifoSelector] = pushPutPtr.increment();
		}
	}
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::internal::fifoPointer_t, trick, value);


