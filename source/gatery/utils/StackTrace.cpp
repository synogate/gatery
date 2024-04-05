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
#include "StackTrace.h"

#include "Enumerate.h"
#include "Range.h"

#include <boost/format.hpp>

#include <memory>
#include <stdexcept>


template class std::vector<boost::stacktrace::frame>;

namespace gtry::utils 
{

	void StackTrace::record(size_t size, size_t skipTop) 
	{ 
		boost::stacktrace::stacktrace trace;

		size_t traceSize = trace.size();
		if (traceSize > skipTop) {
			m_trace.resize(traceSize - skipTop);
			for (size_t i = skipTop; i < traceSize; i++)
				m_trace[i-skipTop] = trace[i];
		} else
			m_trace.clear();
	}

	std::vector<std::string> StackTrace::formatEntries() const 
	{ 
#ifdef WIN32
		// ifdef mio .... andy does not like filtering the stack trace
		return formatEntriesFiltered();
#else
		static FrameResolver resolver;

		std::vector<std::string> result;
		result.resize(m_trace.size());
		for (auto i : Range(m_trace.size()))
			result[i] = resolver.to_string(m_trace[i]); // (boost::format("[%08X] %s - %s(%d)") % m_trace[i].address() % m_trace[i].name() % m_trace[i].source_file() % m_trace[i].source_line()).str();
	
		return result;
#endif
	}

	std::vector<std::string> StackTrace::formatEntriesFiltered() const
	{
		static FrameResolver resolver;

		std::vector<std::string> result;
		for (auto i : Range(m_trace.size()))
		{
			std::string formatted = resolver.to_string(m_trace[i]);

			if (formatted.starts_with("boost::"))
				continue;
			if (formatted.starts_with("`boost::"))
				continue;
			if (formatted.starts_with("std::"))
				continue;
			if (formatted.starts_with("`std::"))
				continue;
			if (formatted.starts_with("gtry::") && !formatted.starts_with("gtry::scl::"))
				continue;
			if (formatted.starts_with("`gtry::") && !formatted.starts_with("`gtry::scl::"))
				continue;

			result.emplace_back(std::move(formatted));
		}

		while (!result.empty())
			if (!result.back().starts_with("main "))
				result.pop_back();
			else
				break;

		if (result.size() > 1)
		{
			// remove common path prefix
			std::string prefix;

			for (const std::string& frame : result)
			{
				size_t pos = frame.find(" at ");
				if (pos == std::string::npos)
					continue;
				pos += 4;

				if (prefix.empty())
				{
					prefix = frame.substr(pos);
					continue;
				}

				size_t prefixLen = 0;
				for (char ref : prefix)
					if (pos == frame.size() || ref != frame[pos++])
						break;
					else
						prefixLen++;

				prefix.resize(prefixLen);
			}

			for (std::string& frame : result)
			{
				size_t pos = frame.find(" at ");
				if (pos == std::string::npos)
					continue;
				pos += 4;

				if (pos + prefix.size() <= frame.length())
					frame = frame.substr(0, pos) + frame.substr(pos + prefix.size());
				else
					frame = frame.substr(0, pos);
			}
		}

		return result;
	}

	std::ostream &operator<<(std::ostream &stream, const StackTrace &trace)
	{
		auto symbols = trace.formatEntries();
		for (auto p : Enumerate(symbols))
			stream << "	" << p.first << ": " << p.second << std::endl;
	
		return stream;
	}

	std::string FrameResolver::to_string(const boost::stacktrace::frame& frame)
	{
#ifdef WIN32
		std::string res;
		idebug.to_string_impl(frame.address(), res);

		for (char& c : res)
			if (c == '\\')
				c = '/';

		return res;
#else
		return (boost::format("%s at %s:%d") % frame.name() % frame.source_file() % frame.source_line()).str();
#endif
	}
}
