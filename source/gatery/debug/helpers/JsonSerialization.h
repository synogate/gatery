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
#pragma once

#include <span>
#include <string>
#include <ostream>

namespace gtry::utils {
	class StackTrace;
}

namespace gtry::hlim {
	class Subnet;
	class ConstSubnet;
	class BaseNode;
	class NodeGroup;
	class Circuit;
}

namespace gtry::dbg {
	class LogMessage;
}

/**
 * @addtogroup gtry_frontend_logging
 * @{
 */

namespace gtry::dbg::json {
	void serializeSubnet(std::ostream &json, const hlim::ConstSubnet &subnet);

	void serializeLogMessage(std::ostream &json, const LogMessage &logMessage);
	std::string serializeLogMessage(const LogMessage &logMessage);

	void serializeGroup(std::ostream &json, const hlim::NodeGroup *group, bool reccurse);

	void serializeNode(std::ostream &json, const hlim::BaseNode *node);
	void serializeAllNodes(std::ostream &json, const hlim::Circuit &circuit);

	void serializeStackFrame(std::ostream &json, const void *ptr);
	void serializeStackTrace(std::ostream &json, const utils::StackTrace &trace, bool resolved);

}

/**@}*/