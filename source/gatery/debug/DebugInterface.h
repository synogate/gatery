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

#include "../hlim/Subnet.h"

#include <memory>
#include <string>
#include <variant>

namespace gtry {

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

		LogMessage();
		LogMessage(const char *c);

		LogMessage &operator<<(Severity s) { m_severity = s; return *this; }
		LogMessage &operator<<(Source s) { m_source = s; return *this; }

		LogMessage &operator<<(const char *c) { m_messageParts.push_back(c); return *this; }
		LogMessage &operator<<(std::string s) { m_messageParts.push_back(std::move(s)); return *this; }
		LogMessage &operator<<(const hlim::BaseNode *node) { m_messageParts.push_back(node); return *this; }
		LogMessage &operator<<(const hlim::NodeGroup *group) { m_messageParts.push_back(group); return *this; }
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
	protected:
		Severity m_severity = LOG_INFO;
		Source m_source = LOG_DESIGN;

		std::vector<std::variant<const char*, std::string, const hlim::BaseNode*, const hlim::NodeGroup*, hlim::Subnet>> m_messageParts;
};

class DebugInterface 
{
	public:
		static std::unique_ptr<DebugInterface> instance;

		virtual void awaitDebugger() { }
		virtual void pushGraph() { }
		virtual void stopInDebugger() { }
		virtual void log(LogMessage msg) { }
	protected:
};

void awaitDebugger();
void pushGraph();
void stopInDebugger();
void log(const LogMessage &msg);

}

}