#pragma once

#if defined(_MSC_VER)

#define GET_FUNCTION_NAME __FUNCSIG__

#elif defined(__GNUC__)

#define GET_FUNCTION_NAME __PRETTY_FUNCTION__

#else
#error "Unsupported platform!"
#endif


#define MHDL_NAMED(x) mhdl::core::frontend::setName(x, #x)

#define MHDL_SIGNAL \
        virtual const char *getSignalTypeName() override { return GET_FUNCTION_NAME; }


        
#define MHDL_ASSERT(x) { if (!(x)) { throw mhdl::utils::InternalError(__FILE__, __LINE__, std::string("Assertion failed: ") + #x); }}
#define MHDL_ASSERT_HINT(x, message) { if (!(x)) { throw mhdl::utils::InternalError(__FILE__, __LINE__, std::string("Assertion failed: ") + #x + " Hint: " + message); }}


#define MHDL_DESIGNCHECK(x) { if (!(x)) { throw mhdl::utils::DesignError(__FILE__, __LINE__, std::string("Design failed: ") + #x); }}
#define MHDL_DESIGNCHECK_HINT(x, message) { if (!(x)) { throw mhdl::utils::DesignError(__FILE__, __LINE__, std::string("Design failed: ") + #x + " Hint: " + message); }}
