/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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

#include "Signal.h"
#include "Bit.h"
#include "UInt.h"
#include "BVec.h"

#include <gatery/hlim/NodePtr.h>
#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Traits.h>

#include <external/magic_enum.hpp>

#include <vector>
#include <optional>

namespace gtry {

	namespace hlim {
		class Node_Signal;
		class SignalGroup;
	}

/**
 * @addtogroup gtry_signals
 * @{
 */


	template<EnumType T>
	class Enum;
	struct construct_from_t;
/*
	template<EnumType T>
	class EnumDefault {
		public:
			EnumDefault(const Enum<T>& rhs);
			EnumDefault(const T& rhs) { m_nodePort = rhs.readPort(); }

			hlim::NodePort getNodePort() const { return m_nodePort; }
		protected:
			hlim::RefCtdNodePort m_nodePort;
	};
*/
	class KnownEnum
	{
	public:
		// gives a list of c++ enum types and ther values used during hw construction.
		// used for gtkwave enum translation.
		static const auto& knownEnums() { return s_knownEnums; }

	protected:
		template<typename T>
		void registerKnownEnum();
	private:
		inline static std::map<std::string, std::map<int, std::string>> s_knownEnums;
	};

	class BaseEnum : public ElementarySignal, public KnownEnum
	{
		public:
			UInt numericalValue() const { return UInt(readPort()); }

			hlim::ConnectionType connType() const override final;
			SignalReadPort readPort() const override final { return rawDriver(); }
			SignalReadPort outPort() const override final;
			std::string_view getName() const override final;
			void setName(std::string name) override;
			void setName(std::string name) const override;
			void addToSignalGroup(hlim::SignalGroup *signalGroup);

			hlim::Node_Signal* node() const { return m_node.get(); }
		protected:
			virtual void createNode();
			void assign(size_t v, std::string_view name);
			virtual void assign(SignalReadPort in, bool ignoreConditions = false) override;

			void exportOverride(const SignalReadPort& exportOverride);
			void simulationOverride(const SignalReadPort& simulationOverride);

			bool valid() const final { return true; } // hide method since Enum is always valid
			SignalReadPort rawDriver() const;

			void connectInput(const SignalReadPort& port);
		private:
			hlim::NodePtr<hlim::Node_Signal> m_node;
	};

	template<EnumType T>
	class Enum : public BaseEnum
	{
	public:
		using isEnumSignal = void;
		using enum_type = T;
		
		Enum();
		Enum(const Enum<T>& rhs);
		Enum(const Enum<T>& rhs, construct_from_t&&);
		Enum(Enum<T>&& rhs);
//		Enum(const EnumDefault<T> &defaultValue);

		Enum(const SignalReadPort& port, std::optional<T> resetValue = std::nullopt);

		Enum(T v);

		explicit Enum(const UInt &rhs) : Enum(rhs.readPort()) { HCL_DESIGNCHECK_HINT(rhs.width() == width(), "Only bit vectors of correct size can be converted to enum signals"); }

		template<UIntValue V>
		explicit Enum(V rhs) : Enum((UInt)rhs) { }

		Enum<T>& operator=(const Enum<T>& rhs);
		Enum<T>& operator=(Enum<T>&& rhs);
//		Enum<T>& operator=(const EnumDefault<T> &defaultValue);

		Enum<T>& operator=(T rhs);

		void exportOverride(const Enum<T>& exportOverride) { BaseEnum::exportOverride(exportOverride.readPort()); }

		void resetValue(T v);
		std::optional<T> resetValue() const { return m_resetValue; }
		Enum<T> final() const;

		BitWidth width() const override final;

		virtual BVec toBVec() const override { return (BVec) numericalValue(); }
		virtual void fromBVec(const BVec &bvec) override { (*this) = Enum<T>((UInt)bvec); }

	protected:
		virtual void createNode() override;
		void assignEnum(T);
	private:
		std::optional<T> m_resetValue;
	};

	// overload to implement reset value override

	namespace internal_enum {
		SignalReadPort reg(SignalReadPort val, std::optional<SignalReadPort> reset, const RegisterSettings& settings);
	}

	template<EnumType T>
	Enum<T> reg(const Enum<T>& val, const RegisterSettings& settings) {
		if(auto rval = val.resetValue())
			return internal_enum::reg(val.readPort(), Enum<T>(*rval).readPort(), settings);

		return internal_enum::reg(val.readPort(), std::nullopt, settings);
	}


	/// Matches all enum signals
	template<typename T>
	concept EnumSignal = requires() { typename T::isEnumSignal; };

	/// @brief Matches any pair of enum types that can be compared. 
	/// @details This requires that at least one of them is a enum signal, the other must either be the same enum signal or the base enum type.
	template<typename TL, typename TR>
	concept ValidEnumValuePair = 
			(EnumSignal<TL> || EnumSignal<TR>) &&  // At least one is a signal
			(std::same_as<ValueToBaseSignal<TL>, ValueToBaseSignal<TR>>); // Both decay to same enum signal

