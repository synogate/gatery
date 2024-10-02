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
#include "DebugInterface.h"
#include "websocks/WebSocksInterface.h"
#include "reporting/ReportInterface.h"

namespace gtry::dbg {

LogMessage::LogMessage()
{
}

LogMessage::LogMessage(const hlim::NodeGroup *anchor)
{
	(*this) << Anchor{ anchor };
}

LogMessage::LogMessage(const char *c)
{
	(*this) << c;
}


thread_local std::unique_ptr<DebugInterface> DebugInterface::instance = std::make_unique<DebugInterface>();

void awaitDebugger()
{
	DebugInterface::instance->awaitDebugger();
}

void pushGraph()
{
	DebugInterface::instance->pushGraph();
}

void stopInDebugger()
{
	DebugInterface::instance->stopInDebugger();
}

void operate()
{
	DebugInterface::instance->operate();
}

void changeState(State state, hlim::Circuit* circuit)
{
	DebugInterface::instance->changeState(state, circuit);
}

size_t createAreaVisualization(unsigned width, unsigned height)
{
	return DebugInterface::instance->createAreaVisualization(width, height);
}

void updateAreaVisualization(size_t id, const std::string content)
{
	DebugInterface::instance->updateAreaVisualization(id, std::move(content));
}

void log(const LogMessage &msg)
{
	DebugInterface::instance->log(msg);
}

void vis()
{
	WebSocksInterface::create(1337);
	awaitDebugger();
	stopInDebugger();
}

void logWebsocks(unsigned port)
{
	WebSocksInterface::create(port);
}

void logHtml(const std::filesystem::path &outputDir)
{
	ReportInterface::create(outputDir);
}

std::string howToReachLog()
{
	return DebugInterface::instance->howToReachLog();
}


}
