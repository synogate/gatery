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
#include "Scope.h"

#include <gatery/hlim/coreNodes/Node_Signal.h>
#include <gatery/hlim/coreNodes/Node_Rewire.h>
#include <gatery/hlim/coreNodes/Node_Shift.h>


#include <gatery/utils/Preprocessor.h>
#include <gatery/utils/Traits.h>


#include <boost/format.hpp>

#include <optional>


namespace gtry {

/**
	* @addtogroup gtry_bitshift Bitshift Operations for Signals
	* @ingroup gtry_frontend
	* @brief All the operations for shifting and rotating bit vectors.
	* @{
	*/

/// Shift left by specified amount (must be non-negative), retaining size and inserting zeros.
/// @deprecated Will be removed when making all shifts dynamic (initially)
BVec shl(const BVec& signal, int amount); // TODO (remove)
/// Shift right by specified amount (must be non-negative), retaining size and inserting zeros.
/// @deprecated Will be removed when making all shifts dynamic (initially)
BVec shr(const BVec& signal, int amount); // TODO (remove)

/// Shift left by specified amount (must be non-negative), retaining size and inserting zeros.
/// @deprecated Will be removed when making all shifts dynamic (initially)
UInt shl(const UInt& signal, int amount); // TODO (remove)
/// Shift right by specified amount (must be non-negative), retaining size and inserting zeros.
/// @deprecated Will be removed when making all shifts dynamic (initially)
UInt shr(const UInt& signal, int amount); // TODO (remove)

/// Shift left by specified amount (must be non-negative), retaining size and inserting zeros.
/// @deprecated Will be removed when making all shifts dynamic (initially)
SInt shl(const SInt& signal, int amount); // TODO (remove)
/// Shift right by specified amount (must be non-negative), retaining size and duplicating the sign bit.
/// @deprecated Will be removed when making all shifts dynamic (initially)
SInt shr(const SInt& signal, int amount); // TODO (remove)

/// Shift left by specified amount (must be non-negative), retaining size and inserting zeros.
/// @deprecated Will be removed when making all shifts dynamic (initially)
inline BVec operator<<(const BVec &signal, int amount) { return shl(signal, amount); } // TODO (remove)
/// Shift right by specified amount (must be non-negative), retaining size and inserting zeros.
/// @deprecated Will be removed when making all shifts dynamic (initially)
inline BVec operator>>(const BVec &signal, int amount) { return shr(signal, amount); } // TODO (remove)

/// Shift left by specified amount (must be non-negative), retaining size and inserting zeros.
/// @deprecated Will be removed when making all shifts dynamic (initially)
inline UInt operator<<(const UInt &signal, int amount) { return shl(signal, amount); } // TODO (remove)
/// Shift right by specified amount (must be non-negative), retaining size and inserting zeros.
/// @deprecated Will be removed when making all shifts dynamic (initially)
inline UInt operator>>(const UInt &signal, int amount) { return shr(signal, amount); } // TODO (remove)

/// Shift left by specified amount (must be non-negative), retaining size and inserting zeros.
/// @deprecated Will be removed when making all shifts dynamic (initially)
inline SInt operator<<(const SInt &signal, int amount) { return shl(signal, amount); } // TODO (remove)
/// Shift right by specified amount (must be non-negative), retaining size and duplicating the sign bit.
/// @deprecated Will be removed when making all shifts dynamic (initially)
inline SInt operator>>(const SInt &signal, int amount) { return shr(signal, amount); } // TODO (remove)

/// Shift left by specified amount (must be non-negative), retaining size and inserting zeros.
/// @deprecated Will be removed when making all shifts dynamic (initially)
template<BitVectorDerived T> 
inline T &operator<<=(T &signal, int amount) { return signal = shl(signal, amount); } // TODO (remove)
/// Shift right by specified amount (must be non-negative), retaining size and inserting zeros or duplicating the sign bit for SInt.
/// @deprecated Will be removed when making all shifts dynamic (initially)
template<BitVectorDerived T> 
inline T &operator>>=(T &signal, int amount)  { return signal = shr(signal, amount); } // TODO (remove)



BVec rot(const BVec& signal, int amount); // TODO (remove)
UInt rot(const UInt& signal, int amount); // TODO (remove)
SInt rot(const SInt& signal, int amount); // TODO (remove)

/// Rotate left (from LSB to MSB)
/// @deprecated Will be removed when making all shifts dynamic (initially)
inline BVec rotl(const BVec& signal, int amount) { return rot(signal, amount); }  // TODO (remove)
/// Rotate right (from MSB to LSB)
/// @deprecated Will be removed when making all shifts dynamic (initially)
inline BVec rotr(const BVec& signal, int amount) { return rot(signal, -amount); }  // TODO (remove)
/// Rotate left (from LSB to MSB)
/// @deprecated Will be removed when making all shifts dynamic (initially)
inline BVec rotl(const BVec& signal, size_t amount) { return rot(signal, int(amount)); }  // TODO (remove)
/// Rotate right (from MSB to LSB)
/// @deprecated Will be removed when making all shifts dynamic (initially)
inline BVec rotr(const BVec& signal, size_t amount) { return rot(signal, -int(amount)); }  // TODO (remove)

/// Rotate left (from LSB to MSB)
/// @deprecated Will be removed when making all shifts dynamic (initially)
inline UInt rotl(const UInt& signal, int amount) { return rot(signal, amount); }  // TODO (remove)
/// Rotate right (from MSB to LSB)
/// @deprecated Will be removed when making all shifts dynamic (initially)
inline UInt rotr(const UInt& signal, int amount) { return rot(signal, -amount); }  // TODO (remove)
/// Rotate left (from LSB to MSB)
/// @deprecated Will be removed when making all shifts dynamic (initially)
inline UInt rotl(const UInt& signal, size_t amount) { return rot(signal, int(amount)); }  // TODO (remove)
/// Rotate right (from MSB to LSB)
/// @deprecated Will be removed when making all shifts dynamic (initially)
inline UInt rotr(const UInt& signal, size_t amount) { return rot(signal, -int(amount)); }  // TODO (remove)

/// Rotate left (from LSB to MSB)
/// @deprecated Will be removed when making all shifts dynamic (initially)
inline SInt rotl(const SInt& signal, int amount) { return rot(signal, amount); }  // TODO (remove)
/// Rotate right (from MSB to LSB)
/// @deprecated Will be removed when making all shifts dynamic (initially)
inline SInt rotr(const SInt& signal, int amount) { return rot(signal, -amount); }  // TODO (remove)
/// Rotate left (from LSB to MSB)
/// @deprecated Will be removed when making all shifts dynamic (initially)
inline SInt rotl(const SInt& signal, size_t amount) { return rot(signal, int(amount)); }  // TODO (remove)
/// Rotate right (from MSB to LSB)
/// @deprecated Will be removed when making all shifts dynamic (initially)
inline SInt rotr(const SInt& signal, size_t amount) { return rot(signal, -int(amount)); }  // TODO (remove)

SignalReadPort internal_shift(const SignalReadPort& signal, const SignalReadPort& amount, hlim::Node_Shift::dir direction, hlim::Node_Shift::fill fill);
UInt shr(const UInt& signal, size_t amount, const Bit& arithmetic);
UInt shr(const UInt& signal, const UInt& amount, const Bit& arithmetic);

inline BVec zshl(const BVec& signal, const UInt& amount) { return BVec{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::left, hlim::Node_Shift::fill::zero)}; }
inline BVec oshl(const BVec& signal, const UInt& amount) { return BVec{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::left, hlim::Node_Shift::fill::one)}; }
inline BVec sshl(const BVec& signal, const UInt& amount) { return BVec{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::left, hlim::Node_Shift::fill::last)}; }
inline BVec zshr(const BVec& signal, const UInt& amount) { return BVec{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::right, hlim::Node_Shift::fill::zero)}; }
inline BVec oshr(const BVec& signal, const UInt& amount) { return BVec{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::right, hlim::Node_Shift::fill::one)}; }
inline BVec sshr(const BVec& signal, const UInt& amount) { return BVec{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::right, hlim::Node_Shift::fill::last)}; }
inline BVec rotl(const BVec& signal, const UInt& amount) { return BVec{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::left, hlim::Node_Shift::fill::rotate)}; }
inline BVec rotr(const BVec& signal, const UInt& amount) { return BVec{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::right, hlim::Node_Shift::fill::rotate)}; }

