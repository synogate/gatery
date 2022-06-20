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

#include <gatery/frontend/tech/TechnologyCapabilities.h>

namespace gtry::scl
{

	class FifoMeta : public hlim::NodeGroupMetaInfo
	{
		public:
			FifoCapabilities::Choice fifoChoice;
			std::vector<std::pair<std::string, std::string>> almostFullSignalLevel;
			std::vector<std::pair<std::string, std::string>> almostEmptySignalLevel;
	};

	template<typename TData>
	class Fifo
	{
	public:
		Fifo() : m_area("scl_fifo") { m_area.getNodeGroup()->createMetaInfo<FifoMeta>(); }
		Fifo(size_t minDepth, TData ref) : Fifo() { setup(minDepth, std::move(ref)); }
		void setup(size_t minDepth, TData ref);

		size_t depth();

		// push clock domain
		void push(TData data);
		const Bit& full() const { return m_pushFull; }
		Bit almostFull(const UInt& level);

		// pop clock domain
		TData peak() const;
		void pop();
		const Bit& empty() const { return m_popEmpty; }
		Bit almostEmpty(const UInt& level);

	protected:
		virtual void selectFifo(size_t minDepth, const TData& ref);

		virtual void generate();
		virtual void generateCdc(const UInt& pushPut, UInt& pushGet, UInt& popPut, const UInt& popGet);
		virtual UInt generatePush(Memory<TData>& mem, const UInt& getAddr);
		virtual UInt generatePop(const Memory<TData>& mem, const UInt& putAddr);

		Area m_area;

		std::optional<Clock> m_pushClock;
		Bit m_pushFull;
		Bit m_pushValid;
		TData m_pushData;
		UInt m_pushSize;

		std::optional<Clock> m_popClock;
		Bit m_popEmpty;
		Bit m_popValid;
		TData m_peakData;
		UInt m_popSize;

	private:
		bool m_hasSetup = false;
	};

	template<typename TData>
	inline size_t Fifo<TData>::depth() {
		auto *meta = dynamic_cast<FifoMeta*>(m_area.getNodeGroup()->getMetaInfo());
		FifoCapabilities::Choice &fifoChoice = meta->fifoChoice;
		return fifoChoice.readDepth;
	}

	template<typename TData>
	inline void Fifo<TData>::setup(size_t minDepth, TData ref)
	{
		HCL_DESIGNCHECK_HINT(!m_hasSetup, "fifo already initialized");
		m_hasSetup = true;

		auto scope = m_area.enter();
		selectFifo(minDepth, ref);

		m_peakData = constructFrom(ref);
		HCL_NAMED(m_peakData);

		const auto ctrWidth = BitWidth::count(depth()) + 1;
		m_popSize = ctrWidth;
		HCL_NAMED(m_popSize);
		m_pushSize = ctrWidth;
		HCL_NAMED(m_pushSize);

		m_popValid = '0';
		m_pushValid = '0';
		m_pushData = dontCare(ref);
	}

	template<typename TData>
	inline void Fifo<TData>::push(TData data)
	{
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasSetup, "fifo not initialized");

		const bool hadPush = m_pushClock.has_value();
		m_pushClock = ClockScope::getClk();
		m_pushValid = !m_pushFull; // TODO: assert
		m_pushData = data;

