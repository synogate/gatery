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

		size_t getDepth();

		// NOTE: always push before pop for correct conflict resolution
		// TODO: fix above note by adding explicit write before read conflict resulution to bram
		void push(TData data, Bit valid);
		void pop(TData& data, Bit ready);

		const Bit& empty() const { return m_empty; }
		const Bit& full() const { return m_full; }

		Bit almostEmpty(const UInt& level);
		Bit almostFull(const UInt& level);
	private:
		Area m_area;

		Memory<UInt>	m_mem;
		UInt m_put;
		UInt m_get;
		UInt m_size;

		Bit m_full;
		Bit m_empty;

		std::optional<TData> m_defaultValue;

		bool m_hasMem = false;
		bool m_hasPush = false;
		bool m_hasPop = false;
	};

	template<typename TData>
	inline size_t Fifo<TData>::getDepth() {
		auto *meta = dynamic_cast<FifoMeta*>(m_area.getNodeGroup()->getMetaInfo());
		FifoCapabilities::Choice &fifoChoice = meta->fifoChoice;
		return fifoChoice.readDepth;
	}

	template<typename TData>
	inline void Fifo<TData>::setup(size_t minDepth, TData ref)
	{
		auto scope = m_area.enter();

		m_defaultValue = std::move(ref);
		auto wordWidth = width(*m_defaultValue);

		FifoCapabilities::Request fifoRequest;
		fifoRequest.readDepth.atLeast((uint32_t)minDepth);
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

		auto *meta = dynamic_cast<FifoMeta*>(m_area.getNodeGroup()->getMetaInfo());
		FifoCapabilities::Choice &fifoChoice = meta->fifoChoice;

		fifoChoice = TechnologyScope::getCap<FifoCapabilities>().select(fifoRequest);
		HCL_DESIGNCHECK_HINT(utils::isPow2(fifoChoice.readDepth), "The SCL fifo implementation only works for power of 2 depths!");

		HCL_DESIGNCHECK_HINT(!m_hasMem, "fifo already initialized");
		m_hasMem = true;
		m_mem.setup(fifoChoice.readDepth, wordWidth);
		m_mem.setType(MemType::DONT_CARE, 1);
		m_mem.setName("scl_fifo_memory");

		const BitWidth ctrWidth = m_mem.addressWidth() + 1;
		m_put = ctrWidth;
		m_get = ctrWidth;
		m_size = m_put - m_get;
		HCL_NAMED(m_size);

		Bit eq = m_put(0, -1) == m_get(0, -1);
		HCL_NAMED(eq);

		m_full = eq & (m_put.msb() != m_get.msb());
		m_empty = eq & (m_put.msb() == m_get.msb());

		m_full = reg(m_full, '0');
		m_empty = reg(m_empty, '1');
		setName(m_full, "full");
		setName(m_empty, "empty");

	}

	template<typename TData>
	inline void Fifo<TData>::push(TData data, Bit valid)
	{
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasMem, "fifo not initialized");
		HCL_DESIGNCHECK_HINT(!m_hasPush, "fifo push port already constructed");
		m_hasPush = true;

		setName(data, "in_data");
		UInt packedData = pack(data);
		setName(packedData, "in_data_packed");
		setName(valid, "in_valid");

		sim_assert(!valid | !m_full) << "push into full fifo";

		UInt put = m_put.width();
		put = reg(put, 0);
		HCL_NAMED(put);

		IF(valid)
		{
			m_mem[put(0, -1)] = packedData;
			put += 1;
		}

		m_put = put;
		HCL_NAMED(m_put);
	}

	template<typename TData>
	inline void Fifo<TData>::pop(TData& data, Bit ready)
	{
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasMem, "fifo not initialized");
		HCL_DESIGNCHECK_HINT(!m_hasPop, "fifo pop port already constructed");
		m_hasPop = true;

		setName(ready, "out_ready");

		sim_assert(!ready | !m_empty) << "pop from empty fifo";

		UInt get = m_get.width();
		get = reg(get, 0);
		HCL_NAMED(get);

		IF(ready)
			get += 1;

		UInt packedData = reg(m_mem[get(0, -1)], {.allowRetimingBackward=true});

		m_get = get;
		HCL_NAMED(m_get);

		setName(packedData, "out_data_packed");
		*m_defaultValue = constructFrom(data);
		unpack(packedData, data);
		setName(data, "out_data");
	}

	template<typename TData>
	inline Bit gtry::scl::Fifo<TData>::almostEmpty(const UInt& level) 
	{ 
		auto *meta = dynamic_cast<FifoMeta*>(m_area.getNodeGroup()->getMetaInfo());
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasMem, "fifo not initialized");

		std::string levelName = (boost::format("almost_empty_level_%d") % meta->almostEmptySignalLevel.size()).str();
		std::string signalName = (boost::format("almost_empty_%d") % meta->almostEmptySignalLevel.size()).str();
		meta->almostEmptySignalLevel.push_back({signalName, levelName});

		UInt namedLevel = level;
		namedLevel.setName(levelName);

		Bit ae = reg(m_size <= namedLevel, '1');
		ae.setName(signalName);
		return ae;
	}

	template<typename TData>
	inline Bit gtry::scl::Fifo<TData>::almostFull(const UInt& level) 
	{ 
		auto *meta = dynamic_cast<FifoMeta*>(m_area.getNodeGroup()->getMetaInfo());
		auto scope = m_area.enter();
		HCL_DESIGNCHECK_HINT(m_hasMem, "fifo not initialized");

		std::string levelName = (boost::format("almost_full_level_%d") % meta->almostFullSignalLevel.size()).str();
		std::string signalName = (boost::format("almost_full_%d") % meta->almostFullSignalLevel.size()).str();
		meta->almostFullSignalLevel.push_back({signalName, levelName});

		HCL_ASSERT_HINT(meta->fifoChoice.readWidth == meta->fifoChoice.writeWidth, "Almost full level computation assumes no mixed read/write widths");
		UInt namedLevel = meta->fifoChoice.readDepth - level;
		namedLevel.setName(levelName);

		Bit af = reg(m_size >= namedLevel, '0');
		af.setName(signalName);
		return af; 
	}
}

