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
#include "Scope.h"

#include "Reg.h"
#include "ConditionalScope.h"

#include <gatery/hlim/NodePtr.h>
#include <gatery/hlim/coreNodes/Node_Signal.h>
#include <gatery/hlim/coreNodes/Node_Multiplexer.h>
#include <gatery/hlim/coreNodes/Node_Compare.h>
#include <gatery/hlim/supportNodes/Node_ExportOverride.h>
#include <gatery/hlim/Attributes.h>
#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Traits.h>

#include <vector>
#include <optional>

namespace gtry {
    
	template<EnumType T>
    class Enum;
/*
	template<EnumType T>
    class EnumDefault {
        public:
            EnumDefault(const Enum<T>& rhs);
			EnumDefault(const T& rhs) { m_nodePort = rhs.getReadPort(); }

            hlim::NodePort getNodePort() const { return m_nodePort; }
        protected:
            hlim::RefCtdNodePort m_nodePort;
    };
*/
    template<EnumType T>
    class Enum : public ElementarySignal
    {
    public:
        using isEnumSignal = void;
        using enum_type = T;
        
        Enum();
        Enum(const Enum<T>& rhs);
        Enum(Enum<T>&& rhs);
//        Enum(const EnumDefault<T> &defaultValue);

        Enum(const SignalReadPort& port);

        Enum(T v);

		template<BitVectorDerived V>
		explicit Enum(V rhs) : Enum(rhs.getReadPort()) {  }

		template<BitVectorDerived V>
		explicit operator V() const { if (m_node) return V(getReadPort()); else return V{}; }


        Enum<T>& operator=(const Enum<T>& rhs);
        Enum<T>& operator=(Enum<T>&& rhs);
//        Enum<T>& operator=(const EnumDefault<T> &defaultValue);

        Enum<T>& operator=(T rhs);

        void setExportOverride(const Enum<T>& exportOverride);

        constexpr BitWidth getWidth() const final;
        hlim::ConnectionType getConnType() const final;
        SignalReadPort getReadPort() const final;
        SignalReadPort getOutPort() const final;
        std::string_view getName() const final;
        void setName(std::string name) override;
        void addToSignalGroup(hlim::SignalGroup *signalGroup);

        void setResetValue(T v);
        std::optional<T> getResetValue() const { return m_resetValue; }

        hlim::Node_Signal* getNode() { return m_node.get(); }

        Enum<T> final() const;

    protected:
        void createNode();
        void assign(T);
        virtual void assign(SignalReadPort in, bool ignoreConditions = false);

        bool valid() const final; // hide method since Bit is always valid
        SignalReadPort getRawDriver() const;

    private:
        hlim::NodePtr<hlim::Node_Signal> m_node;
        size_t m_offset = 0;
        std::optional<T> m_resetValue;
    };

    // overload to implement reset value override

	template<EnumType T>
    Enum<T> reg(const Enum<T>& val, const RegisterSettings& settings) {
        if(auto rval = val.getResetValue())
            return reg<Enum<T>>(val, *rval, settings);

        return reg<Enum<T>>(val, settings);
    }

