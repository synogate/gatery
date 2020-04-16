#include "StackTrace.h"

#include "Enumerate.h"
#include "Range.h"

#include <memory>
#include <stdexcept>

#if defined(_MSC_VER)

#elif defined(__GNUC__) && defined(__linux__)

#include <execinfo.h>
#include <cxxabi.h>

#else
#error "Unsupported platform!"
#endif



namespace mhdl {
namespace utils {


#if defined(_MSC_VER)

#pragma message ( "StackTrace not yet implemented" )

/// @todo: look into stack tracing on windows
    
void StackTrace::record(unsigned size, unsigned skipTop) { }
std::vector<std::string> StackTrace::formatEntries() const { return {"Not implemented"}; }

    
#elif defined(__GNUC__) && defined(__linux__)


void StackTrace::record(unsigned size, unsigned skipTop) 
{ 
    std::vector<void*> fullBuffer(size+skipTop);
    unsigned traceSize = backtrace(fullBuffer.data(), fullBuffer.size());
    if (traceSize > skipTop) {
        m_trace.resize(traceSize - skipTop);
        for (unsigned i = skipTop; i < traceSize; i++)
            m_trace[i-skipTop] = fullBuffer[i];
    } else
        m_trace.clear();
}

std::vector<std::string> StackTrace::formatEntries() const 
{ 
    std::unique_ptr<char*, decltype(&free)> buf(backtrace_symbols(m_trace.data(), m_trace.size()), &free);

    if (buf == nullptr)
        throw std::runtime_error("backtrace_symbols failed!");
    
    std::vector<std::string> result;
    result.resize(m_trace.size());
    for (auto i : Range(m_trace.size())) {
        const char *src = buf.get()[i];
#if 0
        int status;
        std::unique_ptr<char, decltype(&free)> demangled(abi::__cxa_demangle(src, 0, 0, &status), &free);
        if (status != 0)
            throw std::runtime_error("__cxa_demangle failed!");
        
        result[i] = demangled.get();
#else
        /// @todo: demangle and get file/line numbers
        result[i] = src;
#endif
    }
    
    return result;
}

#else
#error "Unsupported platform!"
#endif

std::ostream &operator<<(std::ostream &stream, const StackTrace &trace)
{
    auto symbols = trace.formatEntries();
    for (auto p : Enumerate(symbols))
        stream << "    " << p.first << ": " << p.second << std::endl;
    
    return stream;
}

    
}
}
