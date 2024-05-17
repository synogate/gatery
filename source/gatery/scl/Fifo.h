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
#include <gatery/scl/cdc.h>

#include <gatery/frontend/tech/TechnologyCapabilities.h>
#include <gatery/frontend/EventStatistics.h>

namespace gtry::scl
{
	struct FifoMeta : hlim::NodeGroupMetaInfo
	{
		FifoCapabilities::Choice fifoChoice;
		std::vector<std::pair<std::string, std::string>> almostFullSignalLevel;
		std::vector<std::pair<std::string, std::string>> almostEmptySignalLevel;
	};

	struct TrueAfterMove
	{
		TrueAfterMove() = default;
		TrueAfterMove(TrueAfterMove&& o) noexcept
		{
			val = o.val;
			o.val = true;
		}

		operator bool() const { return val; }
		TrueAfterMove& operator = (bool nval) { val = nval; return *this; }

		bool val = false;
	};

	/**
	* @brief Fallthrough-ness in the gatery sense means the ability to bypass the fifo entirely when it is empty, 
	* resulting in no additional cycles of latency in this specific case. A non-fallthrough fifo systematically 
	* adds at least one cycle of latency between the input and the output of the fifo.
	*/
	using FifoLatency = Option<size_t>;

	template<Signal TData>
	class Fifo
	{
	public:
		Fifo() : m_area("scl_fifo") { m_area.template createMetaInfo<FifoMeta>(); }
		Fifo(const Fifo&) = delete;
		Fifo(Fifo&&) = default;
		explicit Fifo(size_t minDepth, const TData& ref = TData{}, FifoLatency latency = FifoLatency::DontCare()) : Fifo() { setup(minDepth, std::move(ref), latency); }
		~Fifo() noexcept(false);

		void setup(size_t minDepth, const TData& ref = TData{}, FifoLatency latency = FifoLatency::DontCare());
		virtual void generate();

		size_t depth();
		BitWidth wordWidth() const { return width(m_peekData); }

		// push clock domain
		void push(TData data);
		const Bit& full() const { return m_pushFull; }
		Bit almostFull(const UInt& level);

		// pop clock domain
		TData peek() const;
		void pop();
		const Bit& empty() const { return m_popEmpty; }
		Bit almostEmpty(const UInt& level);

	protected:
		virtual void initialFifoSelection(size_t minDepth, const TData& ref, FifoLatency latencyWriteToRead);
		virtual void finalFifoSelection();

		virtual void generateCdc(const UInt& pushPut, UInt& pushGet, UInt& popPut, const UInt& popGet);
		virtual UInt generatePush(Memory<TData>& mem, UInt getAddr);
		virtual UInt generatePop(const Memory<TData>& mem, UInt putAddr);

		Area m_area;

		Option<size_t> m_latencyWriteToRead;

		std::optional<Clock> m_pushClock;
		Bit m_pushFull;
		Bit m_pushValid;
		TData m_pushData;
		UInt m_pushSize;

		std::optional<Clock> m_popClock;
		Bit m_popEmpty;
		Bit m_popValid;
		TData m_peekData;
		UInt m_popSize;
	private:
		bool m_hasSetup = false;
		TrueAfterMove m_hasGenerate;
	};

	template<Signal TData>
	inline Fifo<TData>::~Fifo() noexcept(false)
	{
		HCL_ASSERT(m_hasGenerate);
	}

	template<Signal TData>
	inline size_t Fifo<TData>::depth()
	{
		auto* meta = dynamic_cast<FifoMeta*>(m_area.metaInfo());
		FifoCapabilities::Choice& fifoChoice = meta->fifoChoice;
		return fifoChoice.readDepth;
	}