	namespace internal_enum {
		SignalReadPort makeCompareNodeEQ( NormalizedWidthOperands ops);
		SignalReadPort makeCompareNodeNEQ(NormalizedWidthOperands ops);

		template<EnumSignal T>
		inline Bit eq(const T& lhs, const T& rhs) { return makeCompareNodeEQ({lhs, rhs}); }
		template<EnumSignal T>
		inline Bit neq(const T& lhs, const T& rhs) { return makeCompareNodeNEQ({lhs, rhs}); }
	}


	/// Compares for equality two compatible enum values (signal or literal of equal base enum type).
	template<typename TL, typename TR> requires (ValidEnumValuePair<TL, TR>)
	inline Bit eq(const TL& lhs, const TR& rhs) { return internal_enum::eq<ValueToBaseSignal<TL>>(lhs, rhs); }
	/// Compares for inequality two compatible enum values (signal or literal of equal base enum type).
	template<typename TL, typename TR> requires (ValidEnumValuePair<TL, TR>)
	inline Bit neq(const TL& lhs, const TR& rhs) { return internal_enum::neq<ValueToBaseSignal<TL>>(lhs, rhs); }

	/// Compares for equality two compatible enum values (signal or literal of equal base enum type).
	template<typename TL, typename TR> requires (ValidEnumValuePair<TL, TR>)
	inline Bit operator == (const TL& lhs, const TR& rhs) { return eq(lhs, rhs); }
	/// Compares for inequality two compatible enum values (signal or literal of equal base enum type).
	template<typename TL, typename TR> requires (ValidEnumValuePair<TL, TR>)
	inline Bit operator != (const TL& lhs, const TR& rhs) { return neq(lhs, rhs); }

	template<typename T>
	inline void gtry::KnownEnum::registerKnownEnum()
	{
		std::string name = typeid(T).name();
		for (auto it = name.crbegin(); it != name.crend(); ++it)
			if (!std::isalnum(*it) && *it != '_')
			{
				name = name.substr(it.base() - name.cbegin());
				break;
			}

		if (name.empty())
			throw std::runtime_error{ "failed to parse name from enum type" };

		auto& info = s_knownEnums[name];
		if (info.empty())
			for(T val : magic_enum::enum_values<T>())
				info[(int)val] = magic_enum::enum_name(val);

	}

	template<EnumType T>
	Enum<T>::Enum() { 
		createNode(); 
	}

	template<EnumType T>
	Enum<T>::Enum(const Enum<T>& rhs) : Enum(rhs.readPort()) { 
		m_resetValue = rhs.m_resetValue; 
	}

	template<EnumType T>
	Enum<T>::Enum(const Enum<T>& rhs, construct_from_t&&) : Enum()
	{
		m_resetValue = rhs.m_resetValue;
	}

	template<EnumType T>
	Enum<T>::Enum(Enum<T>&& rhs) : Enum() {
		BaseEnum::assign(rhs.readPort());
		rhs.BaseEnum::assign(SignalReadPort{ node() });
		m_resetValue = rhs.m_resetValue;
	}

	template<EnumType T>
	Enum<T>::Enum(const SignalReadPort& port, std::optional<T> resetValue) :
		m_resetValue(resetValue)
	{
		createNode();
		connectInput(port);
	}

	template<EnumType T>
	Enum<T>::Enum(T v) {
		createNode();
		assignEnum(v);
	}

	template<EnumType T>
	Enum<T>& Enum<T>::operator=(const Enum<T>& rhs) {
		m_resetValue = rhs.m_resetValue;
		BaseEnum::assign(rhs.readPort());
		return *this;
	}

	template<EnumType T>
	Enum<T>& Enum<T>::operator=(Enum<T>&& rhs) {
		m_resetValue = rhs.m_resetValue;
		BaseEnum::assign(rhs.readPort());
		rhs.BaseEnum::assign(SignalReadPort{ node() });
		return *this;
	}

	template<EnumType T>
	Enum<T>& Enum<T>::operator=(T rhs) { 
		assignEnum(rhs); 
		return *this; 
	}

	template<EnumType T>
	BitWidth Enum<T>::width() const
	{
		BitWidth maxWidth = 0_b;
		for(auto v : magic_enum::enum_values<T>())
			maxWidth = std::max(maxWidth, BitWidth::last((size_t)v));

		return maxWidth;
	}
	
	template<EnumType T>
	void Enum<T>::resetValue(T v) {
		m_resetValue = v;	
	}

	template<EnumType T>
	Enum<T> Enum<T>::final() const {
		return SignalReadPort{ node() };
	}

	template<EnumType T>
	void Enum<T>::createNode() {
		BaseEnum::createNode();

		registerKnownEnum<T>();
	}

	template<EnumType T>
	void Enum<T>::assignEnum(T v) {
		HCL_DESIGNCHECK_HINT((size_t)v < MAGIC_ENUM_RANGE_MAX, "The values of enums adapted to signals must be within a small range defined by the Magic Enum library!");

		BaseEnum::assign((size_t)v, magic_enum::enum_name(v));
	}


/**@}*/

}

