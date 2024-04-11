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

namespace dbg {

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

		LogMessage();
		LogMessage(const hlim::NodeGroup *anchor);
		LogMessage(const char *c);

		LogMessage &operator<<(Severity s) { m_severity = s; return *this; }
		LogMessage &operator<<(Source s) { m_source = s; return *this; }
		LogMessage &operator<<(Anchor a) { m_anchor = a.group; return *this; }

		LogMessage &operator<<(const char *c) { m_messageParts.push_back(c); return *this; }
		LogMessage &operator<<(std::string s) { m_messageParts.push_back(std::move(s)); return *this; }
		LogMessage &operator<<(std::string_view s) { m_messageParts.push_back(std::string(s)); return *this; }
		LogMessage &operator<<(const hlim::BaseNode *node) { m_messageParts.push_back(node); return *this; }
		LogMessage &operator<<(const hlim::NodeGroup *group) { m_messageParts.push_back(group); return *this; }
		LogMessage &operator<<(const hlim::NodePort &nodePort) { m_messageParts.push_back(nodePort); return *this; }
		LogMessage &operator<<(hlim::Subnet subnet) { m_messageParts.push_back(std::move(subnet)); return *this; }

		template<std::derived_from<hlim::BaseNode> T>
		LogMessage &operator<<(const T &v) { return this->operator<<((const hlim::BaseNode *)v); }

		template<std::derived_from<hlim::NodeGroup> T>
		LogMessage &operator<<(const T &v) { return this->operator<<((const hlim::NodeGroup *)v); }
/*
		template<typename T>
		LogMessage &operator<<(const T &v) { return this->operator<<(std::to_string(v)); }
*/
		LogMessage &operator<<(std::size_t v) { m_messageParts.push_back(std::to_string(v)); return *this; }

		Severity severity() const { return m_severity; }
		Source source() const { return m_source; }

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


void logWebsocks(unsigned port = 1337);
void logHtml(const std::filesystem::path &outputDir);


void awaitDebugger();
void pushGraph();
void stopInDebugger();
void operate();
void changeState(State state, hlim::Circuit* circuit);
size_t createAreaVisualization(unsigned width, unsigned height);
void updateAreaVisualization(size_t id, const std::string content);
void log(const LogMessage &msg);
std::string howToReachLog();


void vis();

}

}
