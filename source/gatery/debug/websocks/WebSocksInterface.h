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

#include <vector>
#include <span>

namespace gtry::utils {
	class StackTrace;
}

namespace gtry::hlim {
	class Circuit;
}


/**
 * @addtogroup gtry_frontend_logging
 * @{
 */

namespace gtry::dbg {

class WebSocksInterface : public DebugInterface
{
	public:
		virtual ~WebSocksInterface();

		static void create(unsigned port = 1337);

		virtual std::string howToReachLog() override;

		virtual void awaitDebugger() override;
		virtual void pushGraph() override;
		virtual void stopInDebugger() override;
		virtual void log(LogMessage msg) override;
		virtual void operate() override;
		virtual void changeState(State state, hlim::Circuit* circuit) override;

		virtual void createVisualization(const std::string &id, const std::string &title) override;
		virtual void updateVisualization(const std::string &id, const std::string &imageData) override;

		virtual size_t createAreaVisualization(unsigned width, unsigned height) override;
		virtual void updateAreaVisualization(size_t id, const std::string content) override;
	protected:
		boost::asio::io_context m_ioc;

		struct Visualization {
			std::string title;
			std::string content;
			size_t contentVersion = 0;
		};

		std::map<std::string, Visualization> m_visualizations;

		struct AreaVisualizations {
			unsigned width, height;
			size_t nodeGroupId = ~0ull;
			std::string content;
			size_t contentVersion = 0;
		};
		std::vector<AreaVisualizations> m_areaVisualizations;

		const hlim::Circuit *m_circuit = nullptr;
		std::vector<std::string> m_logMessages;

		using tcp = boost::asio::ip::tcp;
		tcp::acceptor m_acceptor;

		struct Session {
			bool closing = false;
			bool ready = false;
			bool graphDirty = true;
			bool stateDirty = true;
			size_t messagesSend = 0;
			std::map<std::string, size_t> visualizationStates;
			std::vector<size_t> areaVisStates;

			boost::beast::flat_buffer buffer;
			boost::beast::http::request_parser<boost::beast::http::empty_body> req;
			boost::beast::websocket::stream<tcp::socket> websockStream;

			Session(tcp::socket socket) : websockStream(std::move(socket)) { }
		};

		std::list<Session> m_sessions;

		WebSocksInterface(unsigned port);

		void waitAccept();
		void acceptSession(tcp::socket socket);
		void closeSession(Session &session);

		void awaitRequest(Session &session);
};

}

/**@}*/