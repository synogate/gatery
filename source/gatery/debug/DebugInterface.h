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

#include <memory>
#include <string>
#include <variant>

#include "../hlim/NodePort.h"
#include "../hlim/Subnet.h"

namespace gtry {

namespace hlim {
	class Subnet;
	class BaseNode;
	class NodeGroup;
	class Circuit;
}

/**
 * @addtogroup gtry_frontend_logging
 * @{
 */

namespace dbg {

/**
 * @brief Helper class for composing logging messages.
 * @details Similarly to std::ostream, it uses the << operator to concatenate message parts.
 * However quite crucially, message parts can be nodes, groups, subnets, etc. such that the logging backend 
 * can properly render these in whatever way is suitable.
 * 
 * A common use case is `log(LogMessage() << LogMessage::LOG_ERROR << LogMessage::LOG_POSTPROCESSING << LogMessage::Anchor(currentGroup) << "Something is wrong with node " << brokenNode);`
 */
class LogMessage
{
	public:
		enum Severity {
			LOG_INFO,
			LOG_WARNING,
			LOG_ERROR
		};

		enum Source {
			LOG_DESIGN,
			LOG_POSTPROCESSING,
			LOG_TECHNOLOGY_MAPPING
		};

		struct Anchor {
			const hlim::NodeGroup *group;
		};

		/// Creates an empty log message
		LogMessage();
		/// Same as `LogMessage() << Anchor(anchor)`
		LogMessage(const hlim::NodeGroup *anchor);
		/// Same as `LogMessage() << c`
		LogMessage(const char *c);

		/// Sets the severity of the log message
		LogMessage &operator<<(Severity s) { m_severity = s; return *this; }
		/// Sets the origin of the log message
		LogMessage &operator<<(Source s) { m_source = s; return *this; }
		/// Sets a node group as an anchor or context of this log message, to allow backends to filter messages by node group.
		LogMessage &operator<<(Anchor a) { m_anchor = a.group; return *this; }

		/// Adds a string message part 
		LogMessage &operator<<(const char *c) { m_messageParts.push_back(c); return *this; }
		/// Adds a string message part 
		LogMessage &operator<<(std::string s) { m_messageParts.push_back(std::move(s)); return *this; }
		/// Adds a string message part
		LogMessage &operator<<(std::string_view s) { m_messageParts.push_back(std::string(s)); return *this; }
		/// @brief Adds a reference to a node to the message.
		/// @details Note that the node may change or even be deleted between now and when the log message is viewed.
		LogMessage &operator<<(const hlim::BaseNode *node) { m_messageParts.push_back(node); return *this; }
		/// @brief Adds a reference to a group to the message.
		/// @details Note that the group may change or even be deleted between now and when the log message is viewed.
		LogMessage &operator<<(const hlim::NodeGroup *group) { m_messageParts.push_back(group); return *this; }
		/// @brief Adds a reference to a node port to the message.
		/// @details Note that the node may change or even be deleted between now and when the log message is viewed.
		LogMessage &operator<<(const hlim::NodePort &nodePort) { m_messageParts.push_back(nodePort); return *this; }
		/// @brief Adds a reference to a subnet to the message, usually to be displayed as a visual graph.
		/// @details Note that the nodes int the subnet may change or even be deleted between now and when the log message is viewed.
		LogMessage &operator<<(hlim::Subnet subnet) { m_messageParts.push_back(std::move(subnet)); return *this; }

		template<std::derived_from<hlim::BaseNode> T>
		LogMessage &operator<<(const T &v) { return this->operator<<((const hlim::BaseNode *)v); }

		template<std::derived_from<hlim::NodeGroup> T>
		LogMessage &operator<<(const T &v) { return this->operator<<((const hlim::NodeGroup *)v); }
/*
		template<typename T>
		LogMessage &operator<<(const T &v) { return this->operator<<(std::to_string(v)); }
*/
		/// Adds an integer number to the message
		LogMessage &operator<<(std::size_t v) { m_messageParts.push_back(std::to_string(v)); return *this; }

		Severity severity() const { return m_severity; }
		Source source() const { return m_source; }

		/// @brief Returns the parts of which this message is composed.
		/// @details Each part is a std::variant that aside from strings can refer entities in the circuit.
		const auto &parts() const { return m_messageParts; }
		const hlim::NodeGroup *anchor() const { return m_anchor; }
	protected:
		Severity m_severity = LOG_INFO;
		Source m_source = LOG_DESIGN;
		const hlim::NodeGroup *m_anchor = nullptr;

		std::vector<std::variant<const char*, std::string, const hlim::BaseNode*, const hlim::NodeGroup*, hlim::Subnet, hlim::NodePort>> m_messageParts;
};

enum class State {
	DESIGN,
	POSTPROCESS,
	POSTPROCESSINGDONE,
	SIMULATION
};

/**
 * @brief Common interface that all logging backends must implement.
 * @details Also serves as the default implementation that silently ignores all log messages.
 */
class DebugInterface 
{
	public:
		virtual ~DebugInterface() = default;

		inline State getState() const { return m_state; }

		thread_local static std::unique_ptr<DebugInterface> instance;

		virtual void awaitDebugger() { }
		virtual void pushGraph() { }
		virtual void stopInDebugger() { }
		virtual void log(LogMessage msg) { }
		virtual void operate() { }
		virtual void changeState(State state, hlim::Circuit* circuit) { m_state = state; }
		virtual std::string howToReachLog() { return "Logging disabled! Rerun with a call to e.g. gtry::dbg::logWebsocks or gtry::dbg::logHtml."; }

		virtual void createVisualization(const std::string &id, const std::string &title) { }
		virtual void updateVisualization(const std::string &id, const std::string &imageData) { }

		virtual size_t createAreaVisualization(unsigned width, unsigned height) { return 0; }
		virtual void updateAreaVisualization(size_t id, const std::string content) { }

	protected:
		State m_state = State::DESIGN;
};

/// Initialize logging to use the browser based web debugger that connects via websockets
void logWebsocks(unsigned port = 1337);
/// Initialize logging to write to a html-file based static log
void logHtml(const std::filesystem::path &outputDir);


void awaitDebugger();
void pushGraph();
void stopInDebugger();
void operate();
void changeState(State state, hlim::Circuit* circuit);

size_t createAreaVisualization(unsigned width, unsigned height);
void updateAreaVisualization(size_t id, const std::string content);

/// Log a message to whatever backend has been initialized.
void log(const LogMessage &msg);
/// Print a short, human readable description of how the log can be accessed.
std::string howToReachLog();


void vis();

}

}

/**@}*/