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

#include "Signal.h"
#include "Bit.h"
#include "BVec.h"
#include "UInt.h"
#include "SInt.h"

#include <gatery/hlim/coreNodes/Node_Logic.h>

#include <gatery/utils/Preprocessor.h>
#include <gatery/utils/Traits.h>


#include <boost/format.hpp>


namespace gtry {

/**
 * @addtogroup gtry_logic Logical Operations for Signals
 * @ingroup gtry_frontend
 * @brief All bitwise logic operations
 * @{
 */


	SignalReadPort makeNode(hlim::Node_Logic::Op op, const ElementarySignal& in);
	SignalReadPort makeNode(hlim::Node_Logic::Op op, NormalizedWidthOperands ops);

	namespace internal_logic {
		template<BaseSignal T>
		inline T land(const T& lhs, const T& rhs) { return makeNode(hlim::Node_Logic::AND, {lhs, rhs}); }
		template<BaseSignal T>
		inline T lnand(const T& lhs, const T& rhs) { return makeNode(hlim::Node_Logic::NAND, {lhs, rhs}); }
		template<BaseSignal T>
		inline T lor(const T& lhs, const T& rhs) { return makeNode(hlim::Node_Logic::OR, {lhs, rhs}); }
		template<BaseSignal T>
		inline T lnor(const T& lhs, const T& rhs) { return makeNode(hlim::Node_Logic::NOR, {lhs, rhs}); }
		template<BaseSignal T>
		inline T lxor(const T& lhs, const T& rhs) { return makeNode(hlim::Node_Logic::XOR, {lhs, rhs}); }
		template<BaseSignal T>
		inline T lxnor(const T& lhs, const T& rhs) { return makeNode(hlim::Node_Logic::EQ, {lhs, rhs}); }
		template<BaseSignal T>
		inline T lnot(const T& lhs) { return makeNode(hlim::Node_Logic::NOT, lhs); }
	}

	/// @brief Matches any pair of types of which one is a (base)signal, both are base signal values, and both can be converted to the same signal type.
	/// @details This is the condition for being accepted as operands of regular logical operations (land, lor, ...)
	template<typename TL, typename TR>
	concept ValidLogicPair = 
		BaseSignalValue<TL> && BaseSignalValue<TR> &&				 // Both must be signals or convertible to signals
		(BaseSignal<TL> || BaseSignal<TR>) &&						 // One of them must be an actual signal
		std::is_same_v<ValueToBaseSignal<TL>, ValueToBaseSignal<TR>>; // They must convert to the same signal type

	/// Logical and between two equivalent signal values (signal or literal) of which one must be an actual signal.
	template<typename TL, typename TR> requires (ValidLogicPair<TL, TR>)
	inline auto land (const TL& lhs, const TR& rhs) { return internal_logic::land<ValueToBaseSignal<TL>>(lhs, rhs); }
	/// Logical nand between two equivalent signal values (signal or literal) of which one must be an actual signal.
	template<typename TL, typename TR> requires (ValidLogicPair<TL, TR>)
	inline auto lnand(const TL& lhs, const TR& rhs) { return internal_logic::lnand<ValueToBaseSignal<TL>>(lhs, rhs); }
	/// Logical or between two equivalent signal values (signal or literal) of which one must be an actual signal.
	template<typename TL, typename TR> requires (ValidLogicPair<TL, TR>)
	inline auto lor  (const TL& lhs, const TR& rhs) { return internal_logic::lor<ValueToBaseSignal<TL>>(lhs, rhs); }
	/// Logical nor between two equivalent signal values (signal or literal) of which one must be an actual signal.
	template<typename TL, typename TR> requires (ValidLogicPair<TL, TR>)
	inline auto lnor (const TL& lhs, const TR& rhs) { return internal_logic::lnor<ValueToBaseSignal<TL>>(lhs, rhs); }
	/// Logical xor between two equivalent signal values (signal or literal) of which one must be an actual signal.
	template<typename TL, typename TR> requires (ValidLogicPair<TL, TR>)
	inline auto lxor (const TL& lhs, const TR& rhs) { return internal_logic::lxor<ValueToBaseSignal<TL>>(lhs, rhs); }
	/// Logical xnor between two equivalent signal values (signal or literal) of which one must be an actual signal.
	template<typename TL, typename TR> requires (ValidLogicPair<TL, TR>)
	inline auto lxnor(const TL& lhs, const TR& rhs) { return internal_logic::lxnor<ValueToBaseSignal<TL>>(lhs, rhs); }
	/// Logical not of any base signal.
	template<BaseSignal TL>
	inline auto lnot (const TL& lhs)				{ return internal_logic::lnot<TL>(lhs); }


