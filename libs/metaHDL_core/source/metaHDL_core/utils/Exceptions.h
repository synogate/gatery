#pragma once

#include "StackTrace.h"


#include <boost/lexical_cast.hpp>

#include <stdexcept>
#include <csignal>


namespace mhdl {
namespace utils {

template<class BaseError>
class MHDLError : public BaseError
{
    public:
        MHDLError(const char *file, size_t line, const std::string &what) : 
                BaseError(what + " Location: " + file + "("+boost::lexical_cast<std::string>(line)+")") {
                    
            m_trace.record(20, 1);
        }
        
        inline const StackTrace &getStackTrace() const { return m_trace; }
    protected:
        StackTrace m_trace;
};

    
class InternalError : public MHDLError<std::logic_error>
{
    public:
        InternalError(const char *file, size_t line, const std::string &what) : MHDLError<std::logic_error>(file, line, what) { }
};

#define MHDL_ASSERT(x) { if (!(x)) { throw mhdl::utils::InternalError(__FILE__, __LINE__, std::string("Assertion failed: ") + #x); }}
#define MHDL_ASSERT_HINT(x, message) { if (!(x)) { throw mhdl::utils::InternalError(__FILE__, __LINE__, std::string("Assertion failed: ") + #x + " Hint: " + message); }}


class DesignError : public MHDLError<std::runtime_error>
{
    public:
        DesignError(const char *file, size_t line, const std::string &what)  : MHDLError<std::runtime_error>(file, line, what) { }
};

#define MHDL_DESIGNCHECK(x) { if (!(x)) { throw mhdl::utils::DesignError(__FILE__, __LINE__, std::string("Design failed: ") + #x); }}
#define MHDL_DESIGNCHECK_HINT(x, message) { if (!(x)) { throw mhdl::utils::DesignError(__FILE__, __LINE__, std::string("Design failed: ") + #x + " Hint: " + message); }}


template<class BaseError>
std::ostream &operator<<(std::ostream &stream, const MHDLError<BaseError> &exception) {
    stream 
        << exception.what() << std::endl
        << "Stack trace: " << std::endl
        << exception.getStackTrace();
        
    return stream;
}

}
}
