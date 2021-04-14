/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

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


namespace hcl::utils {


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
    std::vector<std::string> result;
    result.resize(m_trace.size());
    for (auto i : Range(m_trace.size()))
        result[i] = (boost::format("[%08X] %s - %s(%d)") % m_trace[i].address() % m_trace[i].name() % m_trace[i].source_file() % m_trace[i].source_line()).str();
    
    return result;
}

std::ostream &operator<<(std::ostream &stream, const StackTrace &trace)
{
    auto symbols = trace.formatEntries();
    for (auto p : Enumerate(symbols))
        stream << "    " << p.first << ": " << p.second << std::endl;
    
    return stream;
}

    
}