	template<EnumType T>
    Enum<T> reg(const Enum<T>& val) {
        return reg(val, RegisterSettings{});
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
	    SignalReadPort makeNode(hlim::Node_Compare::Op op, NormalizedWidthOperands ops);

		template<EnumSignal T>
		inline Bit eq(const T& lhs, const T& rhs) { return makeNode(hlim::Node_Compare::EQ, {lhs, rhs}); }
		template<EnumSignal T>
		inline Bit neq(const T& lhs, const T& rhs) { return makeNode(hlim::Node_Compare::NEQ, {lhs, rhs}); }
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







	template<EnumType T>
	Enum<T>::Enum() { 
		createNode(); 
	}

	template<EnumType T>
    Enum<T>::Enum(const Enum<T>& rhs) : Enum(rhs.getReadPort()) { 
		m_resetValue = rhs.m_resetValue; 
	}

	template<EnumType T>
	Enum<T>::Enum(Enum<T>&& rhs) : Enum() {
		assign(rhs.getReadPort());
		rhs.assign(SignalReadPort{ m_node });
		m_resetValue = rhs.m_resetValue;
	}

	template<EnumType T>
	Enum<T>::Enum(const SignalReadPort& port) {
        createNode();
        m_node->connectInput(port);
    }

	template<EnumType T>
	Enum<T>::Enum(T v) {
		createNode();
		assign(v);
	}

	template<EnumType T>
	Enum<T>& Enum<T>::operator=(const Enum<T>& rhs) {
		m_resetValue = rhs.m_resetValue;
		assign(rhs.getReadPort());
		return *this;
	}

	template<EnumType T>
	Enum<T>& Enum<T>::operator=(Enum<T>&& rhs) {
        m_resetValue = rhs.m_resetValue;
        assign(rhs.getReadPort());
        rhs.assign(SignalReadPort{ m_node });
        return *this;
    }
//        Enum<T>& operator=(const EnumDefault<T> &defaultValue);


	template<EnumType T>
	Enum<T>& Enum<T>::operator=(T rhs) { 
		assign(rhs); 
		return *this; 
	}

	template<EnumType T>
	void Enum<T>::setExportOverride(const Enum<T>& exportOverride) {
        auto* expOverride = DesignScope::createNode<hlim::Node_ExportOverride>();
        expOverride->connectInput(getReadPort());
        expOverride->connectOverride(exportOverride.getReadPort());
        assign(SignalReadPort(expOverride));		
	}

	template<EnumType T>
	constexpr BitWidth Enum<T>::getWidth() const {
		//return utils::Log2C(magic_enum::enum_count<T>()+1);
        size_t maxWidth = 0;
        for (auto v : magic_enum::enum_values<T>())
            maxWidth = std::max(maxWidth, utils::Log2C((size_t)v+1));

        return maxWidth;
	}

	template<EnumType T>
	hlim::ConnectionType Enum<T>::getConnType() const {
        return hlim::ConnectionType{
            .interpretation = hlim::ConnectionType::BITVEC,
            .width = getWidth().value
        };
    }

	template<EnumType T>
	SignalReadPort Enum<T>::getReadPort() const {
        return getRawDriver();
    }

	template<EnumType T>
	SignalReadPort Enum<T>::getOutPort() const {
		return SignalReadPort{ m_node };
	}

	template<EnumType T>
	std::string_view Enum<T>::getName() const {
        if (auto *sigNode = dynamic_cast<hlim::Node_Signal*>(m_node->getDriver(0).node))
            return sigNode->getName();
        return {};
    }

	template<EnumType T>
	void Enum<T>::setName(std::string name) {
        auto* signal = DesignScope::createNode<hlim::Node_Signal>();
        signal->connectInput(getReadPort());
        signal->setName(name);
        signal->recordStackTrace();

        assign(SignalReadPort(signal), true);
    }

	template<EnumType T>
	void Enum<T>::addToSignalGroup(hlim::SignalGroup *signalGroup)  {
        m_node->moveToSignalGroup(signalGroup);
    }

	template<EnumType T>
	void Enum<T>::setResetValue(T v) {
		m_resetValue = v;	
	}

	template<EnumType T>
	Enum<T> Enum<T>::final() const {
	 	return SignalReadPort{ m_node };
	}

	template<EnumType T>
	void Enum<T>::createNode() {
        HCL_ASSERT(!m_node);
        m_node = DesignScope::createNode<hlim::Node_Signal>();
        m_node->setConnectionType(getConnType());
        m_node->recordStackTrace();
    }

	template<EnumType T>
	void Enum<T>::assign(T v) {
        HCL_DESIGNCHECK_HINT((size_t)v < MAGIC_ENUM_RANGE_MAX, "The values of enums adapted to signals must be within a small range defined by the Magic Enum library!");

        auto* constant = DesignScope::createNode<hlim::Node_Constant>(
            parseBitVector((size_t)v, getWidth().value), hlim::ConnectionType::BITVEC
		);
		constant->setName(std::string(magic_enum::enum_name(v)));
        assign(SignalReadPort(constant));
    }

	template<EnumType T>
	void Enum<T>::assign(SignalReadPort in, bool ignoreConditions) {
        if (auto* scope = ConditionalScope::get(); !ignoreConditions && scope && scope->getId() > m_initialScopeId)
        {
            auto* signal_in = DesignScope::createNode<hlim::Node_Signal>();
            signal_in->connectInput(getRawDriver());

            auto* mux = DesignScope::createNode<hlim::Node_Multiplexer>(2);
            mux->connectInput(0, {.node = signal_in, .port = 0});
            mux->connectInput(1, in); // assign rhs last in case previous port was undefined
            mux->connectSelector(scope->getFullCondition());
            mux->setConditionId(scope->getId());

            in = SignalReadPort(mux);
        }

        m_node->connectInput(in);
    }

	template<EnumType T>
	bool Enum<T>::valid() const {
		return true;	
	}

	template<EnumType T>
	SignalReadPort Enum<T>::getRawDriver() const {
        hlim::NodePort driver = m_node->getDriver(0);
        if (!driver.node)
            driver = hlim::NodePort{ .node = m_node, .port = 0ull };
        return SignalReadPort(driver);
    }



}