	template<Signal TData>
	inline void Fifo<TData>::setup(size_t minDepth, const TData& ref, FifoLatency latency)
	{
		HCL_DESIGNCHECK_HINT(!m_hasSetup, "fifo already initialized");
		m_hasSetup = true;

		m_popValid = '0';
		m_pushValid = '0';
		m_pushData = dontCare(ref);

		auto scope = m_area.enter();
		initialFifoSelection(minDepth, ref, latency);

		m_peekData = constructFrom(ref);
		HCL_NAMED(m_peekData);

		const auto ctrWidth = BitWidth::count(depth()) + 1;
		m_popSize = ctrWidth;
		HCL_NAMED(m_popSize);
		m_pushSize = ctrWidth;
		HCL_NAMED(m_pushSize);
	}

	template<Signal TData>
	inline void Fifo<TData>::push(TData data)
	{
		//auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasSetup, "fifo not initialized");

		m_pushClock = ClockScope::getClk();
		m_pushValid = !m_pushFull;
		m_pushData = data;
	}

	template<Signal TData>
	inline TData gtry::scl::Fifo<TData>::peek() const
	{
		HCL_DESIGNCHECK_HINT(m_hasSetup, "fifo not initialized");
		return m_peekData;
	}

	template<Signal TData>
	inline void Fifo<TData>::pop()
	{
		//auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasSetup, "fifo not initialized");

		m_popClock = ClockScope::getClk();
		m_popValid = !m_popEmpty;
	}

	template<Signal TData>
	inline Bit gtry::scl::Fifo<TData>::almostEmpty(const UInt& level)
	{
		auto* meta = dynamic_cast<FifoMeta*>(m_area.metaInfo());
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasSetup, "fifo not initialized");

		std::string levelName = (boost::format("almost_empty_level_%d") % meta->almostEmptySignalLevel.size()).str();
		std::string signalName = (boost::format("almost_empty_%d") % meta->almostEmptySignalLevel.size()).str();
		meta->almostEmptySignalLevel.push_back({ signalName, levelName });

		UInt namedLevel = level;
		namedLevel.setName(levelName);

