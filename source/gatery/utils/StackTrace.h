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


#include <boost/stacktrace.hpp>


#include <vector>
#include <string>
#include <ostream>


namespace gtry::utils {
	
	class FrameResolver
	{
	public:
		std::string to_string(const boost::stacktrace::frame& frame);

	private:
#ifdef WIN32
		boost::stacktrace::detail::debugging_symbols idebug; 
#endif
	};

	class StackTrace
	{
	public:
		void record(size_t size, size_t skipTop);
		const std::vector<boost::stacktrace::frame> &getTrace() const { return m_trace; }
		std::vector<std::string> formatEntries() const;
		std::vector<std::string> formatEntriesFiltered() const;
	protected:
		std::vector<boost::stacktrace::frame> m_trace;
	};

	std::ostream &operator<<(std::ostream &stream, const StackTrace &trace);
}

extern template class std::vector<boost::stacktrace::frame>;

