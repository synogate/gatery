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
