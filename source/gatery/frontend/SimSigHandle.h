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
#include "SInt.h"
#include "BVec.h"
#include "Enum.h"

#include "ReadSignalList.h"

#include <gatery/utils/Traits.h>

#include <gatery/hlim/ClockRational.h>

#include <gatery/simulation/SigHandle.h>
#include <gatery/simulation/simProc/Condition.h>
#include <gatery/simulation/simProc/SimulationProcess.h>
#include <gatery/simulation/simProc/WaitFor.h>
#include <gatery/simulation/simProc/WaitUntil.h>
#include <gatery/simulation/simProc/WaitClock.h>
#include <gatery/simulation/simProc/WaitStable.h>

#include <any>

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
		operator sim::DefaultBitVectorState() const { return eval(); }
		void operator=(const sim::DefaultBitVectorState& state) { this->m_handle = state; }
		void operator=(const sim::ExtendedBitVectorState& state) { this->m_handle = state; }
		bool operator==(const sim::DefaultBitVectorState& state) const { ReadSignalList::addToAllScopes(m_handle.getOutput()); return this->m_handle == state; }
		bool operator!=(const sim::DefaultBitVectorState& state) const { return !this->operator==(state); }

		/// Interprete the given array of integers as one concatenated bit string to assign to this signal
		template<std::unsigned_integral T>
		void operator=(std::span<T> array) { this->m_handle = array; }
		/// Interprete the given array of integers as one concatenated bit string to assign to this signal
		template<std::unsigned_integral T>
		void operator=(const std::vector<T> &array) { this->m_handle = std::span(array); }

		/// Interprete the given array of integers as one concatenated bit string and return true if all bits in the signal are defined and equal to the given bit string.
		template<std::unsigned_integral T>
		bool operator==(std::span<T> array) const { return this->m_handle == array; }
		/// Interprete the given array of integers as one concatenated bit string and return true if any bits in the signal are undefined or unequal to the given bit string.
		template<std::unsigned_integral T>
		bool operator!=(std::span<T> array) const { return !this->operator==(array); }
		/// Interprete the given array of integers as one concatenated bit string and return true if all bits in the signal are defined and equal to the given bit string.
		template<std::unsigned_integral T, size_t N>
		bool operator==(std::span<T, N> array) const { return this->m_handle == array; }
		/// Interprete the given array of integers as one concatenated bit string and return true if any bits in the signal are undefined or unequal to the given bit string.
		template<std::unsigned_integral T, size_t N>
		bool operator!=(std::span<T, N> array) const { return !this->operator==(array); }
		/// Interprete the given array of integers as one concatenated bit string and return true if all bits in the signal are defined and equal to the given bit string.
		template<std::unsigned_integral T>
		bool operator==(const std::vector<T> &array) const { return this->m_handle == std::span(array); }
		/// Interprete the given array of integers as one concatenated bit string and return true if any bits in the signal are undefined or unequal to the given bit string.
		template<std::unsigned_integral T>
		bool operator!=(const std::vector<T> &array) const { return !this->operator==(array); }

		/// Split the signal's state into integers and return their values.
		/// @details The value of undefined bits in the returned bit string is arbitrary.
		template<std::unsigned_integral T>
		std::vector<T> toVector() const { return this->m_handle.template toVector<T>(); }

		/// Split the signal's state into integers and return their values.
		/// @details The value of undefined bits in the returned bit string is arbitrary.
		template<std::unsigned_integral T>
		operator std::vector<T> () const { return this->m_handle.template toVector<T>(); }

		void invalidate() { m_handle.invalidate(); }
		void stopDriving() { m_handle.stopDriving(); }
		bool allDefined() const { return m_handle.allDefined(); }

		sim::DefaultBitVectorState eval() const { ReadSignalList::addToAllScopes(m_handle.getOutput()); return m_handle.eval(); }

		inline sim::SigHandle &getBackendHandle() { return m_handle; }
	protected:
		BaseSigHandle(const hlim::NodePort& np) : m_handle(np) { HCL_DESIGNCHECK(np.node != nullptr); }

		sim::SigHandle m_handle;
	};

	template<gtry::BaseSignal SignalType>
	class BaseIntSigHandle : public BaseSigHandle<SignalType>
	{
	public:
		using BaseSigHandle<SignalType>::operator=;
		using BaseSigHandle<SignalType>::operator==;

		template<std::same_as<sim::BigInt> BigInt_> // prevent conversion
		void operator=(const BigInt_& v) { this->m_handle = v; }

		operator sim::BigInt() const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return (sim::BigInt)this->m_handle; }
		std::uint64_t defined() const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return this->m_handle.defined(); }
	protected:
		BaseIntSigHandle(const hlim::NodePort& np) : BaseSigHandle<SignalType>(np) { }
	};

	template<gtry::BaseSignal SignalType>
	class BaseUIntSigHandle : public BaseIntSigHandle<SignalType>
	{
	public:
		using BaseIntSigHandle<SignalType>::operator=;
		using BaseIntSigHandle<SignalType>::operator==;

		// plain copy of base class operator to prevent clang intellisense hang bug
		template<std::same_as<sim::BigInt> BigInt_> // prevent conversion
		void operator=(const BigInt_& v) { this->m_handle = v; }

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


		/// Interprete the given array of integers as one concatenated bit string and return true if all bits in the signal are defined and equal to the given bit string.
		template<std::unsigned_integral T>
		bool operator==(std::span<T> array) const { return this->m_handle == array; }
		/// Interprete the given array of integers as one concatenated bit string and return true if any bits in the signal are undefined or unequal to the given bit string.
		template<std::unsigned_integral T>
		bool operator!=(std::span<T> array) const { return !this->operator==(array); }
		/// Interprete the given array of integers as one concatenated bit string and return true if all bits in the signal are defined and equal to the given bit string.
		template<std::unsigned_integral T>
		bool operator==(const std::vector<T> &array) const { return this->m_handle == std::span(array); }
		/// Interprete the given array of integers as one concatenated bit string and return true if any bits in the signal are undefined or unequal to the given bit string.
		template<std::unsigned_integral T>
		bool operator!=(const std::vector<T> &array) const { return !this->operator==(array); }		

		std::uint64_t value() const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return this->m_handle.value(); }
		operator std::uint64_t() const { return value(); }

		template<std::same_as<int> T> // prevent char to int conversion
		void operator=(T v) { HCL_DESIGNCHECK_HINT(v >= 0, "UInt and BVec signals can not be negative!"); this->operator=((std::uint64_t)v); }
		template<std::same_as<int> T> // prevent char to int conversion
		bool operator==(T v) const { HCL_DESIGNCHECK_HINT(v >= 0, "UInt and BVec signals can not be negative, comparing with negative numbers is most likely a mistake!"); return this->operator==((std::uint64_t)v); }
		template<std::same_as<int> T> // prevent char to int conversion
		bool operator!=(T v) const { HCL_DESIGNCHECK_HINT(v >= 0, "UInt and BVec signals can not be negative, comparing with negative numbers is most likely a mistake!"); return this->operator!=((std::uint64_t)v); }
	protected:
		BaseUIntSigHandle(const hlim::NodePort& np) : BaseIntSigHandle<SignalType>(np) { }
	};


	class SigHandleBVec : public BaseUIntSigHandle<BVec>
	{
	public:
		using BaseUIntSigHandle<BVec>::operator=;
		using BaseUIntSigHandle<BVec>::operator==;

		void operator=(const SigHandleBVec& rhs) { this->operator=(rhs.eval()); }
		bool operator==(const SigHandleBVec& rhs) const { return BaseSigHandle<BVec>::operator==(rhs.eval()); }
		bool operator!=(const SigHandleBVec& rhs) const { return !BaseSigHandle<BVec>::operator==(rhs.eval()); }

		SigHandleBVec drivingReg() const { SigHandleBVec res(m_handle.getOutput()); res.m_handle.overrideDrivingRegister(); return res; }
	protected:
		SigHandleBVec(const hlim::NodePort& np) : BaseUIntSigHandle<BVec>(np) { }

		friend SigHandleBVec simu(const BVec& signal);

		friend SigHandleBVec simu(const InputPins& pin);
		friend SigHandleBVec simu(const OutputPins& pin);
	};

	class SigHandleUInt : public BaseUIntSigHandle<UInt>
	{
	public:
		using BaseUIntSigHandle<UInt>::operator=;
		using BaseUIntSigHandle<UInt>::operator==;

		void operator=(const SigHandleUInt& rhs) { this->operator=(rhs.eval()); }
		bool operator==(const SigHandleUInt& rhs) const { return BaseSigHandle<UInt>::operator==(rhs.eval()); }
		bool operator!=(const SigHandleUInt& rhs) const { return !BaseSigHandle<UInt>::operator==(rhs.eval()); }

		SigHandleUInt drivingReg() const { SigHandleUInt res(m_handle.getOutput()); res.m_handle.overrideDrivingRegister(); return res; }
	protected:
		SigHandleUInt(const hlim::NodePort& np) : BaseUIntSigHandle<UInt>(np) { }

		friend SigHandleUInt simu(const UInt& signal);
	};

	class SigHandleSInt : public BaseIntSigHandle<SInt>
	{
	public:
		using BaseIntSigHandle<SInt>::operator=;
		using BaseIntSigHandle<SInt>::operator==;

		void operator=(const SigHandleSInt& rhs) { this->operator=(rhs.eval()); }
		bool operator==(const SigHandleSInt& rhs) const { return BaseSigHandle<SInt>::operator==(rhs.eval()); }
		bool operator!=(const SigHandleSInt& rhs) const { return !BaseSigHandle<SInt>::operator==(rhs.eval()); }

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


		std::int64_t value() const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return (std::int64_t)this->m_handle; }
		operator std::int64_t() const { return value(); }

		SigHandleSInt drivingReg() const { SigHandleSInt res(m_handle.getOutput()); res.m_handle.overrideDrivingRegister(); return res; }
	protected:
		SigHandleSInt(const hlim::NodePort& np) : BaseIntSigHandle<SInt>(np) { }

		friend SigHandleSInt simu(const SInt& signal);
	};


	class SigHandleBit : public BaseSigHandle<Bit>
	{
	public:
		using BaseSigHandle<Bit>::operator=;
		using BaseSigHandle<Bit>::operator==;

		void operator=(const SigHandleBit& rhs) { this->operator=(rhs.eval()); }
		bool operator==(const SigHandleBit& rhs) const { return BaseSigHandle<Bit>::operator==(rhs.eval()); }
		bool operator!=(const SigHandleBit& rhs) const { return !BaseSigHandle<Bit>::operator==(rhs.eval()); }

		void operator=(bool b) { this->m_handle = (b ? '1' : '0'); }
		void operator=(char c) { this->m_handle = c; }
		bool operator==(bool b) const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return this->m_handle == (b ? '1' : '0'); }
		bool operator==(char c) const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return this->m_handle == c; }

		bool operator!=(bool b) const { return !this->operator==(b); }
		bool operator!=(char c) const { return !this->operator==(c); }

		bool defined() const { return allDefined(); }
		bool value() const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return (bool)this->m_handle; }
		explicit operator bool() const { return value(); }
		operator char() const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return this->m_handle; }

		SigHandleBit drivingReg() const { SigHandleBit res(m_handle.getOutput()); res.m_handle.overrideDrivingRegister(); return res; }
	protected:
		SigHandleBit(const hlim::NodePort& np) : BaseSigHandle<Bit>(np) { }

		friend SigHandleBit simu(const Bit& signal);

		friend SigHandleBit simu(const InputPin& pin);
		friend SigHandleBit simu(const OutputPin& pin);
	};

	template<EnumSignal SignalType>
	class SigHandleEnum;

	template<EnumSignal SignalType>
	SigHandleEnum<SignalType> simu(const SignalType &signal) {
		return SigHandleEnum<SignalType>(signal.readPort());
	}

	template<EnumSignal SignalType>
	class SigHandleEnum : public BaseSigHandle<SignalType>
	{
	public:
		using BaseSigHandle<SignalType>::operator=;
		using BaseSigHandle<SignalType>::operator==;

		void operator=(const SigHandleEnum<SignalType>& rhs) { this->operator=(rhs.eval()); }
		bool operator==(const SigHandleEnum<SignalType>& rhs) const { return BaseSigHandle<SignalType>::operator==(rhs.eval()); }
		bool operator!=(const SigHandleEnum<SignalType>& rhs) const { return !BaseSigHandle<SignalType>::operator==(rhs.eval()); }

		void operator=(typename SignalType::enum_type v) { this->m_handle = v; }
		bool operator==(typename SignalType::enum_type v) const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return this->m_handle == v; }
		bool operator!=(typename SignalType::enum_type v) const { return !this->operator==(v); }

		bool defined() const { return this->allDefined(); }
		typename SignalType::enum_type value() const { ReadSignalList::addToAllScopes(this->m_handle.getOutput()); return (typename SignalType::enum_type) this->m_handle; }

		SigHandleEnum<SignalType> drivingReg() const { SigHandleEnum<SignalType> res(this->m_handle.getOutput()); res.m_handle.overrideDrivingRegister(); return res; }
	protected:
		SigHandleEnum(const hlim::NodePort& np) : BaseSigHandle<SignalType>(np) { }

		friend SigHandleEnum<SignalType> simu<SignalType>(const SignalType &signal);
	};


	inline SigHandleBVec simu(const BVec &signal) { return SigHandleBVec(signal.readPort()); }
	inline SigHandleUInt simu(const UInt &signal) { return SigHandleUInt(signal.readPort()); }
	inline SigHandleSInt simu(const SInt &signal) { return SigHandleSInt(signal.readPort()); }
	inline SigHandleBit simu(const Bit &signal) { return SigHandleBit(signal.readPort()); }



	inline std::ostream& operator<<(std::ostream& stream, const SigHandleBVec& handle) { return stream << (sim::DefaultBitVectorState)handle; }
	inline std::ostream& operator<<(std::ostream& stream, const SigHandleUInt& handle) { return stream << (sim::DefaultBitVectorState)handle; }
	inline std::ostream& operator<<(std::ostream& stream, const SigHandleSInt& handle) { return stream << (sim::DefaultBitVectorState)handle; }
	inline std::ostream& operator<<(std::ostream& stream, const SigHandleBit& handle) { return stream << (sim::DefaultBitVectorState)handle; }
	template<EnumSignal SignalType>
	inline std::ostream& operator<<(std::ostream& stream, const SigHandleEnum<SignalType>& handle) { return stream << (sim::DefaultBitVectorState)handle; }


	SigHandleBit simu(const InputPin& pin);
	SigHandleBVec simu(const InputPins& pins);

	SigHandleBit simu(const OutputPin& pin);
	SigHandleBVec simu(const OutputPins& pins);



	using Condition = sim::Condition;

	template<typename T>
	using SimFunction = sim::SimulationFunction<T>;

	using SimProcess = sim::SimulationFunction<void>;
	using WaitFor = sim::WaitFor;
	using WaitUntil = sim::WaitUntil;
	using WaitStable = sim::WaitStable;
	using Seconds = hlim::ClockRational;
	constexpr auto toNanoseconds = hlim::toNanoseconds;

	Seconds getCurrentSimulationTime();
	inline double nowNs() { return toNanoseconds(getCurrentSimulationTime()); }

	/// @brief Returns true in the time period where the simulator is pulling down all the simulation coroutines on reseting or closing the simulation.
	/// @details This allows coroutine code to differentiate between destructing because of going out of scope normally and destructing because the
	///			 entire coroutine stack is being destructed.
	bool simulationIsShuttingDown();

	using BigInt = sim::BigInt;

	sim::WaitClock AfterClk(const Clock& clk);
	sim::WaitClock OnClk(const Clock& clk);

	/**
	 * @brief Forks a new simulation process that will run in (quasi-)parallel.
	 * @details Execution resumes in the new simulation process and returns to the calling simulation process
	 * once the forked one suspends or finishes. Forked simulation processes can be used in a fire&forget manner or
	 * can be joined again, in which case they can return a return value.
	 * @param simProc An instance of gtry::SimFunction.
	 * @return A handle that other simulation processes (not necessarily the one that called fork()) can @ref gtry::join on to await the completion forked process.
	 * @see gtry::sim::ork
	 */
	template<typename ReturnValue>
	auto fork(const sim::SimulationFunction<ReturnValue>& simProc) {
		return sim::forkFunc(simProc);
	}

	/**
	 * @brief Forks a new simulation process from a lambda expression that will run in (quasi-)parallel.
	 * @details Execution resumes in the new simulation process and returns to the calling simulation process
	 * once the forked one suspends or finishes. Forked simulation processes can be used in a fire&forget manner or
	 * can be joined again, in which case they can return a return value.
	 * For lambdas with captures (and functors in general), the simulator must store the lambda instance to prevent it from being destructed before the simulation process terminates.
	 * This overload of fork thus takes the uninvoked functor, stores a copy internally, and invokes the copy.
	 * @param simProcLambda A lambda expression that gets moved (and stored) until execution is done.
	 * @return A handle that other simulation processes (not necessarily the one that called fork()) can @ref gtry::join on to await the completion forked process.
	 * @see gtry::sim::fork
	 */
	template<std::invocable Functor>
	auto fork(Functor simProcLambda) {
		using SimFunc = std::invoke_result_t<Functor>;
		return sim::forkFunc(std::function<SimFunc()>(simProcLambda));
	}

	/**
	 * @brief Suspends execution until another simulation function has finished, potentially returning a return value from the other simulation function.
	 * @param handle A handle to another simulation function as returned from @ref gtry::fork.
	 * @return The return value of the awaited simulation function if it isn't void.
	 * @see gtry::sim::SimulationFunction::Join
	 */
	template<typename Handle>
	auto join(const Handle& handle) {
		return typename SimFunction<typename Handle::promiseType::returnType>::Join(handle);
	}

	void simAnnotationStart(const std::string& id, const std::string& desc);
	void simAnnotationStartDelayed(const std::string& id, const std::string& desc, const Clock& clk, int cycles);

	void simAnnotationEnd(const std::string& id);
	void simAnnotationEndDelayed(const std::string& id, const Clock& clk, int cycles);


	bool simHasData(std::string_view key);
	std::any& registerSimData(std::string_view key, std::any data);
	std::any& getSimData(std::string_view key);

	template<typename T>
	T& getSimData(std::string_view key) { return std::any_cast<T&>(getSimData(key)); }

	template<typename T, typename... Args>
	T& emplaceSimData(std::string_view key, Args&&... args) { return std::any_cast<T&>(registerSimData(key, std::make_any<T>(std::forward<Args>(args)...))); }

	/**@}*/
}

namespace std {
	/// Serializes an std::span or std::vector into an std::ostream.
	/// @details This is required to use in an BOOST_TEST (or similar) expression. Boost test only looks for these in the
	/// namespace of the right hand argument, so we need to place this into std::
	template<typename Container> requires (std::same_as<Container, std::span<typename Container::value_type>> || std::same_as<Container, std::vector<typename Container::value_type>>)
	inline std::ostream &operator<<(std::ostream &stream, const Container &array) { 
		for (auto i : gtry::utils::Range(array.size())) {
			if (i > 0) stream << ' ';
			stream << array[i];
		}
		return stream;
	}
}
