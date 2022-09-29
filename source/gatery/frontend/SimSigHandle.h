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

#include "Pin.h"
#include "Signal.h"
#include "Bit.h"
#include "UInt.h"

#include "ReadSignalList.h"

#include <gatery/utils/Traits.h>

#include <gatery/simulation/SigHandle.h>
#include <gatery/simulation/simProc/SimulationProcess.h>
#include <gatery/simulation/simProc/WaitFor.h>
#include <gatery/simulation/simProc/WaitUntil.h>
#include <gatery/simulation/simProc/WaitClock.h>
#include <gatery/simulation/simProc/WaitStable.h>


namespace gtry {

class Clock;

/**
 * @addtogroup gtry_simProcs
 * @{
 */


template<gtry::BaseSignal SignalType>
class BaseSigHandle
{
	public:
		operator sim::DefaultBitVectorState () const { return eval(); }
		void operator=(const sim::DefaultBitVectorState &state) { this->m_handle = state; }
		bool operator==(const sim::DefaultBitVectorState &state) const { ReadSignalList::addToAllScopes(m_handle.getOutput()); return this->m_handle == state; }
		bool operator!=(const sim::DefaultBitVectorState &state) const { return !this->operator==(state); }

		void invalidate() { m_handle.invalidate(); }
		bool allDefined() const { return m_handle.allDefined(); }

		sim::DefaultBitVectorState eval() const { ReadSignalList::addToAllScopes(m_handle.getOutput()); return m_handle.eval(); }
	protected:
		BaseSigHandle(const hlim::NodePort &np) : m_handle(np) { HCL_DESIGNCHECK(np.node != nullptr); }

		sim::SigHandle m_handle;
};

template<gtry::BaseSignal SignalType>
class BaseIntSigHandle : public BaseSigHandle<SignalType> 
{
	public:
		using BaseSigHandle<SignalType>::operator=;

		template<std::same_as<sim::BigInt> BigInt_> // prevent conversion
		void operator=(const BigInt_ &v) { this->m_handle = v; }

		operator sim::BigInt () const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return (sim::BigInt) this->m_handle; }
		std::uint64_t defined() const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return this->m_handle.defined(); }
	protected:
		BaseIntSigHandle(const hlim::NodePort &np) : BaseSigHandle<SignalType>(np) { }
};

template<gtry::BaseSignal SignalType>
class BaseUIntSigHandle : public BaseIntSigHandle<SignalType> 
{
	public:
		using BaseIntSigHandle<SignalType>::operator=;

		void operator=(std::uint64_t v) { this->m_handle = v; }
		void operator=(std::string_view v) { this->m_handle = v; }
		bool operator==(std::uint64_t v) const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return this->m_handle == v; }
		bool operator==(std::string_view v) const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return this->m_handle == v; }
		bool operator!=(std::uint64_t v) const { return !this->operator==(v); }
		bool operator!=(std::string_view v) const { return !this->operator==(v); }

		// use templates to provide a perfect fit and prevent ambiguous overloads (cast operands for provided operator== vs use provided cast operator and buildin ==)
		template<std::unsigned_integral T>
		void operator=(T v) { this->operator=((std::uint64_t)v); }
		template<std::unsigned_integral T>
		bool operator==(T v) const { return this->operator==((std::uint64_t)v); }
		template<std::unsigned_integral T>
		bool operator!=(T v) const { return this->operator!=((std::uint64_t)v); }

		std::uint64_t value() const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return this->m_handle.value(); }
		operator std::uint64_t () const { return value(); }

		template<std::same_as<int> T> // prevent char to int conversion
		void operator=(T v) { HCL_DESIGNCHECK_HINT(v >= 0, "UInt and BVec signals can not be negative!"); this->operator=((std::uint64_t)v); }
		template<std::same_as<int> T> // prevent char to int conversion
		bool operator==(T v) const { HCL_DESIGNCHECK_HINT(v >= 0, "UInt and BVec signals can not be negative, comparing with negative numbers is most likely a mistake!"); return this->operator==((std::uint64_t)v); }
		template<std::same_as<int> T> // prevent char to int conversion
		bool operator!=(T v) const { HCL_DESIGNCHECK_HINT(v >= 0, "UInt and BVec signals can not be negative, comparing with negative numbers is most likely a mistake!"); return this->operator!=((std::uint64_t)v); }
	protected:
		BaseUIntSigHandle(const hlim::NodePort &np) : BaseIntSigHandle<SignalType>(np) { }
};


