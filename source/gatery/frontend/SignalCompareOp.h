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

#include "SignalLogicOp.h"

#include <gatery/hlim/coreNodes/Node_Compare.h>

#include <gatery/utils/Preprocessor.h>

namespace gtry {


/**
 * @addtogroup gtry_compare Compare Operations for Signals
 * @ingroup gtry_frontend
 * @brief All comparison operations
 * @{
 */


	SignalReadPort makeNode(hlim::Node_Compare::Op op, NormalizedWidthOperands ops);

	inline Bit eq(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Compare::EQ, {lhs, rhs}); }
	inline Bit neq(const BVec& lhs, const BVec& rhs) { return makeNode(hlim::Node_Compare::NEQ, {lhs, rhs}); }

	inline Bit eq(const UInt& lhs, const UInt& rhs) { return makeNode(hlim::Node_Compare::EQ, {lhs, rhs}); }
	inline Bit neq(const UInt& lhs, const UInt& rhs) { return makeNode(hlim::Node_Compare::NEQ, {lhs, rhs}); }
	inline Bit gt(const UInt& lhs, const UInt& rhs) { return makeNode(hlim::Node_Compare::GT, {lhs, rhs}); }
	inline Bit lt(const UInt& lhs, const UInt& rhs) { return makeNode(hlim::Node_Compare::LT, {lhs, rhs}); }
	inline Bit geq(const UInt& lhs, const UInt& rhs) { return makeNode(hlim::Node_Compare::GEQ, {lhs, rhs}); }
	inline Bit leq(const UInt& lhs, const UInt& rhs) { return makeNode(hlim::Node_Compare::LEQ, {lhs, rhs}); }

	inline Bit eq(const SInt& lhs, const SInt& rhs) { return makeNode(hlim::Node_Compare::EQ, {lhs, rhs}); }
	inline Bit neq(const SInt& lhs, const SInt& rhs) { return makeNode(hlim::Node_Compare::NEQ, {lhs, rhs}); }
	Bit gt(const SInt& lhs, const SInt& rhs);
	Bit lt(const SInt& lhs, const SInt& rhs);
	Bit geq(const SInt& lhs, const SInt& rhs);
	Bit leq(const SInt& lhs, const SInt& rhs);

	inline Bit operator == (const BVec& lhs, const BVec& rhs) { return eq(lhs, rhs); }
	inline Bit operator != (const BVec& lhs, const BVec& rhs) { return neq(lhs, rhs); }


	inline Bit operator == (const UInt& lhs, const UInt& rhs) { return eq(lhs, rhs); }
	inline Bit operator != (const UInt& lhs, const UInt& rhs) { return neq(lhs, rhs); }
	inline Bit operator <= (const UInt& lhs, const UInt& rhs) { return leq(lhs, rhs); }
	inline Bit operator >= (const UInt& lhs, const UInt& rhs) { return geq(lhs, rhs); }
	inline Bit operator < (const UInt& lhs, const UInt& rhs) { return lt(lhs, rhs); }
	inline Bit operator > (const UInt& lhs, const UInt& rhs) { return gt(lhs, rhs); }


	inline Bit operator == (const SInt& lhs, const SInt& rhs) { return eq(lhs, rhs); }
	inline Bit operator != (const SInt& lhs, const SInt& rhs) { return neq(lhs, rhs); }
	inline Bit operator <= (const SInt& lhs, const SInt& rhs) { return leq(lhs, rhs); }
	inline Bit operator >= (const SInt& lhs, const SInt& rhs) { return geq(lhs, rhs); }
	inline Bit operator < (const SInt& lhs, const SInt& rhs) { return lt(lhs, rhs); }
	inline Bit operator > (const SInt& lhs, const SInt& rhs) { return gt(lhs, rhs); }




	// Allow eq/neq comparison with BVec

	inline Bit eq(const UInt& lhs, const BVec& rhs) { return makeNode(hlim::Node_Compare::EQ, {lhs, rhs}); }
	inline Bit neq(const UInt& lhs, const BVec& rhs) { return makeNode(hlim::Node_Compare::NEQ, {lhs, rhs}); }
	inline Bit eq(const SInt& lhs, const BVec& rhs) { return makeNode(hlim::Node_Compare::EQ, {lhs, rhs}); }
	inline Bit neq(const SInt& lhs, const BVec& rhs) { return makeNode(hlim::Node_Compare::NEQ, {lhs, rhs}); }
	inline Bit eq(const BVec& lhs, const UInt& rhs) { return makeNode(hlim::Node_Compare::EQ, {lhs, rhs}); }
	inline Bit neq(const BVec& lhs, const UInt& rhs) { return makeNode(hlim::Node_Compare::NEQ, {lhs, rhs}); }
	inline Bit eq(const BVec& lhs, const SInt& rhs) { return makeNode(hlim::Node_Compare::EQ, {lhs, rhs}); }
	inline Bit neq(const BVec& lhs, const SInt& rhs) { return makeNode(hlim::Node_Compare::NEQ, {lhs, rhs}); }

	inline Bit operator == (const UInt& lhs, const BVec& rhs) { return eq(lhs, rhs); }
	inline Bit operator != (const UInt& lhs, const BVec& rhs) { return neq(lhs, rhs); }
	inline Bit operator == (const SInt& lhs, const BVec& rhs) { return eq(lhs, rhs); }
	inline Bit operator != (const SInt& lhs, const BVec& rhs) { return neq(lhs, rhs); }

	inline Bit operator == (const BVec& lhs, const UInt& rhs) { return eq(lhs, rhs); }
	inline Bit operator != (const BVec& lhs, const UInt& rhs) { return neq(lhs, rhs); }
	inline Bit operator == (const BVec& lhs, const SInt& rhs) { return eq(lhs, rhs); }
	inline Bit operator != (const BVec& lhs, const SInt& rhs) { return neq(lhs, rhs); }



	inline Bit eq(const Bit& lhs, const Bit& rhs) { return makeNode(hlim::Node_Compare::EQ, { lhs, rhs }); }
	inline Bit neq(const Bit& lhs, const Bit& rhs) { return makeNode(hlim::Node_Compare::NEQ, { lhs, rhs }); }

	inline Bit operator == (const Bit& lhs, const Bit& rhs) { return eq(lhs, rhs); }
	inline Bit operator != (const Bit& lhs, const Bit& rhs) { return neq(lhs, rhs); }

/**@}*/
}

