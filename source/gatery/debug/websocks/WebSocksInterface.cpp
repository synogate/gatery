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

#include "gatery/debug/helpers/JsonSerialization.h"

#include "../../frontend/DesignScope.h"
#include "../../frontend/Scope.h"

#include "../../hlim/Circuit.h"
#include "../../hlim/Node.h"
#include "../../hlim/Clock.h"
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

std::string concatenateLogMessages(const std::span<std::string> &logMessages)
{
	std::stringstream json;
	json << "{ \"operation\":\"addLogMessages\", \"data\": [\n";
	
	bool first = true;
	for (const auto &msg : logMessages) {
		if (!first) json << ",\n"; 
		first = false;
		json << msg;
	}
	json << "]}\n";

	return json.str();
}

void WebSocksInterface::create(unsigned port)
{
	instance.reset(nullptr); // Close previous first
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
	m_logMessages.push_back(json::serializeLogMessage(std::move(msg)));
}

void WebSocksInterface::changeState(State state, hlim::Circuit* circuit)
{
	DebugInterface::changeState(state, circuit);

	for (auto &session : m_sessions)
		session.stateDirty = true;

	operate();
}


WebSocksInterface::WebSocksInterface(unsigned port) : m_acceptor(m_ioc, {net::ip::make_address("0.0.0.0"), (unsigned short) port})
{
	m_circuit = &DesignScope::get()->getCircuit();

	m_acceptor.set_option(tcp::socket::reuse_address(true));

	waitAccept();
}

WebSocksInterface::~WebSocksInterface()
{
	m_acceptor.close();

	while (!m_sessions.empty())
		closeSession(m_sessions.front());

	m_ioc.run();
}

std::string WebSocksInterface::howToReachLog()
{
	return "Open the web debugger in a browser";
}

void WebSocksInterface::waitAccept()
{
	m_acceptor.async_accept([this](beast::error_code ec, tcp::socket socket){
		if (ec)
			return;

		acceptSession(std::move(socket));
		waitAccept();
	});
}