class SigHandleBVec : public BaseUIntSigHandle<BVec>
{
	public:
		using BaseUIntSigHandle<BVec>::operator=;
		using BaseUIntSigHandle<BVec>::operator==;

		void operator=(const SigHandleBVec &rhs) { this->operator=(rhs.eval()); }
		bool operator==(const SigHandleBVec &rhs) const { return BaseSigHandle<BVec>::operator==(rhs.eval()); }
		bool operator!=(const SigHandleBVec &rhs) const { return !BaseSigHandle<BVec>::operator==(rhs.eval()); }

		SigHandleBVec drivingReg() const { SigHandleBVec res(m_handle.getOutput()); res.m_handle.overrideDrivingRegister(); return res; }
	protected:
		SigHandleBVec(const hlim::NodePort &np) : BaseUIntSigHandle<BVec>(np) { }

		friend SigHandleBVec simu(const BVec &signal);

		friend SigHandleBVec simu(const InputPins &pin);
		friend SigHandleBVec simu(const OutputPins &pin);		
};

class SigHandleUInt : public BaseUIntSigHandle<UInt>
{
	public:
		using BaseUIntSigHandle<UInt>::operator=;
		using BaseUIntSigHandle<UInt>::operator==;

		void operator=(const SigHandleUInt &rhs) { this->operator=(rhs.eval()); }
		bool operator==(const SigHandleUInt &rhs) const { return BaseSigHandle<UInt>::operator==(rhs.eval()); }
		bool operator!=(const SigHandleUInt &rhs) const { return !BaseSigHandle<UInt>::operator==(rhs.eval()); }

		SigHandleUInt drivingReg() const { SigHandleUInt res(m_handle.getOutput()); res.m_handle.overrideDrivingRegister(); return res; }
	protected:
		SigHandleUInt(const hlim::NodePort &np) : BaseUIntSigHandle<UInt>(np) { }

		friend SigHandleUInt simu(const UInt &signal);
};

class SigHandleSInt : public BaseIntSigHandle<SInt>
{
	public:
		using BaseIntSigHandle<SInt>::operator=;
		using BaseIntSigHandle<SInt>::operator==;

		void operator=(const SigHandleSInt &rhs) { this->operator=(rhs.eval()); }
		bool operator==(const SigHandleSInt &rhs) const { return BaseSigHandle<SInt>::operator==(rhs.eval()); }
		bool operator!=(const SigHandleSInt &rhs) const { return !BaseSigHandle<SInt>::operator==(rhs.eval()); }

		void operator=(std::int64_t v) { this->m_handle = v; }
		bool operator==(std::int64_t v) const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return this->m_handle == v; }
		bool operator!=(std::int64_t v) const { return !this->operator==(v); }

		// use templates to provide a perfect fit and prevent ambiguous overloads (cast operands for provided operator== vs use provided cast operator and buildin ==)
		template<std::signed_integral T>
		void operator=(T v) { this->operator=((std::uint64_t)v); }
		template<std::signed_integral T>
		bool operator==(T v) const { return this->operator==((std::uint64_t)v); }
		template<std::signed_integral T>
		bool operator!=(T v) const { return this->operator!=((std::uint64_t)v); }


		std::int64_t value() const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return (std::int64_t) this->m_handle; }
		operator std::int64_t () const { return value(); }

		SigHandleSInt drivingReg() const { SigHandleSInt res(m_handle.getOutput()); res.m_handle.overrideDrivingRegister(); return res; }
	protected:
		SigHandleSInt(const hlim::NodePort &np) : BaseIntSigHandle<SInt>(np) { }

		friend SigHandleSInt simu(const SInt &signal);
};


class SigHandleBit : public BaseSigHandle<Bit>
{
	public:
		using BaseSigHandle<Bit>::operator=;
		using BaseSigHandle<Bit>::operator==;

		void operator=(const SigHandleBit &rhs) { this->operator=(rhs.eval()); }
		bool operator==(const SigHandleBit &rhs) const { return BaseSigHandle<Bit>::operator==(rhs.eval()); }
		bool operator!=(const SigHandleBit &rhs) const { return !BaseSigHandle<Bit>::operator==(rhs.eval()); }