		Bit ae = reg(m_popSize <= namedLevel, '1');
		ae.setName(signalName);
		registerEvent(signalName, ae);
		return ae;
	}

	template<Signal TData>
	inline Bit gtry::scl::Fifo<TData>::almostFull(const UInt& level)
	{
		auto* meta = dynamic_cast<FifoMeta*>(m_area.metaInfo());
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasSetup, "fifo not initialized");

		std::string levelName = (boost::format("almost_full_level_%d") % meta->almostFullSignalLevel.size()).str();
		std::string signalName = (boost::format("almost_full_%d") % meta->almostFullSignalLevel.size()).str();
		meta->almostFullSignalLevel.push_back({ signalName, levelName });

		HCL_ASSERT_HINT(meta->fifoChoice.readWidth == meta->fifoChoice.writeWidth, "Almost full level computation assumes no mixed read/write widths");
		UInt namedLevel = meta->fifoChoice.readDepth - zext(level);
		namedLevel.setName(levelName);

		Bit af = reg(m_pushSize >= namedLevel, '0');
		af.setName(signalName);
		registerEvent(signalName, af);
		return af;
	}

	template<Signal TData>
	inline void Fifo<TData>::initialFifoSelection(size_t minDepth, const TData& ref, FifoLatency latencyWriteToRead)
	{
		auto wordWidth = width(ref);

		FifoCapabilities::Request fifoRequest;
		fifoRequest.readDepth.atLeast(minDepth);
		fifoRequest.readWidth = wordWidth.value;
		fifoRequest.writeWidth = wordWidth.value;
		m_latencyWriteToRead = latencyWriteToRead;
		fifoRequest.latency_writeToEmpty = m_latencyWriteToRead;
		fifoRequest.latency_readToFull = m_latencyWriteToRead;
		fifoRequest.latency_writeToAlmostEmpty = m_latencyWriteToRead;
		fifoRequest.latency_readToAlmostFull = m_latencyWriteToRead;		

		FifoCapabilities::Choice choice = TechnologyScope::getCap<FifoCapabilities>().select(fifoRequest);
		HCL_DESIGNCHECK_HINT(utils::isPow2(choice.readDepth), "The SCL fifo implementation only works for power of 2 depths!");

		auto* meta = dynamic_cast<FifoMeta*>(m_area.metaInfo());
		meta->fifoChoice = choice;
	}

	template<Signal TData>
	inline void Fifo<TData>::finalFifoSelection()
	{
		auto* meta = dynamic_cast<FifoMeta*>(m_area.metaInfo());

		FifoCapabilities::Request fifoRequest;
		fifoRequest.readDepth = meta->fifoChoice.readDepth;
		fifoRequest.readWidth = meta->fifoChoice.readWidth;
		fifoRequest.writeWidth = meta->fifoChoice.writeWidth;

		// default to the user requested latency
		fifoRequest.latency_writeToEmpty = m_latencyWriteToRead;
		fifoRequest.latency_readToFull = m_latencyWriteToRead;
		fifoRequest.latency_writeToAlmostEmpty = m_latencyWriteToRead;
		fifoRequest.latency_readToAlmostFull = m_latencyWriteToRead;

		fifoRequest.singleClock = true;

		if (m_pushClock->getClk()->getClockPinSource() != m_popClock->getClk()->getClockPinSource() ||
			m_pushClock->getClk()->getTriggerEvent() != m_popClock->getClk()->getTriggerEvent()) {

			// For now, don't differentiate between phase aligned integer multiples and completely unrelated clocks

			fifoRequest.singleClock = false;

			auto latency = FifoLatency::AtLeast(4).mergeWith(m_latencyWriteToRead);

			if (!latency) {
				dbg::log(dbg::LogMessage(m_area.getNodeGroup()) << dbg::LogMessage::LOG_ERROR << dbg::LogMessage::LOG_DESIGN << "The scl clock domain crossing fifo implementation requires at least four cycles of latency.");
				HCL_DESIGNCHECK_HINT(false, "The scl clock domain crossing fifo implementation requires at least four cycles of latency.");
			} else {
				fifoRequest.latency_writeToEmpty = *latency;
				fifoRequest.latency_readToFull = *latency;
				fifoRequest.latency_writeToAlmostEmpty = *latency;
				fifoRequest.latency_readToAlmostFull = *latency;
			}
		}


		FifoCapabilities::Choice choice = TechnologyScope::getCap<FifoCapabilities>().select(fifoRequest);
		HCL_DESIGNCHECK_HINT(utils::isPow2(choice.readDepth), "The SCL fifo implementation only works for power of 2 depths!");

		meta->fifoChoice = choice;
	}

	template<Signal TData>
	inline void Fifo<TData>::generate()
	{
		HCL_DESIGNCHECK_HINT(!m_hasGenerate, "generate called twice");
		m_hasGenerate = true;

		if (!m_pushClock || !m_popClock)
			return; // no push or pop port, nothing to do

		auto scope = m_area.enter();
		auto scopeLock = ConditionalScope::lock(); // exit conditionals

		finalFifoSelection();
		auto* meta = dynamic_cast<FifoMeta*>(m_area.metaInfo());

		Memory<TData> mem{ depth(), m_peekData };
		mem.setType(MemType::DONT_CARE, 1);
		mem.setName("scl_fifo_memory");
		if (meta->fifoChoice.latency_writeToEmpty > 1)
			mem.noConflicts();


		const BitWidth ctrWidth = mem.addressWidth() + 1;
		UInt pushGet = ctrWidth;
		HCL_NAMED(pushGet);
		UInt popPut = ctrWidth;
		HCL_NAMED(popPut);

		UInt pushPut = generatePush(mem, pushGet);
		HCL_NAMED(pushPut);
		UInt popGet = generatePop(mem, popPut);
		HCL_NAMED(popGet);

		if (meta->fifoChoice.singleClock)
		{
			HCL_DESIGNCHECK_HINT(meta->fifoChoice.latency_readToFull == meta->fifoChoice.latency_readToAlmostFull, 
				"Technology mapping yielded invalid choice, only supporting equal latencies for latency_read_full and latency_read_almostFull.");
			pushGet = popGet;
			for ([[maybe_unused]] auto i : utils::Range(meta->fifoChoice.latency_readToFull-1))
				pushGet = reg(pushGet, 0);

			HCL_DESIGNCHECK_HINT(meta->fifoChoice.latency_writeToEmpty == meta->fifoChoice.latency_writeToAlmostEmpty, 
				"Technology mapping yielded invalid choice, only supporting equal latencies for latency_write_empty and latency_write_almostEmpty.");
			popPut = pushPut;
			for ([[maybe_unused]] auto i : utils::Range(meta->fifoChoice.latency_writeToEmpty-1))
				popPut = reg(popPut, 0);
		} else {
			generateCdc(pushPut, pushGet, popPut, popGet);
		}


	}

	template<Signal TData>
	inline void Fifo<TData>::generateCdc(const UInt& pushPut, UInt& pushGet, UInt& popPut, const UInt& popGet)
	{
		auto scope = m_area.enter("scl_fifo_cdc");
		auto* meta = dynamic_cast<FifoMeta*>(m_area.metaInfo());

		HCL_DESIGNCHECK_HINT(meta->fifoChoice.latency_readToFull == meta->fifoChoice.latency_readToAlmostFull, 
			"Technology mapping yielded invalid choice, only supporting equal latencies for latency_read_full and latency_read_almostFull.");
		HCL_DESIGNCHECK_HINT(meta->fifoChoice.latency_readToFull >= 4, 
			"Insufficient latency_read_full chosen by technology mapping to build proper synchronizer chain.");
		pushGet = synchronizeGrayCode(popGet, ConstUInt(0, popGet.width()), *m_popClock, *m_pushClock, {.outStages = meta->fifoChoice.latency_readToFull-2, .inStage = true});

		HCL_DESIGNCHECK_HINT(meta->fifoChoice.latency_writeToEmpty == meta->fifoChoice.latency_writeToAlmostEmpty, 
			"Technology mapping yielded invalid choice, only supporting equal latencies for latency_write_empty and latency_write_almostEmpty.");
		HCL_DESIGNCHECK_HINT(meta->fifoChoice.latency_writeToEmpty >= 4, 
			"Insufficient latency_write_empty chosen by technology mapping to build proper synchronizer chain.");
		popPut = synchronizeGrayCode(pushPut, ConstUInt(0, popGet.width()), *m_pushClock, *m_popClock, {.outStages = meta->fifoChoice.latency_writeToEmpty-2, .inStage = true});
	}

	template<Signal TData>
	inline UInt Fifo<TData>::generatePush(Memory<TData>& mem, UInt get)
	{
		Area area("pushManagement", true);
		HCL_NAMED(get);

		HCL_NAMED(m_pushValid);
		HCL_NAMED(m_pushData);

		ClockScope clkScope(*m_pushClock);

		UInt put = get.width();
		put = reg(put, 0);
		HCL_NAMED(put);

		IF(m_pushValid)
			mem[put(0, -1_b)] = m_pushData;

		put += m_pushValid;

		m_pushSize = put - get;
		m_pushFull = reg(put.msb() != get.msb() & put(0, -1_b) == get(0, -1_b), '0');
		HCL_NAMED(m_pushFull);

		//registerEvent("full", m_pushFull);

		return put;
	}

	template<Signal TData>
	inline UInt Fifo<TData>::generatePop(const Memory<TData>& mem, UInt put)
	{
		Area area("popManagement", true);
		HCL_NAMED(put);

		HCL_NAMED(m_popValid);
		ClockScope clkScope(*m_popClock);

		UInt get = put.width();
		get = reg(get, 0);

		get += m_popValid;
		HCL_NAMED(get);
		
		m_peekData = reg(mem[get(0, -1_b)], { .allowRetimingBackward = true });

		m_popSize = put - get;
		m_popEmpty = reg(put.msb() == get.msb() & put(0, -1_b) == get(0, -1_b), '1');
		HCL_NAMED(m_popEmpty);

		//registerEvent("empty", m_popEmpty);

		return get;
	}


}


