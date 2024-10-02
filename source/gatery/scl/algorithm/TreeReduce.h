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

#include <gatery/frontend.h>

#include <gatery/utils/BitManipulation.h>
#include <gatery/utils/Range.h>
#include <gatery/scl/ShiftReg.h>

#include <vector>


namespace gtry::scl
{
	template<typename Signal, class Functor>
	Signal treeReduceImpl(const std::vector<Signal> &input, size_t depth, size_t registersRemaining, size_t registerInterval, Functor functor) {
		
		if (input.size() == 1)
			return delay(input.front(), registersRemaining);
		
		bool insertReg = false;
		if (registerInterval > 0)
			insertReg = registersRemaining > 0 && (depth % registerInterval) == 0;

		if (insertReg)
			registersRemaining--;
		
		Signal left = treeReduceImpl(std::vector<Signal>(input.begin(), input.begin()+input.size()/2),
									depth+1,
									registersRemaining,
									registerInterval,
									functor);
									
		Signal right = treeReduceImpl(std::vector<Signal>(input.begin()+input.size()/2, input.end()),
									depth+1,
									registersRemaining,
									registerInterval,
									functor);

		Signal res = functor(left, right);
		return delay(res, insertReg?1:0);
	}
	
	template<typename ContainerType, class Functor>
	typename ContainerType::value_type treeReduce(const ContainerType &input, size_t numRegisterSteps, Functor functor) {
		std::vector<typename ContainerType::value_type> inputValues(input.begin(), input.end());
		
		size_t treeDepth = utils::Log2C(inputValues.size());
		
		return treeReduceImpl(inputValues, 0, numRegisterSteps, numRegisterSteps>0?(treeDepth+numRegisterSteps-1) / numRegisterSteps:0, functor);		
	}

}

