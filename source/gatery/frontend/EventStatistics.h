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





namespace gtry {

	/**
	 * @brief Event counter for attached Signals
	 * @details Counts how offen an added Bit was high during runtime of the simulation.  
	 */
	class EventStatistics : public BaseScope<EventStatistics> {
		public:
			/// @brief Adds Bit Signal to observing list
			/// @details Counts how offen an added Bit was high during runtime of the simulation.
			void addEvent(std::string_view name, const Bit &trigger);
			/// Prints all observed signals with counter values to the terminal
			static void dumpStatistics() { get()->protDumpStatistics(); }
			/// Getter for signal counter. Expects full Nodepath.
			static size_t readEventCounter(std::string_view name) { return get()->protReadEventCounter(name); }
			/// Prints all observed signals with counter values to a .csv file. 
			static void writeStatTable(const std::filesystem::path &file_name) { get()->protWriteStatTable(file_name); }

			static EventStatistics* get() { return m_currentScope; }
		protected:
			std::map<std::string, size_t, std::less<>> m_counter;

			/// Returns full Nodepath for given Node
			std::string getNodePath(std::string_view name) const;

			void protDumpStatistics() const;
			size_t protReadEventCounter(std::string_view name) const;
			void protWriteStatTable(const std::filesystem::path &file_name) const;
	};

	/// Register an event to be recorded in the EventStatistics
	void registerEvent(std::string_view name, const Bit& trigger);

}
