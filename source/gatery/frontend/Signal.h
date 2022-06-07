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

#include <gatery/hlim/coreNodes/Node_Signal.h>
#include <gatery/hlim/NodeIO.h>
#include <gatery/hlim/Node.h>
#include <gatery/hlim/Attributes.h>


#include <gatery/utils/Enumerate.h>
#include <gatery/utils/Preprocessor.h>
#include <gatery/utils/Traits.h>

#include <boost/format.hpp>

#include "BitWidth.h"

namespace gtry {

	class ConditionalScope;
	class SignalPort;

	enum class Expansion
	{
		none,
		zero,
		one,
		sign
	};

	struct SignalReadPort : hlim::NodePort
	{
		SignalReadPort() = default;
		SignalReadPort(const SignalReadPort&) = default;
		explicit SignalReadPort(hlim::BaseNode* node, Expansion policy = Expansion::none) : hlim::NodePort{.node = node, .port = 0}, expansionPolicy(policy) {}
		explicit SignalReadPort(hlim::NodePort np, Expansion policy = Expansion::none) : hlim::NodePort(np), expansionPolicy(policy) {}

		Expansion expansionPolicy = Expansion::none;

		SignalReadPort expand(size_t width, hlim::ConnectionType::Interpretation resultType) const;

		BitWidth width() const { return node->getOutputConnectionType(port).width; }
	};

	inline const hlim::ConnectionType& connType(const SignalReadPort& port) { return port.node->getOutputConnectionType(port.port); }

	class BVec;

	class ElementarySignal {
	public:
		using isSignal = void;
		using isElementarySignal = void;
		
		ElementarySignal();
		ElementarySignal(const ElementarySignal&) noexcept = delete;
		virtual ~ElementarySignal();

		virtual bool valid() const = 0;

		// these methods are undefined for invalid signals (uninitialized)
		virtual BitWidth width() const = 0;
		size_t size() const { return width().value; }
		virtual hlim::ConnectionType connType() const = 0;
		virtual SignalReadPort readPort() const = 0;
		virtual SignalReadPort outPort() const = 0;
		virtual std::string_view getName() const = 0;
		virtual void setName(std::string name) = 0;

		virtual BVec pack() const = 0;
		virtual void unpack(const BVec &b) = 0;

		void attribute(hlim::SignalAttributes attributes);

	protected:
		size_t m_initialScopeId = 0;

	};

	struct NormalizedWidthOperands
	{
		template<typename SigA, typename SigB>
		NormalizedWidthOperands(const SigA&, const SigB&);

		SignalReadPort lhs, rhs;
	};


	template<typename SigA, typename SigB>
	inline NormalizedWidthOperands::NormalizedWidthOperands(const SigA& l, const SigB& r)
	{
		lhs = l.readPort();
		rhs = r.readPort();

		const size_t maxWidth = std::max(lhs.width(), rhs.width()).bits();

		hlim::ConnectionType::Interpretation type = hlim::ConnectionType::BITVEC;
		if(maxWidth == 1 &&
			(l.connType().interpretation != r.connType().interpretation ||
			l.connType().interpretation == hlim::ConnectionType::BOOL))
			type = hlim::ConnectionType::BOOL;

		lhs = lhs.expand(maxWidth, type);
		rhs = rhs.expand(maxWidth, type);
	}


}
