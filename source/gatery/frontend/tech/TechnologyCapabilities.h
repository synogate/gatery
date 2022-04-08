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

#include "../Scope.h"

#include <gatery/utils/BitFlags.h>

namespace gtry {

enum class Preference {
	DONTCARE,
	SPEED,
	AREA,
	SPECIFIC_VALUE,
	MIN_VALUE,
	MAX_VALUE,
};

template<typename BaseType>
struct Option {
	Preference choice = Preference::DONTCARE;
	BaseType value;

	/// For DONTCARE, SPEED, and AREA
	void operator=(Preference choice) { this->choice = choice; }

	void optimizeDontCare() { choice = Preference::DONTCARE; }
	void optimizeSpeed() { choice = Preference::SPEED; }
	void optimizeArea() { choice = Preference::AREA; }

	void operator=(BaseType v) { choice = Preference::SPECIFIC_VALUE; value = v; }
	void atLeast(BaseType v) { choice = Preference::MIN_VALUE; value = v; }
	void atMost(BaseType v) { choice = Preference::MAX_VALUE; value = v; }
};

namespace details {
	template<typename BaseType>
	struct RequestWrapper {	
		using wrap = Option<BaseType>;
	};

	template<typename BaseType>
	struct ChoiceWrapper {	
		using wrap = BaseType;
	};
}


class Capabilities {
	public:
		virtual ~Capabilities() = default;
};


class SerdesCapabilities : public Capabilities {
	public:
		enum DataRate {
			SDR,
			DDR,
		};

		template<template <typename> class Wrapper>
		struct Settings {
			typename Wrapper<unsigned>::wrap width;
			typename Wrapper<DataRate>::wrap dataRate;
		};

		using Request = Settings<details::RequestWrapper>;
		using Choice = Settings<details::ChoiceWrapper>;

	protected:
};

class IOCapabilities : public Capabilities {
};

class MemoryCapabilities : public Capabilities {
	public:
		// More options in git history!
		
		enum class SizeCategory {
			SMALL,  // LUTRAMS, MLABS, ...
			MEDIUM, // BlockRAMs, BRAM, MxK, ...
			LARGE   // UltraRAMS, eSRAM, ...
		};

		struct Request {
			uint64_t size;
			uint64_t maxDepth;
			utils::BitFlags<SizeCategory> sizeCategory = utils::BitFlags<SizeCategory>::ALL;
		};

		struct Choice {
			bool inputRegs;
			size_t outputRegs;
			size_t totalReadLatency;
		};

		Choice select(const Request &request) const;
		virtual Choice select(hlim::NodeGroup *group, const Request &request) const;

		static const char *getName() { return "mem"; }
	protected:
};

class FifoCapabilities : public Capabilities {
	public:
		template<template <typename> class Wrapper>
		struct Settings {
			typename Wrapper<size_t>::wrap readWidth;
			typename Wrapper<size_t>::wrap readDepth;
			typename Wrapper<size_t>::wrap writeWidth;

			typename Wrapper<bool>::wrap outputIsFallthrough;
			typename Wrapper<bool>::wrap singleClock;

			typename Wrapper<bool>::wrap useECCEncoder;
			typename Wrapper<bool>::wrap useECCDecoder;

			typename Wrapper<size_t>::wrap latency_write_empty;
			typename Wrapper<size_t>::wrap latency_read_empty;
			typename Wrapper<size_t>::wrap latency_write_full;
			typename Wrapper<size_t>::wrap latency_read_full;
			typename Wrapper<size_t>::wrap latency_write_almostEmpty;
			typename Wrapper<size_t>::wrap latency_read_almostEmpty;
			typename Wrapper<size_t>::wrap latency_write_almostFull;
			typename Wrapper<size_t>::wrap latency_read_almostFull;
		};

		using Request = Settings<details::RequestWrapper>;
		using Choice = Settings<details::ChoiceWrapper>;

		virtual ~FifoCapabilities() = default;

		Choice select(const Request &request) const;
		virtual Choice select(hlim::NodeGroup *group, const Request &request) const;

		static const char *getName() { return "fifo"; }
	protected:
};

class TechnologyCapabilities {
	public:
		template<class Cap>
		const Cap &getCap() const {
			auto it = m_capabilities.find(Cap::getName());
			HCL_DESIGNCHECK_HINT(it != m_capabilities.end(), std::string("Could not find handler for tech capability ") + Cap::getName());
			return dynamic_cast<const Cap&>(*it->second);
		}

		template<class Cap>
		void registerCap(Cap *cap) { m_capabilities[Cap::getName()] = cap; }
	protected:
		std::map<std::string, Capabilities*, std::less<>> m_capabilities;
};


class TechnologyScope : public BaseScope<TechnologyScope> {
	public:
		static TechnologyScope *get() { return m_currentScope; }

		TechnologyScope(const TechnologyCapabilities &techCaps) : m_techCaps(techCaps) { }

		inline const TechnologyCapabilities &getTechCaps() const { return m_techCaps; }

		template<class Cap>
		static const Cap &getCap() { 
			HCL_ASSERT_HINT(m_currentScope != nullptr, "No technology scope active!");
			return m_currentScope->m_techCaps.getCap<Cap>();
		}
	protected:
		const TechnologyCapabilities &m_techCaps;
};

}
