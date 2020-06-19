#pragma once


#include <boost/stacktrace.hpp>


#include <vector>
#include <string>
#include <ostream>


namespace hcl::utils {
    
    

class StackTrace
{
    public:
        void record(size_t size, size_t skipTop);
        const std::vector<boost::stacktrace::frame> &getTrace() const { return m_trace; }
        std::vector<std::string> formatEntries() const;
    protected:
        std::vector<boost::stacktrace::frame> m_trace;
};

std::ostream &operator<<(std::ostream &stream, const StackTrace &trace);


}
