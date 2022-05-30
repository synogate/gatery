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

#include "../../utils/Range.h"


#include <external/magic_enum.hpp>

#include <chrono>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>


namespace gtry::dbg {

std::string JsonSerializer::serializeAllLogMessages(const std::list<LogMessage> &logMessages)
{
	std::stringstream json;
	json << "{ \"operation\":\"addLogMessages\", \"data\": [\n";
	
	bool first = true;
	for (const auto &msg : logMessages) {
		if (!first) json << ",\n"; first = false;
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
			else if (std::holds_alternative<hlim::Subnet>(part))
				json << "{\"type\": \"subnet\", \"nodes\": []}\n";
		}

		json << "]}";
	}

	json << "]}\n";

	std::fstream jsonFile("testLog.json", std::fstream::out);
	jsonFile << json.str();

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

std::string JsonSerializer::serializeAllNodes(const hlim::Circuit &circuit)
{
	std::stringstream json;
	{
		bool firstElement = true;
		json << "{ \"operation\":\"addNodes\", \"data\": [\n";
		for (const auto &node : circuit.getNodes()) {
			if (!firstElement) json << ",\n"; firstElement = false;
			json << "{\n    \"id\": " << node->getId() << ",\n    \"name\": \"" << node->getName() << "\",\n    \"type\": \"" << node->getTypeName() << "\",\n"
					<< "    \"group\": " << node->getGroup()->getId() << ",\n"
					<< "    \"stack_trace\": ";
			
			serializeStackTrace(json, node->getStackTrace());
			json << ",\n";

			if (auto *rewire = dynamic_cast<const hlim::Node_Rewire*>(node.get())) {
				json << R"(    "rewireOp": [)";
				bool firstElement = true;
				for (const auto &r : rewire->getOp().ranges) {
					if (!firstElement) json << ",\n"; firstElement = false;
					json << "        {\n"
							<< "            \"subwidth\": " << r.subwidth << ",\n"
							<< "            \"source\": \"" << magic_enum::enum_name(r.source) << "\",\n"
							<< "            \"inputIdx\": " << r.inputIdx << ",\n"
							<< "            \"inputOffset\": " << r.inputOffset << "\n"
							<< "        }";
				}
				json << "],\n";
			}

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
		json << "{ \"addr\": " << (size_t)frame.address() << "}";
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
	std::unique_lock<std::mutex> conditionLock(m_sessionListMutex);
	while (m_sessions.empty())
		m_newSession.wait(conditionLock);
	std::cout << "Debugger connected" << std::endl;
}

void WebSocksInterface::pushGraph()
{
	std::unique_lock<std::mutex> conditionLock(m_conditionMutex);
	m_graphAccessible.store(true);
	m_wakeSessions.notify_all();
	m_interfaceSleeping.wait(conditionLock);
	m_graphAccessible.store(false);
}

void WebSocksInterface::stopInDebugger()
{
	{
		std::unique_lock<std::mutex> conditionLock(m_conditionMutex);
		m_graphAccessible.store(true);
		m_wakeSessions.notify_all();
	}
    while (true) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1s);
    }

}

void WebSocksInterface::log(LogMessage msg)
{
	m_logMessages.push_back(std::move(msg));
}


WebSocksInterface::WebSocksInterface(unsigned port)
{
	m_circuit = &DesignScope::get()->getCircuit();

	m_acceptorThread = std::thread(std::bind(&WebSocksInterface::acceptorLoop, this, port));
	m_sessionThread = std::thread(std::bind(&WebSocksInterface::sessionLoop, this));
}

WebSocksInterface::~WebSocksInterface()
{
	m_shutdown.store(true);
	m_acceptorThread.join();
	m_interfaceSleeping.notify_all();
	m_sessionThread.join();
}

void WebSocksInterface::acceptorLoop(unsigned port)
{
	try {
		auto const address = net::ip::make_address("0.0.0.0");

		tcp::acceptor acceptor{m_ioc, {address, port}};
		while (!m_shutdown.load()) {
			tcp::socket socket{m_ioc};

			acceptor.accept(socket);
			acceptSession(std::move(socket));
		}
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}
}

void WebSocksInterface::acceptSession(tcp::socket socket)
{
	try {
		beast::flat_buffer buffer;
		beast::http::request_parser<beast::http::empty_body> req;
		read(socket, buffer, req);
		websocket::stream<tcp::socket> ws{std::move(socket)};
		ws.accept(req.get());
		{
			std::lock_guard<std::mutex> lock(m_sessionListMutex);
			m_sessions.push_back(std::move(ws));
			m_newSession.notify_all();

			std::unique_lock<std::mutex> conditionLock(m_conditionMutex, std::defer_lock);
			if (conditionLock.try_lock() && m_graphAccessible.load()) {
				JsonSerializer serializer;

				std::string json = R"({"operation": "clearAll"})";
				std::cout << "Sending clear all" << std::endl;
				m_sessions.back().text(true);
				m_sessions.back().write(net::buffer(json));

				json = serializer.serializeAllGroups(*m_circuit);
				std::cout << "Sending groups" << std::endl;
				m_sessions.back().text(true);
				m_sessions.back().write(net::buffer(json));

				json = serializer.serializeAllNodes(*m_circuit);
				std::cout << "Sending graph" << std::endl;
				m_sessions.back().text(true);
				m_sessions.back().write(net::buffer(json));

				json = serializer.serializeAllLogMessages(m_logMessages);
				std::cout << "Sending messages" << std::endl;
				m_sessions.back().text(true);
				m_sessions.back().write(net::buffer(json));
			}
		}
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

void WebSocksInterface::sessionLoop()
{
	std::unique_lock<std::mutex> conditionLock(m_conditionMutex);
	while (!m_shutdown.load()) {
		m_wakeSessions.wait(conditionLock);
		if (m_graphAccessible.load()) {
			JsonSerializer serializer;
			std::string jsonClear = R"({"operation": "clearAll"})";
			std::string jsonGroup = serializer.serializeAllGroups(*m_circuit);
			std::string jsonNodes = serializer.serializeAllNodes(*m_circuit);
			std::string jsonMessages = serializer.serializeAllLogMessages(m_logMessages);
			std::lock_guard<std::mutex> lock(m_sessionListMutex);
			for (auto &session : m_sessions) {
				try {
					std::cout << "Resending clear all" << std::endl;
					m_sessions.back().text(true);
					m_sessions.back().write(net::buffer(jsonClear));
					
					std::cout << "Resending group" << std::endl;
					m_sessions.back().text(true);
					m_sessions.back().write(net::buffer(jsonGroup));

					std::cout << "Resending graph" << std::endl;
					m_sessions.back().text(true);
					m_sessions.back().write(net::buffer(jsonNodes));

					std::cout << "Resending messages" << std::endl;
					m_sessions.back().text(true);
					m_sessions.back().write(net::buffer(jsonMessages));
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
		m_interfaceSleeping.notify_all();
	}
}

}