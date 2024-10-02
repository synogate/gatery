/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2024 Michael Offel, Andreas Ley

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
#include "gatery/scl_pch.h"

#include "PipelinedMath.h"

#include <gatery/frontend/Area.h>
#include <gatery/frontend/UInt.h>
#include <gatery/frontend/Compound.h>
#include <gatery/frontend/SignalArithmeticOp.h>
#include <gatery/utils/Preprocessor.h>

namespace gtry::scl::math {

UInt pipelinedMul(UInt a, UInt b, BitWidth resultW, size_t resultOffset)
{
	Area area("scl_pipelinedMul", true);
	area.template createMetaInfo<PipelinedMulMeta>()->resultOffset = resultOffset;
	HCL_NAMED(a);
	HCL_NAMED(b);

	BitWidth immW = resultW + resultOffset;

	auto resizedA = resizeTo(a, immW);
	HCL_NAMED(resizedA);
	auto resizedB = resizeTo(b, immW);
	HCL_NAMED(resizedB);

	UInt out = (resizedA * resizedB).upper(resultW);
	HCL_NAMED(out);
	return out;
}

SInt pipelinedMul(SInt a, SInt b, BitWidth resultW, size_t resultOffset)
{
	Area area("scl_pipelinedMul", true);
	area.template createMetaInfo<PipelinedMulMeta>()->resultOffset = resultOffset;
	HCL_NAMED(a);
	HCL_NAMED(b);

	BitWidth immW = resultW + resultOffset;

	auto resizedA = resizeTo(a, immW);
	HCL_NAMED(resizedA);
	auto resizedB = resizeTo(b, immW);
	HCL_NAMED(resizedB);

	SInt out = (resizedA * resizedB).upper(resultW);
	HCL_NAMED(out);
	return out;
}

}