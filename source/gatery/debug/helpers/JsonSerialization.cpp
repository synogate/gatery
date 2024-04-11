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

#include "gatery/pch.h"

#include "JsonSerialization.h"

#include "../DebugInterface.h"

#include "../../frontend/DesignScope.h"
#include "../../frontend/Scope.h"

#include "../../hlim/Circuit.h"
#include "../../hlim/Node.h"
#include "../../hlim/Clock.h"
#include "../../hlim/NodeGroup.h"
#include "../../hlim/Subnet.h"
#include "../../hlim/coreNodes/Node_Rewire.h"
#include "../../hlim/coreNodes/Node_Signal.h"
#include "../../hlim/coreNodes/Node_Pin.h"
#include "../../hlim/coreNodes/Node_Constant.h"
#include "../../hlim/coreNodes/Node_Compare.h"
#include "../../hlim/coreNodes/Node_Arithmetic.h"
#include "../../hlim/coreNodes/Node_Logic.h"
#include "../../hlim/coreNodes/Node_Multiplexer.h"
#include "../../hlim/coreNodes/Node_Register.h"

#include "../../utils/StackTrace.h"

#include <external/magic_enum.hpp>

namespace gtry::dbg::json {

void serializeSubnet(std::ostream &json, const hlim::ConstSubnet &subnet)
{
	json << '[';
	bool first = true;
	for (auto *n : subnet) {
		if (first) first = false; else json << ',';

		json << n->getId();
	}
	json << ']';
}


void serializeLogMessage(std::ostream &json, const LogMessage &msg)
{
	json 
		<< "{ \"severity\": \"" << magic_enum::enum_name(msg.severity()) << "\",\n"
		<< "\"source\": \"" << magic_enum::enum_name(msg.source()) << "\",\n"
		<< "\"anchor\": " << (msg.anchor()?msg.anchor()->getId():~0ull) << ",\n"
		<< "\"message_parts\": [\n";
	
	bool firstPart = true;
	for (const auto &part : msg.parts()) {
		if (!firstPart) json << ",\n";
		firstPart = false;
		
		if (std::holds_alternative<const char*>(part))
			json << "{\"type\": \"string\", \"data\": \"" << std::get<const char*>(part) << "\"}\n";
		else if (std::holds_alternative<std::string>(part))
			json << "{\"type\": \"string\", \"data\": \"" << std::get<std::string>(part) << "\"}\n";
		else if (std::holds_alternative<const hlim::BaseNode*>(part))
			json << "{\"type\": \"node\", \"id\": " << std::get<const hlim::BaseNode*>(part)->getId() << "}\n";
		else if (std::holds_alternative<const hlim::NodeGroup*>(part))
			json << "{\"type\": \"group\", \"id\": " << std::get<const hlim::NodeGroup*>(part)->getId() << "}\n";
		else if (std::holds_alternative<hlim::Subnet>(part)) {
			json << "{\"type\": \"subnet\", \"nodes\": ";
			serializeSubnet(json, std::get<hlim::Subnet>(part).asConst());
			json << "}\n";
		} else if (std::holds_alternative<hlim::NodePort>(part))
			json << "{\"type\": \"nodeport\", \"node\": " << std::get<hlim::NodePort>(part).node->getId() << ", \"port\": " << std::get<hlim::NodePort>(part).port << "}\n";
	}

	json << "]}";
}

std::string serializeLogMessage(const LogMessage &msg)
{
	std::stringstream json;
	serializeLogMessage(json, msg);
	return json.str();
}

void serializeGroup(std::ostream &json, const hlim::NodeGroup *group, bool reccurse)
{
	json << "{ \"id\":" << group->getId() << ", \"name\":\"" << group->getName() << "\", \"instanceName\":\""<<group->getInstanceName()<<"\",";

	if (group->getParent() != nullptr)
		json << "\"parent\": " << group->getParent()->getId() << ", ";              

	json << "    \"stack_trace\": ";		
	serializeStackTrace(json, group->getStackTrace(), false);
	json << ",\n";


	json << "\"children\":[";

	for (auto i : utils::Range(group->getChildren().size())) {
		if (i > 0) json << ", ";
		json << group->getChildren()[i]->getId();
	}

	json << "]}";
	if (reccurse) {
		for (auto i : utils::Range(group->getChildren().size())) {
			json << ',';
			serializeGroup(json, group->getChildren()[i].get(), true);
		}
	}
}

std::ostream &operator<<(std::ostream &json, const hlim::Node_Rewire *rewire)
{
	json 
		<< "    \"type\": \"rewire\",\n"
		<< "    \"meta\": {"
		<< "        \"rewireOp\": [";
	bool firstElement = true;
	for (const auto &r : rewire->getOp().ranges) {
		if (!firstElement) json << ",\n";
		firstElement = false;
		json 
			<< "        {\n"
			<< "            \"subwidth\": " << r.subwidth << ",\n"
			<< "            \"source\": \"" << magic_enum::enum_name(r.source) << "\",\n"
			<< "            \"inputIdx\": " << r.inputIdx << ",\n"
			<< "            \"inputOffset\": " << r.inputOffset << "\n"
			<< "        }";
	}
	json << "]},\n";

	return json;
}

std::ostream &operator<<(std::ostream &json, const hlim::Node_Signal *signal)
{
	json 
		<< "    \"type\": \"signal\",\n"
		<< "    \"meta\": {"
		<< "        \"name_inferred\": " << (signal->nameWasInferred()?"true":"false") << "\n"
		<< "    },\n";

	return json;
}

std::ostream &operator<<(std::ostream &json, const hlim::Node_Pin *ioPin)
{
	json 
		<< "    \"type\": \"io_pin\",\n"
		<< "    \"meta\": {"
		<< "        \"is_input_pin\": " << (ioPin->isInputPin()?"true":"false") << ",\n"
		<< "        \"is_output_pin\": " << (ioPin->isOutputPin()?"true":"false") << "\n"
		<< "    },\n";

	return json;
}

std::ostream &operator<<(std::ostream &json, const hlim::Node_Multiplexer *node)
{
	json 
		<< "    \"type\": \"mux\",\n"
		<< "    \"meta\": {"
		<< "    },\n";

	return json;
}

std::ostream &operator<<(std::ostream &json, const hlim::Node_Register *node)
{
	json 
		<< "    \"type\": \"register\",\n"
		<< "    \"meta\": {"
		<< "    },\n";

	return json;
}

std::ostream &operator<<(std::ostream &json, const hlim::Node_Constant *node)
{
	json 
		<< "    \"type\": \"constant\",\n"
		<< "    \"meta\": {"
		<< "    	\"value\": \"" << node->getValue() << "\",\n"
		<< "    	\"width\": " << node->getValue().size() << "\n"
		<< "    },\n";

	return json;
}


std::ostream &operator<<(std::ostream &json, const hlim::Node_Arithmetic *node)
{
	json 
		<< "    \"type\": \"arithmetic\",\n"
		<< "    \"meta\": {"
		<< "    	\"op\": \"" << magic_enum::enum_name(node->getOp()) << "\"\n"
		<< "    },\n";

	return json;
}


std::ostream &operator<<(std::ostream &json, const hlim::Node_Compare *node)
{
	json 
		<< "    \"type\": \"compare\",\n"
		<< "    \"meta\": {"
		<< "    	\"op\": \"" << magic_enum::enum_name(node->getOp()) << "\"\n"
		<< "    },\n";

	return json;
}

std::ostream &operator<<(std::ostream &json, const hlim::Node_Logic *node)
{
	json 
		<< "    \"type\": \"logic\",\n"
		<< "    \"meta\": {"
		<< "    	\"op\": \"" << magic_enum::enum_name(node->getOp()) << "\"\n"
		<< "    },\n";

	return json;
}

void serializeNode(std::ostream &json, const hlim::BaseNode *node)
{
	json << "{\n    \"id\": " << node->getId() << ",\n    \"name\": \"" << node->getName() << "\",\n"
			<< "    \"group\": " << node->getGroup()->getId() << ",\n"
			<< "    \"stack_trace\": ";
	
	serializeStackTrace(json, node->getStackTrace(), false);
	json << ",\n";

	if (auto *mux = dynamic_cast<const hlim::Node_Multiplexer*>(node))
		json << mux;
	else
	if (auto *rewire = dynamic_cast<const hlim::Node_Rewire*>(node))
		json << rewire;
	else
	if (auto *signal = dynamic_cast<const hlim::Node_Signal*>(node))
		json << signal;
	else
	if (auto *ioPin = dynamic_cast<const hlim::Node_Pin*>(node))
		json << ioPin;
	else
	if (auto *reg = dynamic_cast<const hlim::Node_Register*>(node))
		json << reg;
	else
	if (auto *arith = dynamic_cast<const hlim::Node_Arithmetic*>(node))
		json << arith;
	else
	if (auto *compare = dynamic_cast<const hlim::Node_Compare*>(node))
		json << compare;
	else
	if (auto *logic = dynamic_cast<const hlim::Node_Logic*>(node))
		json << logic;
	else
	if (auto *constant = dynamic_cast<const hlim::Node_Constant*>(node))
		json << constant;
	else
		json << " \"type\": \"" << node->getTypeName() << "\",\n";

	json << "    \"clocks\": [\n";
	for (auto i : utils::Range(node->getClocks().size())) {
		if (i != 0) json << ",\n";
		if (node->getClocks()[i] != nullptr)
			json << "            \"" << node->getClocks()[i]->getName() << '"';
		else
			json << "            \"\"";
	}
	json << "\n    ],\n";
	json << "    \"inputPorts\": [\n";
	for (auto i : utils::Range(node->getNumInputPorts())) {
		if (i != 0) json << ",\n";
		json << "        {\n";
		json << "            \"name\": \"" << node->getInputName(i) << "\",\n"
			<< "            \"node\": " << (node->getDriver(i).node != nullptr?node->getDriver(i).node->getId():~0ull) << ",\n"
			<< "            \"port\": " << node->getDriver(i).port << '\n'
			<< "        }";
	}
	json << "\n    ],\n";
	json << "    \"outputPorts\": [\n";
	for (auto i : utils::Range(node->getNumOutputPorts())) {
		if (i != 0) json << ",\n";
		json << "        {\n";
		json << "            \"name\": \"" << node->getOutputName(i) << "\",\n"
				<< "            \"width\": " << node->getOutputConnectionType(i).width << ",\n"
				<< "            \"interpretation\": \"" << magic_enum::enum_name(node->getOutputConnectionType(i).type) << "\",\n"
				<< "            \"type\": \"" << magic_enum::enum_name(node->getOutputType(i)) << "\",\n"
				<< "            \"consumers\": [\n";
		bool firstElement = true;
		for (const auto &np : node->getDirectlyDriven(i)) {
			if (!firstElement) json << ",\n"; 
			firstElement = false;
			json << "                {\"node\": " << (np.node != nullptr?np.node->getId():~0ull) << ", \"port\": " << np.port << '}';
		}
		json << "\n            ]\n"
			<< "        }";
	}
	json << "\n    ]\n";
	json << "}";
}

void serializeAllNodes(std::ostream &json, const hlim::Circuit &circuit)
{
	bool firstElement = true;
	for (const auto &node : circuit.getNodes()) {
		if (!firstElement) json << ",\n";
		firstElement = false;
		serializeNode(json, node.get());
	}
}

void serializeStackFrame(std::ostream &json, const void *ptr)
{
	boost::stacktrace::frame frame(ptr);
	json << "{ \"addr\": " << (size_t)frame.address() << ", \"file\": \"" << frame.source_file() << "\", \"line\": " << frame.source_line() << "}";
}

void serializeStackTrace(std::ostream &json, const utils::StackTrace &trace, bool resolved)
{
	json << "[";
	bool first = true;
	for (const auto &frame : trace.getTrace()) {
		if (!first) json << ",\n";
		first = false;

		if (resolved)
			serializeStackFrame(json, frame.address());
		else
			json << (size_t)frame.address();
	}
	json << "]";
}

}