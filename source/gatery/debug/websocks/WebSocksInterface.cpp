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
#include "WebSocksInterface.h"

#include "../../frontend/DesignScope.h"

#include "../../hlim/Circuit.h"
#include "../../hlim/Node.h"
#include "../../hlim/NodeGroup.h"
#include "../../hlim/coreNodes/Node_Rewire.h"
#include "../../hlim/coreNodes/Node_Signal.h"
#include "../../hlim/coreNodes/Node_Pin.h"
#include "../../hlim/coreNodes/Node_Constant.h"
#include "../../hlim/coreNodes/Node_Compare.h"
#include "../../hlim/coreNodes/Node_Arithmetic.h"
#include "../../hlim/coreNodes/Node_Logic.h"
#include "../../hlim/coreNodes/Node_Multiplexer.h"
#include "../../hlim/coreNodes/Node_Register.h"

#include "../../utils/Range.h"

#include "../../utils/StackTrace.h"

#include <boost/json.hpp>

#include <external/magic_enum.hpp>

#include <thread>
#include <chrono>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>


namespace gtry::dbg {

std::string JsonSerializer::serializeAllLogMessages(const std::span<std::string> &logMessages)
{
	std::stringstream json;
	json << "{ \"operation\":\"addLogMessages\", \"data\": [\n";
	
	bool first = true;
	for (const auto &msg : logMessages) {
		if (!first) json << ",\n"; first = false;
		json << msg;
	}
	json << "]}\n";
/*
	std::fstream jsonFile("testLog.json", std::fstream::out);
	jsonFile << json.str();
*/
	return json.str();
}

std::string JsonSerializer::serializeLogMessage(const LogMessage &msg)
{
	std::stringstream json;
	json 
		<< "{ \"severity\": \"" << magic_enum::enum_name(msg.severity()) << "\",\n"
		<< "\"source\": \"" << magic_enum::enum_name(msg.source()) << "\",\n"
		<< "\"message_parts\": [\n";
	
	bool firstPart = true;
	for (const auto &part : msg.parts()) {
		if (!firstPart) json << ",\n"; firstPart = false;
		
		if (std::holds_alternative<const char*>(part))
			json << "{\"type\": \"string\", \"data\": \"" << std::get<const char*>(part) << "\"}\n";
		else if (std::holds_alternative<std::string>(part))
			json << "{\"type\": \"string\", \"data\": \"" << std::get<std::string>(part) << "\"}\n";
		else if (std::holds_alternative<const hlim::BaseNode*>(part))
			json << "{\"type\": \"node\", \"id\": " << std::get<const hlim::BaseNode*>(part)->getId() << "}\n";
		else if (std::holds_alternative<const hlim::NodeGroup*>(part))
			json << "{\"type\": \"group\", \"id\": " << std::get<const hlim::NodeGroup*>(part)->getId() << "}\n";
		else if (std::holds_alternative<const hlim::Subnet*>(part))
			json << "{\"type\": \"subnet\", \"nodes\": []}\n";
	}

	json << "]}";
	return json.str();
}

std::string JsonSerializer::serializeAllGroups(const hlim::Circuit &circuit)
{
	std::stringstream json;
	json << "{ \"operation\":\"addGroups\", \"data\": [\n";

	bool first = true;

	std::function<void(const hlim::NodeGroup*)> serializeGroup;
	serializeGroup = [&](const hlim::NodeGroup* group) {
		if (!first) json << ",\n";
		first = false;

		json << "{ \"id\":" << group->getId() << ", \"name\":\"" << group->getName() << "\", \"instanceName\":\""<<group->getInstanceName()<<"\",";

		if (group->getParent() != nullptr)
			json << "\"parent\": " << group->getParent()->getId() << ", ";              

		json << "    \"stack_trace\": ";		
		serializeStackTrace(json, group->getStackTrace());
		json << ",\n";


		json << "\"children\":[";
	
		for (auto i : utils::Range(group->getChildren().size())) {
			if (i > 0) json << ", ";
			json << group->getChildren()[i]->getId();
		}

		json << "]}";
		for (auto i : utils::Range(group->getChildren().size()))
			serializeGroup(group->getChildren()[i].get());
	};
	if (circuit.getRootNodeGroup() != nullptr)
		serializeGroup(circuit.getRootNodeGroup());
	json << "]}\n";

	return json.str();
}

namespace {

std::ostream &operator<<(std::ostream &json, const hlim::Node_Rewire *rewire)
{
	json 
		<< "    \"type\": \"rewire\",\n"
		<< "    \"meta\": {"
		<< "        \"rewireOp\": [";
	bool firstElement = true;
	for (const auto &r : rewire->getOp().ranges) {
		if (!firstElement) json << ",\n"; firstElement = false;
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

}

std::string JsonSerializer::serializeAllNodes(const hlim::Circuit &circuit)
{
	std::stringstream json;
	{
		bool firstElement = true;
		json << "{ \"operation\":\"addNodes\", \"data\": [\n";
		for (const auto &node : circuit.getNodes()) {
			if (!firstElement) json << ",\n"; firstElement = false;
			json << "{\n    \"id\": " << node->getId() << ",\n    \"name\": \"" << node->getName() << "\",\n"
					<< "    \"group\": " << node->getGroup()->getId() << ",\n"
					<< "    \"stack_trace\": ";
			
			serializeStackTrace(json, node->getStackTrace());
			json << ",\n";

			if (auto *mux = dynamic_cast<const hlim::Node_Multiplexer*>(node.get()))
				json << mux;
			else
			if (auto *rewire = dynamic_cast<const hlim::Node_Rewire*>(node.get()))
				json << rewire;
			else
			if (auto *signal = dynamic_cast<const hlim::Node_Signal*>(node.get()))
				json << signal;
			else
			if (auto *ioPin = dynamic_cast<const hlim::Node_Pin*>(node.get()))
				json << ioPin;
			else
			if (auto *reg = dynamic_cast<const hlim::Node_Register*>(node.get()))
				json << reg;
			else
			if (auto *arith = dynamic_cast<const hlim::Node_Arithmetic*>(node.get()))
				json << arith;
			else
			if (auto *compare = dynamic_cast<const hlim::Node_Compare*>(node.get()))
				json << compare;
			else
			if (auto *logic = dynamic_cast<const hlim::Node_Logic*>(node.get()))
				json << logic;
			else
			if (auto *constant = dynamic_cast<const hlim::Node_Constant*>(node.get()))
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
						<< "            \"interpretation\": \"" << magic_enum::enum_name(node->getOutputConnectionType(i).interpretation) << "\",\n"
						<< "            \"type\": \"" << magic_enum::enum_name(node->getOutputType(i)) << "\",\n"
						<< "            \"consumers\": [\n";
				bool firstElement = true;
				for (const auto &np : node->getDirectlyDriven(i)) {
					if (!firstElement) json << ",\n"; firstElement = false;
					json << "                {\"node\": " << (np.node != nullptr?np.node->getId():~0ull) << ", \"port\": " << np.port << '}';
				}
				json << "\n            ]\n"
					<< "        }";
			}
			json << "\n    ]\n";
			json << "}";
		}
		json << "\n]}\n";
	}
	
	std::fstream jsonFile("testNodes.json", std::fstream::out);
	jsonFile << json.str();
	
	return json.str();
}

void JsonSerializer::serializeStackTrace(std::ostream &json, const utils::StackTrace &trace)
{
	json << "[";
	bool first = true;
	for (const auto &frame : trace.getTrace()) {
		if (!first) json << ",\n"; first = false;
		//json << "{ \"addr\": " << (size_t)frame.address() << ", \"file\": \"" << frame.source_file() << "\", \"line\": " << frame.source_line() << "}";
		//json << "{ \"addr\": " << (size_t)frame.address() << "}";
		json << (size_t)frame.address();
	}
	json << "]";
}



void WebSocksInterface::create(unsigned port)
{
	instance.reset(new WebSocksInterface(port));
}

void WebSocksInterface::awaitDebugger()
{
	std::cout << "Waiting for websocks debugger session" << std::endl;
	while (m_sessions.empty())
        m_ioc.run_one();
	std::cout << "Debugger connected" << std::endl;
}

void WebSocksInterface::pushGraph()
{
	for (auto &session : m_sessions)
		session.graphDirty = true;

	operate();
}

void WebSocksInterface::stopInDebugger()
{
    while (true) {
		operate();
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(200ms);
    }
}

void WebSocksInterface::log(LogMessage msg)
{
	JsonSerializer s;
	m_logMessages.push_back(s.serializeLogMessage(std::move(msg)));
}

void WebSocksInterface::changeState(State state)
{
	DebugInterface::changeState(state);

	for (auto &session : m_sessions)
		session.stateDirty = true;

	operate();
}


WebSocksInterface::WebSocksInterface(unsigned port) : m_acceptor(m_ioc, {net::ip::make_address("0.0.0.0"), (unsigned short) port})
{
	m_circuit = &DesignScope::get()->getCircuit();

	waitAccept();
}

WebSocksInterface::~WebSocksInterface()
{
	while (!m_sessions.empty())
		closeSession(m_sessions.front());
}

void WebSocksInterface::waitAccept()
{
	m_acceptor.async_accept([this](beast::error_code ec, tcp::socket socket){
		if (ec)
			throw std::runtime_error(std::string("Networking failure, cannot accept connections"));

		acceptSession(std::move(socket));
		waitAccept();
	});
}

void WebSocksInterface::awaitRequest(Session &session)
{
	session.websockStream.async_read(session.buffer, [this, &session](beast::error_code ec, std::size_t bytes_written){
		if (ec) {
			std::cerr << "An error occured with one of the websocks debugger connections, dropping connection!" << std::endl;
			closeSession(session);
			return;
		}
		// Handle request

		std::string responseStr;
		std::string requestStr = boost::beast::buffers_to_string(session.buffer.data());
		session.buffer.consume(requestStr.length());
		try {
			boost::json::object request = boost::json::parse(requestStr).as_object();
			boost::json::object response;

			auto op = request["operation"].as_string();
			if (op == "resolve_stacktrace") {
				response["operation"] = "request_response";
				response["handle"] = request["handle"];

				boost::json::array resolvedFrames;
				auto frames = request["stack_trace"].as_array();
				for (auto &frameAddr : frames) {
					boost::stacktrace::frame frame((boost::stacktrace::detail::native_frame_ptr_t) frameAddr.as_int64());
					boost::json::object resolvedFrame;
					resolvedFrame["addr"] = (std::uint64_t) frame.address();
					resolvedFrame["name"] = frame.name();
					resolvedFrame["file"] = frame.source_file();
					resolvedFrame["line"] = frame.source_line();
					resolvedFrames.push_back(resolvedFrame);
				}

				response["data"] = resolvedFrames;

				responseStr = boost::json::serialize(response);

			} else
				throw std::runtime_error(std::string("Unknown op: ") + op.c_str());

		} catch (const std::exception &e) {
			std::cerr << "Error while parsing json message from websocks debugger: " << e.what() << " Dropping connection!" << std::endl
						<< "Message: "<< std::endl << requestStr << std::endl;
			closeSession(session);
			return;
		}

		try {
			std::cout << "Sending " << responseStr << std::endl;
			session.websockStream.text(true);
			session.websockStream.write(net::buffer(responseStr));
		}
		catch(beast::system_error const& se)
		{
			// This indicates that the session was closed
			if(se.code() != websocket::error::closed)
				std::cerr << "beast error: " << se.code().message() << std::endl;
		}
		catch(std::exception const& e)
		{
			std::cerr << "Error: " << e.what() << std::endl;
		}

		awaitRequest(session);
	});
}

void WebSocksInterface::closeSession(Session &session)
{
	for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it)
		if (&*it == &session) {
			m_sessions.erase(it);
			return;
		}
	throw std::runtime_error("Invalid tcp session");
}

void WebSocksInterface::acceptSession(tcp::socket socket)
{
	m_sessions.emplace_back(std::move(socket));
	auto &session = m_sessions.back();

	async_read(session.websockStream.next_layer(), session.buffer, session.req, [&session, this](beast::error_code ec, std::size_t bytes_transferred){
		if (ec) {
			std::cout << "Websocket connection failed to connect: " << ec.message() << std::endl;
			closeSession(session);
			return;
		} 
		session.websockStream.async_accept(session.req.get(), [&session, this](beast::error_code ec){
			if (ec) {
				std::cout << "Websocket connection failed to connect: " << ec.message() << std::endl;
				closeSession(session);
				return;
			}
			awaitRequest(session);
			session.ready = true;
		});
	});
}

void WebSocksInterface::operate()
{
	m_ioc.poll();

	JsonSerializer serializer;

	for (auto &session : m_sessions) {
		if (!session.ready) continue;
		if (session.graphDirty) {
			std::string jsonClear = R"({"operation": "clearAll"})";
			std::string jsonGroup = serializer.serializeAllGroups(*m_circuit);
			std::string jsonNodes = serializer.serializeAllNodes(*m_circuit);

			try {
				std::cout << "Resending clear all" << std::endl;
				session.websockStream.text(true);
				session.websockStream.write(net::buffer(jsonClear));
				
				std::cout << "Resending group" << std::endl;
				session.websockStream.text(true);
				session.websockStream.write(net::buffer(jsonGroup));

				std::cout << "Resending graph" << std::endl;
				session.websockStream.text(true);
				session.websockStream.write(net::buffer(jsonNodes));
			}
			catch(beast::system_error const& se)
			{
				// This indicates that the session was closed
				if(se.code() != websocket::error::closed)
					std::cerr << "beast error: " << se.code().message() << std::endl;
			}
			catch(std::exception const& e)
			{
				std::cerr << "Error: " << e.what() << std::endl;
			}
			session.messagesSend = 0; // caused by clear-all

			session.graphDirty = false;
		}
		if (session.messagesSend < m_logMessages.size()) {
			std::string jsonMessages = serializer.serializeAllLogMessages(std::span<std::string>(m_logMessages.begin() + session.messagesSend, m_logMessages.size()-session.messagesSend));

			try {
				std::cout << "Resending messages" << std::endl;
				session.websockStream.text(true);
				session.websockStream.write(net::buffer(jsonMessages));
			}
			catch(beast::system_error const& se)
			{
				// This indicates that the session was closed
				if(se.code() != websocket::error::closed)
					std::cerr << "beast error: " << se.code().message() << std::endl;
			}
			catch(std::exception const& e)
			{
				std::cerr << "Error: " << e.what() << std::endl;
			}

			session.messagesSend = m_logMessages.size();
		}
		if (session.stateDirty) {
			std::stringstream jsonState;
			jsonState <<R"({"operation": "changeMode", "mode":")" << magic_enum::enum_name(m_state) << R"("})";

			try {
				std::cout << "Resending state" << std::endl;
				session.websockStream.text(true);
				session.websockStream.write(net::buffer(jsonState.str()));
			}
			catch(beast::system_error const& se)
			{
				// This indicates that the session was closed
				if(se.code() != websocket::error::closed)
					std::cerr << "beast error: " << se.code().message() << std::endl;
			}
			catch(std::exception const& e)
			{
				std::cerr << "Error: " << e.what() << std::endl;
			}

			session.stateDirty = false;
		}		
	}

}

void WebSocksInterface::createVisualization(const std::string &id, const std::string &title)
{
	std::stringstream json;
	json << R"({"operation": "newVisualization", "data": {"id": ")" << id << R"(", "title": ")" << title << R"(" }})";

	for (auto &session : m_sessions) {
		if (!session.ready) continue;
		try {
			session.websockStream.text(true);
			session.websockStream.write(net::buffer(json.str()));
		}
		catch(beast::system_error const& se)
		{
			// This indicates that the session was closed
			if(se.code() != websocket::error::closed)
				std::cerr << "beast error: " << se.code().message() << std::endl;
		}
		catch(std::exception const& e)
		{
			std::cerr << "Error: " << e.what() << std::endl;
		}
	}
}

void WebSocksInterface::updateVisualization(const std::string &id, const std::string &imageData)
{
	std::stringstream json;
	json << R"({"operation": "visData", "data": {"id": ")" << id << R"(", "imageData": ")" << imageData << R"(" }})";

	for (auto &session : m_sessions) {
		if (!session.ready) continue;

		try {
			session.websockStream.text(true);
			session.websockStream.write(net::buffer(json.str()));
		}
		catch(beast::system_error const& se)
		{
			// This indicates that the session was closed
			if(se.code() != websocket::error::closed)
				std::cerr << "beast error: " << se.code().message() << std::endl;
		}
		catch(std::exception const& e)
		{
			std::cerr << "Error: " << e.what() << std::endl;
		}
	}
}




}