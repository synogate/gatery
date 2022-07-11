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

	template<Signal TData>
	class Fifo
	{
	public:
		Fifo() : m_area("scl_fifo") { m_area.getNodeGroup()->createMetaInfo<FifoMeta>(); }
		Fifo(const Fifo&) = delete;
		Fifo(Fifo&&) = default;
		explicit Fifo(size_t minDepth, const TData& ref = TData{}) : Fifo() { setup(minDepth, std::move(ref)); }
		~Fifo() noexcept(false);

		void setup(size_t minDepth, const TData& ref = TData{});
		virtual void generate();

		size_t depth();
		BitWidth wordWidth() const { return width(m_peakData); }

		// push clock domain
		void push(TData data);
		const Bit& full() const { return m_pushFull; }
		Bit almostFull(const UInt& level) { m_almostFulls.push_back({Bit(), level}); return m_almostFulls.back().first; }

		// pop clock domain
		TData peak() const;
		void pop();
		const Bit& empty() const { return m_popEmpty; }
		Bit almostEmpty(const UInt& level) { m_almostEmpties.push_back({Bit(), level}); return m_almostEmpties.back().first; }

	protected:
		virtual void selectFifo(size_t minDepth);

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

		size_t m_minDepth = 0;

		std::list<std::pair<Bit, UInt>> m_almostFulls;
		std::list<std::pair<Bit, UInt>> m_almostEmpties;

		virtual Bit buildAlmostFull(UInt level);
		virtual Bit buildAlmostEmpty(UInt level);
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
		auto* meta = dynamic_cast<FifoMeta*>(m_area.getNodeGroup()->getMetaInfo());
		HCL_DESIGNCHECK_HINT(meta != nullptr, "Fifo's depth can only be determined after ::generate() call.");
		FifoCapabilities::Choice& fifoChoice = meta->fifoChoice;
		return fifoChoice.readDepth;
	}

	template<Signal TData>
	inline void Fifo<TData>::setup(size_t minDepth, const TData& ref)
	{
		HCL_DESIGNCHECK_HINT(!m_hasSetup, "fifo already initialized");
		m_hasSetup = true;

		m_minDepth = minDepth;

		auto scope = m_area.enter();

		m_peakData = constructFrom(ref);
		HCL_NAMED(m_peakData);

		m_popValid = '0';
		m_pushValid = '0';
		m_pushData = dontCare(ref);
	}

	template<Signal TData>
	inline void Fifo<TData>::push(TData data)
	{
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasSetup, "fifo not initialized");

		m_pushClock = ClockScope::getClk();
		m_pushValid = !m_pushFull;
		m_pushData = data;
	}

	template<Signal TData>
	inline TData gtry::scl::Fifo<TData>::peak() const
	{
		HCL_DESIGNCHECK_HINT(m_hasSetup, "fifo not initialized");
		return m_peakData;
	}

	template<Signal TData>
	inline void Fifo<TData>::pop()
	{
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasSetup, "fifo not initialized");

		m_popClock = ClockScope::getClk();
		m_popValid = !m_popEmpty;
	}

	template<Signal TData>
	Bit gtry::scl::Fifo<TData>::buildAlmostEmpty(UInt level)
	{
		auto* meta = dynamic_cast<FifoMeta*>(m_area.getNodeGroup()->getMetaInfo());
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasSetup, "fifo not initialized");

		std::string levelName = (boost::format("almost_empty_level_%d") % meta->almostEmptySignalLevel.size()).str();
		std::string signalName = (boost::format("almost_empty_%d") % meta->almostEmptySignalLevel.size()).str();
		meta->almostEmptySignalLevel.push_back({ signalName, levelName });

		level.setName(levelName);

		Bit ae = reg(m_popSize <= level, '1');
		ae.setName(signalName);
		return ae;
	}

	template<Signal TData>
	Bit gtry::scl::Fifo<TData>::buildAlmostFull(UInt level)
	{
		auto* meta = dynamic_cast<FifoMeta*>(m_area.getNodeGroup()->getMetaInfo());
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasSetup, "fifo not initialized");

		std::string levelName = (boost::format("almost_full_level_%d") % meta->almostFullSignalLevel.size()).str();
		std::string signalName = (boost::format("almost_full_%d") % meta->almostFullSignalLevel.size()).str();
		meta->almostFullSignalLevel.push_back({ signalName, levelName });

		HCL_ASSERT_HINT(meta->fifoChoice.readWidth == meta->fifoChoice.writeWidth, "Almost full level computation assumes no mixed read/write widths");
		UInt namedLevel = meta->fifoChoice.readDepth - level;
		namedLevel.setName(levelName);

		Bit af = reg(m_pushSize >= namedLevel, '0');
		af.setName(signalName);
		return af;
	}

	template<Signal TData>
	inline void Fifo<TData>::selectFifo(size_t minDepth)
	{
		auto width = wordWidth();

		FifoCapabilities::Request fifoRequest;
		// defaults
		fifoRequest.readDepth.atLeast(minDepth);
		fifoRequest.readWidth = width.value;
		fifoRequest.writeWidth = width.value;
		fifoRequest.outputIsFallthrough = true;
		fifoRequest.singleClock = true;
		fifoRequest.useECCEncoder = false;
		fifoRequest.useECCDecoder = false;
		fifoRequest.latency_write_empty.atLeast(1);
		fifoRequest.latency_read_empty = 1;
		fifoRequest.latency_write_full = 1;
		fifoRequest.latency_read_full.atLeast(1);
		fifoRequest.latency_write_almostEmpty.atLeast(1);
		fifoRequest.latency_read_almostEmpty = 1;
		fifoRequest.latency_write_almostFull = 1;
		fifoRequest.latency_read_almostFull.atLeast(1);


		if (m_pushClock->getClk()->getClockPinSource() != m_popClock->getClk()->getClockPinSource() ||
			m_pushClock->getClk()->getTriggerEvent() != m_popClock->getClk()->getTriggerEvent()) {

			// For now, don't differentiate between phase aligned integer multiples and completely unrelated clocks

			fifoRequest.outputIsFallthrough = false;
			fifoRequest.singleClock = false;
			fifoRequest.latency_write_empty.atLeast(4);
			fifoRequest.latency_read_empty = 1;
			fifoRequest.latency_write_full = 1;
			fifoRequest.latency_read_full.atLeast(4);
			fifoRequest.latency_write_almostEmpty.atLeast(4);
			fifoRequest.latency_read_almostEmpty = 1;
			fifoRequest.latency_write_almostFull = 1;
			fifoRequest.latency_read_almostFull.atLeast(4);
		}


		FifoCapabilities::Choice choice = TechnologyScope::getCap<FifoCapabilities>().select(fifoRequest);
		HCL_DESIGNCHECK_HINT(utils::isPow2(choice.readDepth), "The SCL fifo implementation only works for power of 2 depths!");

		auto* meta = dynamic_cast<FifoMeta*>(m_area.getNodeGroup()->getMetaInfo());
		meta->fifoChoice = choice;
	}

	template<Signal TData>
	inline void Fifo<TData>::generate()
	{
		HCL_DESIGNCHECK_HINT(!m_hasGenerate, "generate called twice");
		m_hasGenerate = true;

		auto scope = m_area.enter();
		auto scopeLock = ConditionalScope::lock(); // exit conditionals

		selectFifo(m_minDepth);
		auto* meta = dynamic_cast<FifoMeta*>(m_area.getNodeGroup()->getMetaInfo());


		Memory<TData> mem{ depth(), m_peakData };
		mem.setType(MemType::DONT_CARE, 1);
		mem.setName("scl_fifo_memory");
		if (!meta->fifoChoice.singleClock)
			mem.noConflicts();

		const BitWidth ctrWidth = mem.addressWidth() + 1;

		m_popSize = ctrWidth;
		HCL_NAMED(m_popSize);
		m_pushSize = ctrWidth;
		HCL_NAMED(m_pushSize);

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
			HCL_DESIGNCHECK_HINT(meta->fifoChoice.latency_read_full == meta->fifoChoice.latency_read_almostFull, 
				"Technology mapping yielded invalid choice, only supporting equal latencies for latency_read_full and latency_read_almostFull.");
			pushGet = popGet;
			for ([[maybe_unused]] auto i : utils::Range(meta->fifoChoice.latency_read_full-1))
				pushGet = reg(pushGet, 0);

			HCL_DESIGNCHECK_HINT(meta->fifoChoice.latency_write_empty == meta->fifoChoice.latency_write_almostEmpty, 
				"Technology mapping yielded invalid choice, only supporting equal latencies for latency_write_empty and latency_write_almostEmpty.");
			popPut = pushPut;
			for ([[maybe_unused]] auto i : utils::Range(meta->fifoChoice.latency_write_empty-1))
				popPut = reg(popPut, 0);
		} else {
			generateCdc(pushPut, pushGet, popPut, popGet);
		}

		for (auto &p : m_almostFulls)
			p.first = buildAlmostFull(p.second);

		for (auto &p : m_almostEmpties)
			p.first = buildAlmostEmpty(p.second);
	}

	template<Signal TData>
	inline void Fifo<TData>::generateCdc(const UInt& pushPut, UInt& pushGet, UInt& popPut, const UInt& popGet)
	{
		auto scope = m_area.enter("scl_fifo_cdc");
		auto* meta = dynamic_cast<FifoMeta*>(m_area.getNodeGroup()->getMetaInfo());

		HCL_DESIGNCHECK_HINT(meta->fifoChoice.latency_read_full == meta->fifoChoice.latency_read_almostFull, 
			"Technology mapping yielded invalid choice, only supporting equal latencies for latency_read_full and latency_read_almostFull.");
		HCL_DESIGNCHECK_HINT(meta->fifoChoice.latency_read_full >= 4, 
			"Insufficient latency_read_full chosen by technology mapping to build proper synchronizer chain.");
		pushGet = grayCodeSynchronize(popGet, *m_popClock, *m_pushClock, meta->fifoChoice.latency_read_full-2, true);

		HCL_DESIGNCHECK_HINT(meta->fifoChoice.latency_write_empty == meta->fifoChoice.latency_write_almostEmpty, 
			"Technology mapping yielded invalid choice, only supporting equal latencies for latency_write_empty and latency_write_almostEmpty.");
		HCL_DESIGNCHECK_HINT(meta->fifoChoice.latency_write_empty >= 4, 
			"Insufficient latency_write_empty chosen by technology mapping to build proper synchronizer chain.");
		popPut = grayCodeSynchronize(pushPut, *m_pushClock, *m_popClock, meta->fifoChoice.latency_write_empty-2, true);
	}

	template<Signal TData>
	inline UInt Fifo<TData>::generatePush(Memory<TData>& mem, const UInt& get)
	{
		HCL_NAMED(m_pushValid);
		HCL_NAMED(m_pushData);

		ClockScope clk{ *m_pushClock };

		UInt put = get.width();
		put = reg(put, 0);

		IF(m_pushValid)
		{
			mem[put(0, -1)] = m_pushData;
			put += 1;
		}

		m_pushSize = put - get;
		m_pushFull = reg(put.msb() != get.msb() & put(0, -1) == get(0, -1), '0');
		HCL_NAMED(m_pushFull);

		return put;
	}

	template<Signal TData>
	inline UInt Fifo<TData>::generatePop(const Memory<TData>& mem, const UInt& put)
	{
		HCL_NAMED(m_popValid);
		ClockScope clk{ *m_popClock };

		UInt get = put.width();
		get = reg(get, 0);

		IF(m_popValid)
			get += 1;
		
		m_peakData = reg(mem[get(0, -1)], { .allowRetimingBackward = true });

		m_popSize = put - get;
		m_popEmpty = reg(put.msb() == get.msb() & put(0, -1) == get(0, -1), '1');
		HCL_NAMED(m_popEmpty);

		return get;
	}
}
