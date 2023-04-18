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
#include "gatery/pch.h"

#include "VHDLSignalDeclaration.h"

#include "../../hlim/NodePort.h"
#include "../../hlim/Node.h"

#include "../../hlim/coreNodes/Node_MultiDriver.h"
#include "../../hlim/supportNodes/Node_External.h"

namespace gtry::vhdl {

VHDLDataType chooseDataTypeFromOutput(const hlim::NodePort &np)
{
	VHDLDataType dataType;
	if (hlim::outputIsBool(np))
		dataType = VHDLDataType::STD_LOGIC;
	else {
		// This is a bit of a hack to get bidir signals working without type conversion.
		if (dynamic_cast<hlim::Node_MultiDriver*>(np.node))
			dataType = VHDLDataType::STD_LOGIC_VECTOR;
		else if (auto *extNode = dynamic_cast<hlim::Node_External*>(np.node)) {
			if (extNode->outputIsBidir(np.port))
				dataType = VHDLDataType::STD_LOGIC_VECTOR;
			else
				dataType = VHDLDataType::UNSIGNED;
		} else
			dataType = VHDLDataType::UNSIGNED;
	}

	return dataType;
}

}