	/// Logical and between two equivalent signal values (signal or literal) of which one must be an actual signal.
	template<typename TL, typename TR> requires (ValidLogicPair<TL, TR>)
	inline auto operator & (const TL& lhs, const TR& rhs) { return land(lhs, rhs); }
	/// Logical or between two equivalent signal values (signal or literal) of which one must be an actual signal.
	template<typename TL, typename TR> requires (ValidLogicPair<TL, TR>)
	inline auto operator | (const TL& lhs, const TR& rhs) { return lor(lhs, rhs); }
	/// Logical xor between two equivalent signal values (signal or literal) of which one must be an actual signal.
	template<typename TL, typename TR> requires (ValidLogicPair<TL, TR>)
	inline auto operator ^ (const TL& lhs, const TR& rhs) { return lxor(lhs, rhs); }
	/// Logical not of any base signal.
	template<BaseSignal TL>
	inline auto operator ~ (const TL& lhs) { return lnot(lhs); }



	/// Computes logical and between a BaseSignal and value of the same type (signal or literal)
	template<BaseSignal SignalType, std::convertible_to<SignalType> RType> 
	inline SignalType& operator &= (SignalType& lhs, const RType& rhs) { return lhs = land(lhs, rhs); }
	/// Computes logical or between a BaseSignal and value of the same type (signal or literal)
	template<BaseSignal SignalType, std::convertible_to<SignalType> RType> 
	inline SignalType& operator |= (SignalType& lhs, const RType& rhs) { return lhs = lor(lhs, rhs); }
	/// Computes logical xor between a BaseSignal and value of the same type (signal or literal)
	template<BaseSignal SignalType, std::convertible_to<SignalType> RType> 
	inline SignalType& operator ^= (SignalType& lhs, const RType& rhs) { return lhs = lxor(lhs, rhs); }


	inline Bit operator ! (const Bit& lhs) { return lnot(lhs); }


	// All bitvector derived types can be logically combined with bits, which are implicitly broadcast to the size of the vector.

	// We use a bit of magic here: A concept matches a pair of types of which one must be a BitVector value (signal or convertible
	// to BitVector derived) and the other must be Bit value (signal or convertible to signal). The concept matches independent of
	// the order (Bit can be first or last). An overloaded sext_bit function is applied to both operands which sign extends bits
	// and forwards or converts everything else. This way, whichever operand is the bit is broadcast. A second template getVecType extracts
	// the vector type for determining the return type.

	/// @brief Matches any pair of types of which one is a BitVector value, the other is a Bit value, and one of which is an actual signal.
	/// @details This is the condition for being accepted as operands of logical operations (land, lor, ...) between bits and bit vectors.
	template<typename TL, typename TR>
	concept ValidVecBitPair = 
		(BitValue<TL> || BitValue<TR>) &&							 // One must be a bit value (signal or literal)
		(BitVectorValue<TL> || BitVectorValue<TR>);				   // One must be a BitVector value (signal or literal)

	namespace internal_logic {
		// For everything that can be converted to bits, do convert and sign extend
		template<BitValue T>
		inline auto sext_bit(const T &bit) { return sext((Bit)bit); }
		// Convert literals
		template<BitVectorLiteral T>
		inline auto sext_bit(const T &v) { return ValueToBaseSignal<T>{v}; }
		// Forward signals
		template<BitVectorSignal T>
		inline const T& sext_bit(const T &v) { return v; }

		template<typename TL, typename TR> struct get_bitvector_type { };
		template<BitVectorValue TL, typename TR> struct get_bitvector_type<TL, TR> { using type = ValueToBaseSignal<TL>; };
		template<typename TL, BitVectorValue TR> struct get_bitvector_type<TL, TR> { using type = ValueToBaseSignal<TR>; };
		template<BitVectorValue TL, BitVectorValue TR> requires (std::same_as<ValueToBaseSignal<TL>, ValueToBaseSignal<TR>>)
		struct get_bitvector_type<TL, TR> { using type = ValueToBaseSignal<TR>; };

		/// Given two types returns whichever type of the two is BitVector derived
		template<typename TL, typename TR> 
		using getVecType = typename get_bitvector_type<TL, TR>::type;
	}


