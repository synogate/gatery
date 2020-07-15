#pragma once

#include "StackTrace.h"
#include "Preprocessor.h"

#include <boost/lexical_cast.hpp>

#include <stdexcept>
#include <csignal>


namespace hcl::utils {

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



class DesignError : public MHDLError<std::runtime_error>
{
    public:
        DesignError(const char *file, size_t line, const std::string &what)  : MHDLError<std::runtime_error>(file, line, what) { }
};


template<class BaseError>
std::ostream &operator<<(std::ostream &stream, const MHDLError<BaseError> &exception) {
    /// @todo change format so that tools can more easily grab filename and line number
    stream 
        << exception.what() << std::endl
        << "Stack trace: " << std::endl
        << exception.getStackTrace();
        
    return stream;
}

}
