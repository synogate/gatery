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
	BaseType value = 0;

	Option() = default;
	Option(Preference choice) { (*this) = choice; }
	Option(BaseType v) { (*this) = v; }
	Option(Preference choice, BaseType value) : choice(choice), value(value) { }

	void operator=(Preference choice) { this->choice = choice; }
	void operator=(BaseType v) { choice = Preference::SPECIFIC_VALUE; value = v; }

	bool operator==(BaseType v) const { return choice == Preference::SPECIFIC_VALUE && value == v; }

	void dontCare() { choice = Preference::DONTCARE; }
	void optimizeSpeed() { choice = Preference::SPEED; }
	void optimizeArea() { choice = Preference::AREA; }

	static Option<BaseType> DontCare() { return Preference::DONTCARE; }
	static Option<BaseType> OptimizeSpeed() { return Preference::SPEED; }
	static Option<BaseType> OptimizeArea() { return Preference::AREA; }

	void atLeast(BaseType v) { choice = Preference::MIN_VALUE; value = v; }
	void atMost(BaseType v) { choice = Preference::MAX_VALUE; value = v; }
	static Option<BaseType> AtLeast(BaseType v) { return Option(Preference::MIN_VALUE, v); }
	static Option<BaseType> AtMost(BaseType v) { return Option(Preference::MAX_VALUE, v); }

	BaseType resolveSimpleDefault(BaseType defaultValue) const;
	BaseType resolveToPreferredMinimum(BaseType preferredMinimum) const;
	std::optional<Option<BaseType>> mergeWith(Option<BaseType> other) const;
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


		enum class Mode {
			ROM,
			SIMPLE_DUAL_PORT,
			TRUE_DUAL_PORT
		};


		struct Request {
			uint64_t size = 0;
			uint64_t maxDepth = 0;
			utils::BitFlags<SizeCategory> sizeCategory = utils::BitFlags<SizeCategory>::ALL;
			Mode mode = Mode::SIMPLE_DUAL_PORT;
			bool dualClock = false;
			bool powerOnInitialized = false;
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

			typename Wrapper<bool>::wrap singleClock;

			typename Wrapper<size_t>::wrap latency_writeToEmpty;
			typename Wrapper<size_t>::wrap latency_readToFull;
			typename Wrapper<size_t>::wrap latency_writeToAlmostEmpty;
			typename Wrapper<size_t>::wrap latency_readToAlmostFull;
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









template<typename BaseType>
BaseType Option<BaseType>::resolveSimpleDefault(BaseType defaultValue) const {
	HCL_DESIGNCHECK(choice != Preference::MIN_VALUE && 
					choice != Preference::MAX_VALUE);
	if (choice == Preference::SPECIFIC_VALUE)
		return value;
	else
		return defaultValue;
};

template<typename BaseType>
BaseType Option<BaseType>::resolveToPreferredMinimum(BaseType preferredMinimum) const {
	switch (choice) {
		case Preference::MIN_VALUE: 
			return std::max<size_t>(value, preferredMinimum);
		break;
		case Preference::MAX_VALUE:
			return std::min<size_t>(value, preferredMinimum);
		break;
		case Preference::SPECIFIC_VALUE:
			return value;
		break;
		default:
			return preferredMinimum;
	}
};

template<typename BaseType>
std::optional<Option<BaseType>> Option<BaseType>::mergeWith(Option<BaseType> other) const {
	switch (choice) {
		case Preference::MIN_VALUE: 
			switch (other.choice) {
				case Preference::DONTCARE:
				case Preference::SPEED: 
				case Preference::AREA:
					return *this;
				case Preference::MIN_VALUE: 
					return Option::AtLeast(std::max(value, other.value));
				case Preference::MAX_VALUE:
					if (other.value >= value)
						return std::min(value, other.value);
					else
						return {};
				case Preference::SPECIFIC_VALUE:
					if (other.value >= value)
						return other;
					else
						return {};
			}
		break;
		case Preference::MAX_VALUE:
			switch (other.choice) {
				case Preference::DONTCARE:
				case Preference::SPEED: 
				case Preference::AREA:
					return *this;
				case Preference::MIN_VALUE:
					if (other.value <= value)
						return std::min(value, other.value);
					else
						return {};
				case Preference::MAX_VALUE:
					return Option::AtMost(std::min(value, other.value));
				case Preference::SPECIFIC_VALUE:
					if (other.value <= value)
						return other;
					else
						return {};
			}
		break;
		case Preference::SPECIFIC_VALUE:
			switch (other.choice) {
				case Preference::DONTCARE:
				case Preference::SPEED: 
				case Preference::AREA: 
					return *this;
				case Preference::SPECIFIC_VALUE: 
					if (other.value == value)
						return *this;
					else
						return {};
				case Preference::MIN_VALUE: 
					if (other.value <= value)
						return *this;
					else
						return {};
				case Preference::MAX_VALUE: 
					if (other.value >= value)
						return *this;
					else
						return {};
			}
		break;			
		case Preference::DONTCARE:
			return other;
		case Preference::SPEED:
			switch (other.choice) {
				case Preference::DONTCARE:
				case Preference::SPEED:
					return *this;
				case Preference::AREA: 
					return {};
				case Preference::SPECIFIC_VALUE: 
				case Preference::MIN_VALUE: 
				case Preference::MAX_VALUE: 
					return other;
			}
		case Preference::AREA: 
			switch (other.choice) {
				case Preference::DONTCARE:
				case Preference::AREA: 
					return *this;
				case Preference::SPEED:
					return {};
				case Preference::SPECIFIC_VALUE: 
				case Preference::MIN_VALUE: 
				case Preference::MAX_VALUE: 
					return other;
			}
	}
	return {};
}

}