	/// Computes logical and between a BitVector value and a broadcasted Bit value. They can be in any order, but one must be an actual signal.
	template<typename TL, typename TR> requires (ValidVecBitPair<TL, TR>)
	inline internal_logic::getVecType<TL, TR> land(const TL& lhs, const TR& rhs) { 
		return makeNode(hlim::Node_Logic::AND, {internal_logic::sext_bit(lhs), internal_logic::sext_bit(rhs)}); 
	}

	/// Computes logical nand between a BitVector value and a broadcasted Bit value. They can be in any order, but one must be an actual signal.
	template<typename TL, typename TR> requires (ValidVecBitPair<TL, TR>)
	inline internal_logic::getVecType<TL, TR> lnand(const TL& lhs, const TR& rhs) { 
		return makeNode(hlim::Node_Logic::NAND, {internal_logic::sext_bit(lhs), internal_logic::sext_bit(rhs)}); 
	}

	/// Computes logical or between a BitVector value and a broadcasted Bit value. They can be in any order, but one must be an actual signal.
	template<typename TL, typename TR> requires (ValidVecBitPair<TL, TR>)
	inline internal_logic::getVecType<TL, TR> lor(const TL& lhs, const TR& rhs) { 
		return makeNode(hlim::Node_Logic::OR, {internal_logic::sext_bit(lhs), internal_logic::sext_bit(rhs)}); 
	}

	/// Computes logical nor between a BitVector value and a broadcasted Bit value. They can be in any order, but one must be an actual signal.
	template<typename TL, typename TR> requires (ValidVecBitPair<TL, TR>)
	inline internal_logic::getVecType<TL, TR> lnor(const TL& lhs, const TR& rhs) { 
		return makeNode(hlim::Node_Logic::NOR, {internal_logic::sext_bit(lhs), internal_logic::sext_bit(rhs)}); 
	}

	/// Computes logical xor between a BitVector value and a broadcasted Bit value. They can be in any order, but one must be an actual signal.
	template<typename TL, typename TR> requires (ValidVecBitPair<TL, TR>)
	inline internal_logic::getVecType<TL, TR> lxor(const TL& lhs, const TR& rhs) { 
		return makeNode(hlim::Node_Logic::XOR, {internal_logic::sext_bit(lhs), internal_logic::sext_bit(rhs)}); 
	}

	/// Computes logical xnor between a BitVector value and a broadcasted Bit value. They can be in any order, but one must be an actual signal.
	template<typename TL, typename TR> requires (ValidVecBitPair<TL, TR>)
	inline internal_logic::getVecType<TL, TR> lxnor(const TL& lhs, const TR& rhs) { 
		return makeNode(hlim::Node_Logic::EQ, {internal_logic::sext_bit(lhs), internal_logic::sext_bit(rhs)}); 
	}


	/// Computes logical and between a BitVector value and a broadcasted Bit value. They can be in any order, but one must be an actual signal.
	template<typename TL, typename TR> requires (ValidVecBitPair<TL, TR>)
	inline auto operator & (const TL& lhs, const TR& rhs) { return land(lhs, rhs); }

	/// Computes logical or between a BitVector value and a broadcasted Bit value. They can be in any order, but one must be an actual signal.
	template<typename TL, typename TR> requires (ValidVecBitPair<TL, TR>)
	inline auto operator | (const TL& lhs, const TR& rhs) { return lor(lhs, rhs); }

	/// Computes logical xor between a BitVector value and a broadcasted Bit value. They can be in any order, but one must be an actual signal.
	template<typename TL, typename TR> requires (ValidVecBitPair<TL, TR>)
	inline auto operator ^ (const TL& lhs, const TR& rhs) { return lxor(lhs, rhs); }



	/// Computes logical and between a BaseSignal and a (potentially broadcasted) Bit value.
	template<BaseSignal SignalType>
	inline SignalType& operator &= (SignalType& lhs, const Bit& rhs) { return lhs = land(lhs, rhs); }
	/// Computes logical or between a BaseSignal and a (potentially broadcasted) Bit value.
	template<BaseSignal SignalType>
	inline SignalType& operator |= (SignalType& lhs, const Bit& rhs) { return lhs = lor(lhs, rhs); }
	/// Computes logical xor between a BaseSignal and a (potentially broadcasted) Bit value.
	template<BaseSignal SignalType>
	inline SignalType& operator ^= (SignalType& lhs, const Bit& rhs) { return lhs = lxor(lhs, rhs); }

/**@}*/
}

