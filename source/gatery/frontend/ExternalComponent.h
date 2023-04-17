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


#include <gatery/frontend/Bit.h>
#include <gatery/frontend/BVec.h>
#include <gatery/hlim/supportNodes/Node_External.h>

namespace gtry {

/**
 * @addtogroup gtry_frontend
 * @{
 */

	// NOTE: ExternalComonent is depricated. Use ExternalModule instead.
	class ExternalComponent : public hlim::Node_External
	{
	public:
		using GenericParameter = hlim::GenericParameter;

		virtual void setInput(size_t input, const Bit &bit);
		virtual void setInput(size_t input, const BVec &bvec);

		virtual Bit getOutputBit(size_t output);
		virtual BVec getOutputBVec(size_t output);
	};

/// @}

}

