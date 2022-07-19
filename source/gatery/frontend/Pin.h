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
			BaseOutputPin(hlim::NodePort nodePort, std::string name);
	};	

	class OutputPin : public BaseOutputPin {
		public:
			OutputPin(const Bit &bit);

			inline OutputPin &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
			inline OutputPin &setDifferential(std::string_view posPrefix = "_p", std::string_view negPrefix = "_n") { m_pinNode->setDifferential(posPrefix, negPrefix); return *this; }

	};

	class OutputPins : public BaseOutputPin {
		public:
			OutputPins(const ElementarySignal& bitVector) : BaseOutputPin(bitVector.readPort(), std::string(bitVector.getName())) { }

			inline OutputPins &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
			inline OutputPins &setDifferential(std::string_view posPrefix = "_p", std::string_view negPrefix = "_n") { m_pinNode->setDifferential(posPrefix, negPrefix); return *this; }
	};


	class BaseInputPin : public BasePin { 
		public:
			BaseInputPin();
	};

	class InputPin : public BaseInputPin {
		public:
			InputPin();
			operator Bit () const;
			inline InputPin &setName(std::string name) { m_pinNode->setName(std::move(name)); return *this; }
	};

	class InputPins : public BaseInputPin {
		public:
			InputPins(BitWidth width);
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


	inline OutputPin pinOut(const Bit &bit) { return OutputPin(bit); }
	template<BitVectorValue T>
	inline OutputPins pinOut(const T &bitVector) { return OutputPins((ValueToBaseSignal<T>)bitVector); }

	OutputPins pinOut(const InputPins &input);

	inline InputPin pinIn() { return InputPin(); }
	inline InputPins pinIn(BitWidth width) { return InputPins(width); }

	void pinIn(Signal auto& signal, std::string prefix);
	void pinOut(Signal auto& signal, std::string prefix);

	inline TristatePin tristatePin(const Bit &bit, const Bit &outputEnable) { return TristatePin(bit, outputEnable); }

	template<BitVectorValue T>
	inline TristatePins tristatePin(const T &bitVector, const Bit &outputEnable) { return TristatePins((ValueToBaseSignal<T>)bitVector, outputEnable); }

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
					OutputPins(vec).setName(makeName());
				}
				else
				{
					BaseInputPin pin;
					if (vec.connType().interpretation == hlim::ConnectionType::BOOL)
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

	void pinIn(Signal auto& signal, std::string prefix)
	{
		internal::PinVisitor v;
		v.enter(prefix);
		VisitCompound<std::remove_reference_t<decltype(signal)>>{}(signal, v);
		v.leave();
	}

	void pinOut(Signal auto& signal, std::string prefix)
	{
		internal::PinVisitor v;
		v.reverse();
		v.enter(prefix);
		VisitCompound<std::remove_reference_t<decltype(signal)>>{}(signal, v);
		v.leave();
	}
}
