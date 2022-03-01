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

#include "BVec.h"
#include "UInt.h"
#include "SInt.h"
#include "Bit.h"

#include "../simulation/BitVectorState.h"
#include "../hlim/GraphTools.h"

#include <vector>
#include <memory>
#include <map>
#include <set>

namespace gtry::hlim {
	class Node_Signal;
}

namespace gtry {

struct NodeGroupIO {
	std::map<std::string, UInt> inputUInts;
	std::map<std::string, Bit> inputBits;
	std::map<std::string, UInt> outputUInts;
	std::map<std::string, Bit> outputBits;

	NodeGroupIO(hlim::NodeGroup *nodeGroup);
};

class NodeGroupSurgeryHelper {
	public:
		NodeGroupSurgeryHelper(hlim::NodeGroup *nodeGroup);

		bool containsSignal(std::string_view name);

		UInt hookUIntBefore(std::string_view name);
		UInt hookUIntAfter(std::string_view name);
		Bit hookBitBefore(std::string_view name);
		Bit hookBitAfter(std::string_view name);

		Bit getBit(std::string_view name);
		UInt getUInt(std::string_view name);

		const std::vector<hlim::Node_Signal*> &getAllSignals(std::string_view name);
	protected:
		std::vector<hlim::Node_Signal*> m_empty;
		std::map<std::string, std::vector<hlim::Node_Signal*>, std::less<>> m_namedSignalNodes;
};


UInt hookUIntBefore(hlim::NodePort input);
UInt hookUIntAfter(hlim::NodePort output);
Bit hookBitBefore(hlim::NodePort input);
Bit hookBitAfter(hlim::NodePort output);

UInt getUIntBefore(hlim::NodePort input);
UInt getUIntBefore(hlim::NodePort input, UInt defaultValue);
Bit getBitBefore(hlim::NodePort input);
Bit getBitBefore(hlim::NodePort input, Bit defaultValue);


UInt hookUIntBefore(hlim::Node_Signal *signal);
UInt hookUIntAfter(hlim::Node_Signal *signal);
Bit hookBitBefore(hlim::Node_Signal *signal);
Bit hookBitAfter(hlim::Node_Signal *signal);


sim::DefaultBitVectorState evaluateStatically(hlim::NodePort output);
sim::DefaultBitVectorState evaluateStatically(const ElementarySignal &signal);

hlim::Node_Pin *findInputPin(ElementarySignal &sig);
hlim::Node_Pin *findOutputPin(ElementarySignal &sig);

inline const hlim::Node_Pin *findInputPin(const ElementarySignal &sig) { return findInputPin((ElementarySignal &)sig); }
inline const hlim::Node_Pin *findOutputPin(const ElementarySignal &sig) { return findOutputPin((ElementarySignal &)sig); }


}
