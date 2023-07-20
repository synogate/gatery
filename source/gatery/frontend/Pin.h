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

#include "DesignScope.h"

#include "Signal.h"
#include "Compound.h"

#include <gatery/hlim/coreNodes/Node_Pin.h>
#include <gatery/hlim/coreNodes/Node_Signal.h>
#include <gatery/hlim/NodePtr.h>

namespace gtry {

	class Bit;
	class BVec;
	class UInt;
	class SInt;


	class BasePin {
		public:
			inline hlim::Node_Pin *node() { return m_pinNode.get(); }
			inline hlim::Node_Pin *node() const { return m_pinNode.get(); }
		protected:
			hlim::NodePtr<hlim::Node_Pin> m_pinNode;
	};		

	class BaseOutputPin : public BasePin {
		public:
			BaseOutputPin(hlim::NodePort nodePort, std::string name, const hlim::Node_Pin::PinNodeParameter& params = {});
	};	

	class OutputPin : public BaseOutputPin {
		public:
			OutputPin(const Bit &bit, const hlim::Node_Pin::PinNodeParameter& params = {});

			inline OutputPin &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
			inline OutputPin &setDifferential(std::string_view posPrefix = "_p", std::string_view negPrefix = "_n") { m_pinNode->setDifferential(posPrefix, negPrefix); return *this; }

	};

	class OutputPins : public BaseOutputPin {
		public:
			OutputPins(const ElementarySignal& bitVector, const hlim::Node_Pin::PinNodeParameter& params = {}) : BaseOutputPin(bitVector.readPort(), std::string(bitVector.getName()), params) { }

			inline OutputPins &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
			inline OutputPins &setDifferential(std::string_view posPrefix = "_p", std::string_view negPrefix = "_n") { m_pinNode->setDifferential(posPrefix, negPrefix); return *this; }
	};


	class BaseInputPin : public BasePin { 
		public:
			BaseInputPin();
	};

	class InputPin : public BaseInputPin {
		public:
			InputPin(const hlim::Node_Pin::PinNodeParameter& params = {});
			operator Bit () const;
			inline InputPin &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
	};

	class InputPins : public BaseInputPin {
		public:
			InputPins(BitWidth width, const hlim::Node_Pin::PinNodeParameter& params = {});
			operator UInt () const;

			template<BitVectorDerived T> requires (!std::same_as<UInt, T>)
			explicit operator T () const { return T(SignalReadPort({.node=m_pinNode, .port=0ull})); }

			inline InputPins &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
	};

	class BaseTristatePin : public BasePin { 
		public:
			BaseTristatePin(hlim::NodePort nodePort, std::string name, const Bit &outputEnable);
	};

	class TristatePin : public BaseTristatePin {
		public:
			TristatePin(const Bit &bit, const Bit &outputEnable);
			operator Bit () const;
			inline TristatePin &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
	};

	class TristatePins : public BaseTristatePin {
		public:
			template<BitVectorDerived T>
			TristatePins(const T &bitVector, const Bit &outputEnable) : BaseTristatePin(bitVector.readPort(), std::string(bitVector.getName()), outputEnable) { }

			operator UInt () const;

			template<BitVectorDerived T> requires (!std::same_as<UInt, T>)
			explicit operator T () const { return T(SignalReadPort({.node=m_pinNode, .port=0ull})); }

			inline TristatePins &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
	};	


	class BaseBidirPin : public BasePin { 
		public:
			BaseBidirPin(hlim::NodePort nodePort, std::string name);
	};

	class BidirPin : public BaseBidirPin {
		public:
			BidirPin(const Bit &bit);
			operator Bit () const;
			inline BidirPin &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
	};

	class BidirPins : public BaseBidirPin {
		public:
			template<BitVectorDerived T>
			BidirPins(const T &bitVector) : BaseBidirPin(bitVector.readPort(), std::string(bitVector.getName())) { }

			operator UInt () const;

			template<BitVectorDerived T> requires (!std::same_as<UInt, T>)
			explicit operator T () const { return T(SignalReadPort({.node=m_pinNode, .port=0ull})); }

			inline BidirPins &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
	};	


	inline OutputPin pinOut(const Bit &bit, const hlim::Node_Pin::PinNodeParameter& params = {}) { return OutputPin(bit, params); }
	template<BitVectorValue T>
	inline OutputPins pinOut(const T& bitVector, const hlim::Node_Pin::PinNodeParameter& params = {}) {
		auto sig = (ValueToBaseSignal<T>)bitVector;
		HCL_DESIGNCHECK_HINT(sig.valid(), "Can not pinOut uninitialized bitvectors");
		return OutputPins(sig, params);
	}

	OutputPins pinOut(const InputPins &input, const hlim::Node_Pin::PinNodeParameter& params = {});

	inline InputPin pinIn(const hlim::Node_Pin::PinNodeParameter& params = {}) { return InputPin(params); }
	inline InputPins pinIn(BitWidth width, const hlim::Node_Pin::PinNodeParameter& params = {}) { return InputPins(width, params); }

	void pinIn(Signal auto&& signal, std::string prefix);
	void pinOut(Signal auto&& signal, std::string prefix);

	inline TristatePin tristatePin(const Bit &bit, const Bit &outputEnable) { return TristatePin(bit, outputEnable); }

	template<BitVectorValue T>
	inline TristatePins tristatePin(const T &bitVector, const Bit &outputEnable) { return TristatePins((ValueToBaseSignal<T>)bitVector, outputEnable); }

	inline BidirPin bidirPin(const Bit &bit) { return BidirPin(bit); }

	template<BitVectorValue T>
	inline BidirPins bidirPin(const T &bitVector) { return BidirPins((ValueToBaseSignal<T>)bitVector); }

}

namespace gtry
{
	namespace internal
	{
		struct PinVisitor : CompoundNameVisitor
		{
			void reverse() final { isReverse = !isReverse; }

			void operator () (ElementarySignal& vec) final
			{
				if (isReverse)
				{
					auto name = makeName();
					HCL_DESIGNCHECK_HINT(vec.valid(), "Can not pinOut uninitialized bitvectors but the member " + name + " is uninitialized!");
					OutputPins(vec).setName(name);
				}
				else
				{
					BaseInputPin pin;
					if (vec.connType().isBool())
						pin.node()->setBool();
					else
						pin.node()->setWidth(vec.width().bits());
					pin.node()->setName(makeName());
					vec.assign(SignalReadPort{ pin.node() });
				}
			}

			bool isReverse = false;
		};
	}

	void pinIn(Signal auto&& signal, std::string prefix)
	{
		internal::PinVisitor v;
		v.enter(prefix);
		VisitCompound<std::remove_reference_t<decltype(signal)>>{}(signal, v);
		v.leave();
	}

	void pinOut(Signal auto&& signal, std::string prefix)
	{
		internal::PinVisitor v;
		v.reverse();
		v.enter(prefix);
		VisitCompound<std::remove_reference_t<decltype(signal)>>{}(signal, v);
		v.leave();
	}
}
