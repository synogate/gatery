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

#include "Scope.h"
#include "Reg.h"
#include "../hlim/Attributes.h"
#include "Bit.h"
#include "BVec.h"
#include "UInt.h"
#include "SInt.h"


#include <gatery/hlim/Clock.h>

#include <optional>

#include <boost/spirit/home/support/container.hpp>
#include <boost/hana/adapt_struct.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/fwd/accessors.hpp>

namespace gtry {


	namespace hlim {
		class Node_Register;
	}

/**
 * @addtogroup gtry_frontend
 * @{
 */

	class Clock;

	hlim::ClockRational clockFromString(std::string text);

	class ClockConfig
	{
	public:
		using ClockRational = hlim::ClockRational;
		using TriggerEvent = hlim::Clock::TriggerEvent;
		using ResetType = hlim::RegisterAttributes::ResetType;
		using UsageType = hlim::RegisterAttributes::UsageType;
		using ResetActive = hlim::RegisterAttributes::Active;

		void loadConfig(const utils::ConfigTree& config);
		void print(std::ostream& s) const;

		std::optional<ClockRational> absoluteFrequency;
		std::optional<ClockRational> frequencyMultiplier;
		std::optional<std::string> name;
		std::optional<std::string> resetName;
		std::optional<TriggerEvent> triggerEvent;
		std::optional<bool> phaseSynchronousWithParent;

		std::optional<ResetType> resetType;
		std::optional<ResetType> memoryResetType;
		std::optional<bool> initializeRegs;
		std::optional<bool> initializeMemory;
		std::optional<ResetActive> resetActive;

		std::optional<bool> synchronizationRegister;
		std::optional<UsageType> registerResetPinUsage;
		std::optional<UsageType> registerEnablePinUsage;

		hlim::Attributes attributes;

		ClockConfig &addRegisterAttribute(const std::string &vendor, const std::string &attrib, const std::string &type, const std::string &value);
	};

	std::ostream& operator << (std::ostream&, const ClockConfig&);

	/**
	 * @todo write docs
	 */
	class Clock
	{
		public:
			using ClockRational = hlim::ClockRational;
			using TriggerEvent = hlim::Clock::TriggerEvent;
			using ResetType = hlim::RegisterAttributes::ResetType;
			using ResetActive = hlim::RegisterAttributes::Active;

			//Clock(size_t freq);
			Clock(const ClockConfig &config);
			Clock deriveClock(const ClockConfig &config) const;

			Clock(const Clock &other);
			Clock &operator=(const Clock &other);

			Clock(hlim::Clock *clock) : m_clock(clock) { }

			Bit clkSignal() const;
			Bit rstSignal() const;
			BVec operator() (const BVec& signal, const RegisterSettings &settings = {}) const;
			BVec operator() (const BVec& signal, const BVec& reset, const RegisterSettings &settings = {}) const;
			UInt operator() (const UInt& signal, const RegisterSettings &settings = {}) const;
			UInt operator() (const UInt& signal, const UInt& reset, const RegisterSettings &settings = {}) const;
			SInt operator() (const SInt& signal, const RegisterSettings &settings = {}) const;
			SInt operator() (const SInt& signal, const SInt& reset, const RegisterSettings &settings = {}) const;
			Bit operator() (const Bit& signal, const RegisterSettings &settings = {}) const;
			Bit operator() (const Bit& signal, const Bit& reset, const RegisterSettings &settings = {}) const;

			hlim::Clock *getClk() const { return m_clock; }
			hlim::ClockRational absoluteFrequency() const { return m_clock->absoluteFrequency(); }

			void overrideClkWith(const Bit &clkOverride);
			void overrideRstWith(const Bit &rstOverride);

			Bit reset(ResetActive active) const;
			void reset(const Bit& signal);
			void reset(const Bit& signal, const Clock& sourceClock);

			std::string_view name() const { return m_clock->getName(); }
			void setName(std::string name) { m_clock->setName(std::move(name)); }
		protected:
			hlim::Clock *m_clock = nullptr;
			Clock(hlim::Clock *clock, const ClockConfig &config);

			void applyConfig(const ClockConfig &config);


			template<BitVectorDerived T>
			T buildReg(const T& signal, const RegisterSettings &settings = {}) const;
			template<BitVectorDerived T>
			T buildReg(const T& signal, const T& reset, const RegisterSettings &settings = {}) const;

			hlim::Node_Register *prepRegister(std::string_view name, const RegisterSettings& settings) const;

	};

	inline void setName(Clock& clock, std::string_view name) { clock.setName(std::string(name)); }

	class ClockScope : public BaseScope<ClockScope>
	{
		public:
			ClockScope(const Clock clock) : m_clock(clock) { }
			static const Clock &getClk() {
				HCL_DESIGNCHECK_HINT(m_currentScope != nullptr, "No clock scope active!");
				return m_currentScope->m_clock;
			}
			static bool anyActive() { return m_currentScope != nullptr; }
		protected:
			const Clock m_clock;
	};

	/// current clock scope reset signal. normalized to always active high reset.
	Bit reset();

/// @}

}