inline UInt zshl(const UInt& signal, const UInt& amount) { return UInt{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::left, hlim::Node_Shift::fill::zero)}; }
inline UInt oshl(const UInt& signal, const UInt& amount) { return UInt{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::left, hlim::Node_Shift::fill::one)}; }
inline UInt sshl(const UInt& signal, const UInt& amount) { return UInt{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::left, hlim::Node_Shift::fill::last)}; }
inline UInt zshr(const UInt& signal, const UInt& amount) { return UInt{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::right, hlim::Node_Shift::fill::zero)}; }
inline UInt oshr(const UInt& signal, const UInt& amount) { return UInt{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::right, hlim::Node_Shift::fill::one)}; }
inline UInt sshr(const UInt& signal, const UInt& amount) { return UInt{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::right, hlim::Node_Shift::fill::last)}; }
inline UInt rotl(const UInt& signal, const UInt& amount) { return UInt{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::left, hlim::Node_Shift::fill::rotate)}; }
inline UInt rotr(const UInt& signal, const UInt& amount) { return UInt{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::right, hlim::Node_Shift::fill::rotate)}; }


inline SInt zshl(const SInt& signal, const UInt& amount) { return SInt{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::left, hlim::Node_Shift::fill::zero)}; }
inline SInt oshl(const SInt& signal, const UInt& amount) { return SInt{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::left, hlim::Node_Shift::fill::one)}; }
inline SInt sshl(const SInt& signal, const UInt& amount) { return SInt{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::left, hlim::Node_Shift::fill::last)}; }
inline SInt zshr(const SInt& signal, const UInt& amount) { return SInt{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::right, hlim::Node_Shift::fill::zero)}; }
inline SInt oshr(const SInt& signal, const UInt& amount) { return SInt{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::right, hlim::Node_Shift::fill::one)}; }
inline SInt sshr(const SInt& signal, const UInt& amount) { return SInt{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::right, hlim::Node_Shift::fill::last)}; }
inline SInt rotl(const SInt& signal, const UInt& amount) { return SInt{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::left, hlim::Node_Shift::fill::rotate)}; }
inline SInt rotr(const SInt& signal, const UInt& amount) { return SInt{internal_shift(signal.readPort(), amount.readPort(), hlim::Node_Shift::dir::right, hlim::Node_Shift::fill::rotate)}; }



