#pragma once


#include <vector>
#include <string>
#include <ostream>

namespace mhdl {
namespace utils {
    
    

class StackTrace
{
    public:
        void record(size_t size, size_t skipTop);
        const std::vector<void*> &getTrace() const { return m_trace; }
        std::vector<std::string> formatEntries() const;
    protected:
        std::vector<void*> m_trace;
};

std::ostream &operator<<(std::ostream &stream, const StackTrace &trace);


}
}
