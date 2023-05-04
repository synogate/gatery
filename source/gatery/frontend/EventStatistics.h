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

#include "Bit.h"
#include <filesystem>
#include "Scope.h"
#include <functional>






/**
 * @addtogroup gtry_signals
 * @{
 */
 
	
	

	namespace gtry{

	/**
	 * @brief Event counter for attached Signals
	 * @details Counts how offen an added Bit was high during runtime of the simulation.  
	 */
class EventStatistics : public BaseScope<EventStatistics> {
public:
	void addEvent(std::string_view name, Bit trigger);
	void dumpStatistics();
	size_t readEventCounter(std::string_view name);
	void writeStatTable(std::filesystem::path file_name);
	static EventStatistics* get() { return m_currentScope; }

protected:
	std::map<std::string, size_t, std::less<>> m_counter;
	std::string getNodePath(std::string_view name);
};

void registerEvent(std::string_view name, Bit trigger);

/**@}*/

}