		if(!hadPush && m_popClock.has_value())
			generate();
	}

	template<typename TData>
	inline TData gtry::scl::Fifo<TData>::peak() const
	{
		HCL_DESIGNCHECK_HINT(m_hasSetup, "fifo not initialized");
		return m_peakData;
	}

	template<typename TData>
	inline void Fifo<TData>::pop()
	{
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasSetup, "fifo not initialized");

		const bool hadPop = m_popClock.has_value();
		m_popClock = ClockScope::getClk();
		m_popValid = !m_popEmpty; // TODO: assert

		if(!hadPop && m_pushClock.has_value())
			generate();
	}

	template<typename TData>
	inline Bit gtry::scl::Fifo<TData>::almostEmpty(const UInt& level) 
	{ 
		auto *meta = dynamic_cast<FifoMeta*>(m_area.getNodeGroup()->getMetaInfo());
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasSetup, "fifo not initialized");

		std::string levelName = (boost::format("almost_empty_level_%d") % meta->almostEmptySignalLevel.size()).str();
		std::string signalName = (boost::format("almost_empty_%d") % meta->almostEmptySignalLevel.size()).str();
		meta->almostEmptySignalLevel.push_back({signalName, levelName});

		UInt namedLevel = level;
		namedLevel.setName(levelName);

		Bit ae = reg(m_popSize <= namedLevel, '1');
		ae.setName(signalName);
		return ae;
	}

	template<typename TData>
	inline Bit gtry::scl::Fifo<TData>::almostFull(const UInt& level) 
	{ 
		auto *meta = dynamic_cast<FifoMeta*>(m_area.getNodeGroup()->getMetaInfo());
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasSetup, "fifo not initialized");

		std::string levelName = (boost::format("almost_full_level_%d") % meta->almostFullSignalLevel.size()).str();
		std::string signalName = (boost::format("almost_full_%d") % meta->almostFullSignalLevel.size()).str();
		meta->almostFullSignalLevel.push_back({signalName, levelName});

		HCL_ASSERT_HINT(meta->fifoChoice.readWidth == meta->fifoChoice.writeWidth, "Almost full level computation assumes no mixed read/write widths");
		UInt namedLevel = meta->fifoChoice.readDepth - level;
		namedLevel.setName(levelName);

		Bit af = reg(m_pushSize >= namedLevel, '0');
		af.setName(signalName);
		return af; 
	}

	template<typename TData>
	inline void Fifo<TData>::selectFifo(size_t minDepth, const TData& ref)
	{
		auto wordWidth = width(ref);

		FifoCapabilities::Request fifoRequest;
		fifoRequest.readDepth.atLeast(minDepth);
		fifoRequest.readWidth = wordWidth.value;
		fifoRequest.writeWidth = wordWidth.value;
		fifoRequest.outputIsFallthrough = true;
		fifoRequest.singleClock = true;
		fifoRequest.useECCEncoder = false;
		fifoRequest.useECCDecoder = false;
		fifoRequest.latency_write_empty = 1;
		fifoRequest.latency_read_empty = 1;
		fifoRequest.latency_write_full = 1;
		fifoRequest.latency_read_full = 1;
		fifoRequest.latency_write_almostEmpty = 1;
		fifoRequest.latency_read_almostEmpty = 1;
		fifoRequest.latency_write_almostFull = 1;
		fifoRequest.latency_read_almostFull = 1;

		FifoCapabilities::Choice choice = TechnologyScope::getCap<FifoCapabilities>().select(fifoRequest);
		HCL_DESIGNCHECK_HINT(utils::isPow2(choice.readDepth), "The SCL fifo implementation only works for power of 2 depths!");

		auto* meta = dynamic_cast<FifoMeta*>(m_area.getNodeGroup()->getMetaInfo());
		meta->fifoChoice = choice;
	}

	template<typename TData>
	inline void Fifo<TData>::generate()
	{
		auto scopeLock = ConditionalScope::lock(); // exit conditionals

		Memory<TData> mem{ depth(), m_peakData };
		mem.setType(MemType::DONT_CARE, 1);
		mem.setName("scl_fifo_memory");

		const BitWidth ctrWidth = mem.addressWidth() + 1;
		UInt pushGet = ctrWidth;
		HCL_NAMED(pushGet);
		UInt popPut = ctrWidth;
		HCL_NAMED(popPut);

		UInt pushPut = generatePush(mem, pushGet);
		HCL_NAMED(pushPut);
		UInt popGet = generatePop(mem, popPut);
		HCL_NAMED(popGet);

		// TODO: check for clock domain crossing
		if(true)
		{
			pushGet = popGet;
			popPut = pushPut;
		}
		else
		{
			generateCdc(pushPut, pushGet, popPut, popGet);
		}

		m_pushFull = reg(pushPut.msb() != pushGet.msb() & pushPut(0, -1) == pushGet(0, -1), '0');
		HCL_NAMED(m_pushFull);

		m_popEmpty = reg(popPut.msb() == popGet.msb() & popPut(0, -1) == popGet(0, -1), '1');
		HCL_NAMED(m_popEmpty);

		m_pushSize = pushPut - pushGet;
		m_popSize = popPut - popGet;
	}

	template<typename TData>
	inline void Fifo<TData>::generateCdc(const UInt& pushPut, UInt& pushGet, UInt& popPut, const UInt& popGet)
	{
		HCL_ASSERT(!"no impl");
		// TODO: implement gray counter synchronizer and constraints
	}

	template<typename TData>
	inline UInt Fifo<TData>::generatePush(Memory<TData>& mem, const UInt& get)
	{
		HCL_NAMED(m_pushValid);
		HCL_NAMED(m_pushData);

		UInt put = get.width();
		put = reg(put, 0, { .clock = *m_pushClock });

		IF(m_pushValid)
		{
			mem[put(0, -1)] = m_pushData;
			put += 1;
		}
		return put;
	}

	template<typename TData>
	inline UInt Fifo<TData>::generatePop(const Memory<TData>& mem, const UInt& put)
	{
		HCL_NAMED(m_popValid);

		UInt get = put.width();
		get = reg(get, 0, { .clock = *m_popClock });

		IF(m_popValid)
			get += 1;
		
		m_peakData = reg(mem[get(0, -1)], { .clock = *m_popClock, .allowRetimingBackward = true });
		return get;
	}
}
