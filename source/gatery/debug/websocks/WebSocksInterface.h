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

#include "../DebugInterface.h"

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace gtry::utils {
	class StackTrace;
}

namespace gtry::hlim {
	class Circuit;
}

namespace gtry::dbg {

class JsonSerializer
{
	public:
		std::string serializeAllLogMessages(const std::list<std::string> &logMessages);
		std::string serializeLogMessage(const LogMessage &logMessage);
		std::string serializeAllGroups(const hlim::Circuit &circuit);
		std::string serializeAllNodes(const hlim::Circuit &circuit);
	protected:
		void serializeStackTrace(std::ostream &json, const utils::StackTrace &trace);
};

class WebSocksInterface : public DebugInterface
{
	public:
		~WebSocksInterface();

		static void create(unsigned port = 1337);

		virtual void awaitDebugger() override;
		virtual void pushGraph() override;
		virtual void stopInDebugger() override;
		virtual void log(LogMessage msg) override;
	protected:
        boost::asio::io_context m_ioc;

		const hlim::Circuit *m_circuit = nullptr;

		std::list<std::string> m_logMessages;

        std::atomic<bool> m_shutdown = false;
        std::atomic<bool> m_graphAccessible = false;
        std::mutex m_conditionMutex;
        std::condition_variable m_wakeSessions;
        std::condition_variable m_interfaceSleeping;

        std::mutex m_sessionListMutex;
        std::condition_variable m_newSession;

        std::thread m_acceptorThread;
        std::thread m_sessionThread;

		using tcp = boost::asio::ip::tcp;
		std::list<boost::beast::websocket::stream<tcp::socket>> m_sessions;

		WebSocksInterface(unsigned port);

		void acceptorLoop(unsigned port);
		void acceptSession(tcp::socket socket);
		void sessionLoop();
};

}