		void operator=(bool b) { this->m_handle = (b?'1':'0'); }
		void operator=(char c) { this->m_handle = c; }
		bool operator==(bool b) const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return this->m_handle == (b?'1':'0'); }
		bool operator==(char c) const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return this->m_handle == c; }

		bool operator!=(bool b) const { return !this->operator==(b); }
		bool operator!=(char c) const { return !this->operator==(c); }

		bool defined() const { return allDefined(); }
		bool value() const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return (bool) this->m_handle; }
		operator bool () const { return value(); }

		SigHandleBit drivingReg() const { SigHandleBit res(m_handle.getOutput()); res.m_handle.overrideDrivingRegister(); return res; }
	protected:
		SigHandleBit(const hlim::NodePort &np) : BaseSigHandle<Bit>(np) { }

		friend SigHandleBit simu(const Bit &signal);

		friend SigHandleBit simu(const InputPin &pin);
		friend SigHandleBit simu(const OutputPin &pin);
};


template<EnumSignal SignalType>
class SigHandleEnum : public BaseSigHandle<SignalType>
{
	public:
		using BaseSigHandle<SignalType>::operator=;
		using BaseSigHandle<SignalType>::operator==;

		void operator=(const SigHandleEnum<SignalType> &rhs) { this->operator=(rhs.eval()); }
		bool operator==(const SigHandleEnum<SignalType> &rhs) const { return BaseSigHandle<SignalType>::operator==(rhs.eval()); }
		bool operator!=(const SigHandleEnum<SignalType> &rhs) const { return !BaseSigHandle<SignalType>::operator==(rhs.eval()); }

		void operator=(typename SignalType::enum_type v) { this->m_handle = v; }
		bool operator==(typename SignalType::enum_type v) const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return this->m_handle == v; }
		bool operator!=(typename SignalType::enum_type v) const { return !this->operator==(v); }

		bool defined() const { return this->allDefined(); }
		typename SignalType::enum_type value() const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return (typename SignalType::enum_type) this->m_handle; }

		SigHandleEnum<SignalType> drivingReg() const { SigHandleEnum<SignalType> res(this->m_handle.getOutput()); res.m_handle.overrideDrivingRegister(); return res; }
	protected:
		SigHandleEnum(const hlim::NodePort &np) : BaseSigHandle<SignalType>(np) { }

		friend SigHandleEnum<SignalType> simu<>(const SignalType &signal);
};


inline SigHandleBVec simu(const BVec &signal) { return SigHandleBVec(signal.readPort()); }
inline SigHandleUInt simu(const UInt &signal) { return SigHandleUInt(signal.readPort()); }
inline SigHandleSInt simu(const SInt &signal) { return SigHandleSInt(signal.readPort()); }
inline SigHandleBit simu(const Bit &signal) { return SigHandleBit(signal.readPort()); }
template<EnumSignal SignalType>
SigHandleEnum<SignalType> simu(const SignalType &signal) {
	return SigHandleEnum<SignalType>(signal.readPort());
}


inline std::ostream &operator<<(std::ostream &stream, const SigHandleBVec &handle) { return stream << (sim::DefaultBitVectorState) handle; }
inline std::ostream &operator<<(std::ostream &stream, const SigHandleUInt &handle) { return stream << (sim::DefaultBitVectorState) handle; }
inline std::ostream &operator<<(std::ostream &stream, const SigHandleSInt &handle) { return stream << (sim::DefaultBitVectorState) handle; }
inline std::ostream &operator<<(std::ostream &stream, const SigHandleBit &handle) { return stream << (sim::DefaultBitVectorState) handle; }
template<EnumSignal SignalType>
inline std::ostream &operator<<(std::ostream &stream, const SigHandleEnum<SignalType> &handle) { return stream << (sim::DefaultBitVectorState) handle; }


SigHandleBit simu(const InputPin &pin);
SigHandleBVec simu(const InputPins &pins);

SigHandleBit simu(const OutputPin &pin);
SigHandleBVec simu(const OutputPins &pins);




using SimProcess = sim::SimulationProcess;
using WaitFor = sim::WaitFor;
using WaitUntil = sim::WaitUntil;
using WaitStable = sim::WaitStable;
using Seconds = hlim::ClockRational;

using BigInt = sim::BigInt;

sim::WaitClock AfterClk(const Clock &clk);
sim::WaitClock OnClk(const Clock &clk);

void simAnnotationStart(const std::string &id, const std::string &desc);
void simAnnotationStartDelayed(const std::string &id, const std::string &desc, const Clock &clk, int cycles);

void simAnnotationEnd(const std::string &id);
void simAnnotationEndDelayed(const std::string &id, const Clock &clk, int cycles);

/**@}*/

}