void WebSocksInterface::acceptSession(tcp::socket socket)
{
	m_sessions.emplace_back(std::move(socket));
	auto &session = m_sessions.back();

	boost::beast::http::async_read(session.websockStream.next_layer(), session.buffer, session.req, [&session, this](beast::error_code ec, std::size_t bytes_transferred){
		if (ec) {
			std::cout << "Websocket connection failed to connect, could not read handshake: " << ec.message() << std::endl;
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

void WebSocksInterface::awaitRequest(Session &session)
{
	session.websockStream.async_read(session.buffer, [this, &session](beast::error_code ec, std::size_t bytes_written){
		if (ec) {
			if (ec != boost::system::errc::operation_canceled)
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
	if (session.closing) return;
	session.closing = true;

	for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it)
		if (&*it == &session) {
			m_sessions.erase(it);
			return;
		}
	throw std::runtime_error("Invalid tcp session");
}

void WebSocksInterface::operate()
{
	m_ioc.poll();

	for (auto &session : m_sessions) {
		if (!session.ready) continue;
		try {
			if (session.graphDirty) {
				std::string jsonClear = R"({"operation": "clearAll"})";
				std::stringstream jsonGroup;
				jsonGroup << "{ \"operation\":\"addGroups\", \"data\": [\n";
				if (m_circuit->getRootNodeGroup() != nullptr)
					json::serializeGroup(jsonGroup, m_circuit->getRootNodeGroup(), true);
				jsonGroup << "]}\n";

				std::stringstream jsonNodes;
				jsonNodes << "{ \"operation\":\"addNodes\", \"data\": [\n";
				json::serializeAllNodes(jsonNodes, *m_circuit);
				jsonNodes << "\n]}\n";

				std::cout << "Resending clear all" << std::endl;
				session.websockStream.text(true);
				session.websockStream.write(net::buffer(jsonClear));
				
				std::cout << "Resending group" << std::endl;
				session.websockStream.text(true);
				session.websockStream.write(net::buffer(jsonGroup.str()));

				std::cout << "Resending graph" << std::endl;
				session.websockStream.text(true);
				session.websockStream.write(net::buffer(jsonNodes.str()));
				session.messagesSend = 0; // caused by clear-all

				session.graphDirty = false;
			}
			if (session.messagesSend < m_logMessages.size()) {
				std::string jsonMessages = concatenateLogMessages(std::span<std::string>(m_logMessages).subspan(session.messagesSend));

				std::cout << "Resending messages" << std::endl;
				session.websockStream.text(true);
				session.websockStream.write(net::buffer(jsonMessages));

				session.messagesSend = m_logMessages.size();
			}
			if (session.stateDirty) {
				std::stringstream jsonState;
				jsonState <<R"({"operation": "changeMode", "mode":")" << magic_enum::enum_name(m_state) << R"("})";

				std::cout << "Resending state" << std::endl;
				session.websockStream.text(true);
				session.websockStream.write(net::buffer(jsonState.str()));

				session.stateDirty = false;
			}
			for (const auto &vis : m_visualizations) {
				auto it = session.visualizationStates.find(vis.first);
				if (it == session.visualizationStates.end()) {
					std::stringstream json;
					json << R"({"operation": "newVisualization", "data": {"id": ")" << vis.first << R"(", "title": ")" << vis.second.title << R"(" }})";

					session.websockStream.text(true);
					session.websockStream.write(net::buffer(json.str()));
					it = session.visualizationStates.insert({vis.first, 0ull}).first;
				}
				if (it->second < vis.second.contentVersion) {
					session.websockStream.text(true);
					session.websockStream.write(net::buffer(vis.second.content));
					it->second = vis.second.contentVersion;
				}
			}
			while (session.areaVisStates.size() < m_areaVisualizations.size()) {
				const auto id = session.areaVisStates.size();
				const auto &vis = m_areaVisualizations[id];
				std::stringstream json;
				json << R"({"operation": "newAreaVisualization", "data": {"id": )" << id << R"(, "areaId": )" << vis.nodeGroupId << R"(, "width": )" << vis.width << R"(, "height": )" << vis.height << R"( }})";

				session.websockStream.text(true);
				session.websockStream.write(net::buffer(json.str()));
				
				session.areaVisStates.push_back(0);
			}
			for (auto i : utils::Range(m_areaVisualizations.size())) {
				if (session.areaVisStates[i] < m_areaVisualizations[i].contentVersion) {
					session.websockStream.text(true);
					session.websockStream.write(net::buffer(m_areaVisualizations[i].content));
					session.areaVisStates[i] = m_areaVisualizations[i].contentVersion;
				}
			}
		} catch(std::exception const& e) {
			std::cerr << "Error: " << e.what() << std::endl;
			closeSession(session);
			return; // teh closeSession breaks the for loop
		}
	}

}

void WebSocksInterface::createVisualization(const std::string &id, const std::string &title)
{
	auto &vis = m_visualizations[id];
	vis.title = title;
}

void WebSocksInterface::updateVisualization(const std::string &id, const std::string &imageData)
{
	std::stringstream json;
	json << R"({"operation": "visData", "data": {"id": ")" << id << R"(", "imageData": ")";
	for (auto c : imageData)
		switch (c) {
			case '"': json << '\\' << '"'; break;
			case '\n': json << '\\' << 'n'; break;
			case '\t': json << '\\' << 't'; break;
			default: json << c;
		}

	json << R"(" }})";


	auto &vis = m_visualizations[id];
	vis.content = json.str();
	vis.contentVersion++;
}


size_t WebSocksInterface::createAreaVisualization(unsigned width, unsigned height)
{
	m_areaVisualizations.push_back({.width = width, .height = height, .nodeGroupId = GroupScope::get()->nodeGroup()->getId() });
	return m_areaVisualizations.size()-1;
}

void WebSocksInterface::updateAreaVisualization(size_t id, const std::string content)
{
	std::stringstream json;
	json << R"({"operation": "visAreaData", "data": {"id": )" << id << R"(, "content": ")";
	for (auto c : content)
		switch (c) {
			case '"': json << '\\' << '"'; break;
			case '\n': json << '\\' << 'n'; break;
			case '\t': json << '\\' << 't'; break;
			default: json << c;
		}

	json << R"(" }})";
	m_areaVisualizations[id].content = json.str();
	m_areaVisualizations[id].contentVersion++;
}



}
