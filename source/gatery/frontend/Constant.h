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
#include "BitWidth.h"

#include <gatery/simulation/BitVectorState.h>
#include <gatery/hlim/coreNodes/Node_Constant.h>
#include <gatery/frontend/DesignScope.h>
#include <gatery/frontend/Signal.h>

#include <gatery/utils/Preprocessor.h>
#include <gatery/utils/Traits.h>

#include <string_view>

namespace gtry 
{

/**
 * @addtogroup gtry_frontend
 * @{
 */


	class UInt;

#if 0 // ConstUInt as class study

	class ConstUInt
	{
	public:
		constexpr ConstUInt(uint64_t value, BitWidth width, std::string_view name = "") : m_intValue(value), m_width(width), m_name(name) {}
		constexpr ConstUInt(BitWidth width, std::string_view name = "") : m_width(width), m_name(name) {}
		constexpr ConstUInt(std::string_view value, std::string_view name = "") : m_stringValue(value), m_name(name) {}

		UInt get() const;
		operator UInt () const;

		sim::DefaultBitVectorState state() const;

	private:
		std::optional<std::string_view> m_stringValue;
		std::optional<uint64_t> m_intValue;
		BitWidth m_width;
		std::string_view m_name;
	};

	std::ostream& operator << (std::ostream&, const ConstUInt&);
#else

	class UndefinedVec
	{
	public:
		UndefinedVec(BitWidth width) : m_width(width) {}

		template<BitVectorSignal T>
		operator T () const
		{
			sim::DefaultBitVectorState value;
			value.resize(m_width.bits());
			value.setRange(sim::DefaultConfig::DEFINED, 0, m_width.bits(), false);

			return SignalReadPort{ 
				DesignScope::createNode<hlim::Node_Constant>(
					value, hlim::ConnectionType::BITVEC
				)
			};
		}

	private:
		BitWidth m_width;
	};

	BVec ConstBVec(uint64_t value, BitWidth width, std::string_view name = "");
	BVec ConstBVec(BitWidth width, std::string_view name = ""); // undefined constant

	UInt ConstUInt(uint64_t value, BitWidth width, std::string_view name = "");
	UInt ConstUInt(BitWidth width, std::string_view name = ""); // undefined constant

/*
	SInt ConstSInt(int64_t value, BitWidth width, std::string_view name = "");
	SInt ConstSInt(BitWidth width, std::string_view name = ""); // undefined constant
*/
#endif


#define GTRY_CONST_BVEC_DESC(x, value, desc) \
	struct x { operator UInt () { \
		UInt res = value; \
		res.node()->getNonSignalDriver(0).node->setName(#x); \
		return res; \
	} const char *getName() const { return #x; } auto getValue() const { return value; } const char *getDescription() const { return desc; } };

#define GTRY_CONST_BVEC(x, value) \
	GTRY_CONST_BVEC_DESC(x, value, "")

#define GTRY_CONST_BIT_DESC(x, value, desc) \
	struct x { operator Bit () { \
		Bit res = value; \
		res.node()->getNonSignalDriver(0).node->setName(#x); \
		return res; \
	} const char *getName() const { return #x; } auto getValue() const { return value; } const char *getDescription() const { return desc; } };

#define GTRY_CONST_BIT(x, value) \
	GTRY_CONST_BIT_DESC(x, value, "")

/// @}

}