inline BVec shl(const BVec& signal, const UInt& amount) { return zshl(signal, amount); }
inline BVec shr(const BVec& signal, const UInt& amount) { return zshr(signal, amount); }

inline UInt shl(const UInt& signal, const UInt& amount) { return zshl(signal, amount); }
inline UInt shr(const UInt& signal, const UInt& amount) { return zshr(signal, amount); }

inline SInt shl(const SInt& signal, const UInt& amount) { return zshl(signal, amount); }
inline SInt shr(const SInt& signal, const UInt& amount) { return sshr(signal, amount); }



// These are explicit (non-templated) for now to allow implicit parameter conversion
inline BVec operator<<(const BVec& signal, const UInt& amount) { return shl(signal, amount); }
inline BVec operator>>(const BVec& signal, const UInt& amount) { return shr(signal, amount); }

inline UInt operator<<(const UInt& signal, const UInt& amount) { return shl(signal, amount); }
inline UInt operator>>(const UInt& signal, const UInt& amount) { return shr(signal, amount); }

inline SInt operator<<(const SInt& signal, const UInt& amount) { return shl(signal, amount); }
inline SInt operator>>(const SInt& signal, const UInt& amount) { return shr(signal, amount); }


template<BitVectorDerived T>
inline T& operator<<=(T& signal, const UInt& amount) { return signal = shl(signal, amount); }

template<BitVectorDerived T>
inline T& operator>>=(T& signal, const UInt& amount) { return signal = shr(signal, amount); }

/**@}*/
}
