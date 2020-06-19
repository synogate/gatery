#pragma once

#if defined(_MSC_VER)

#define GET_FUNCTION_NAME __FUNCSIG__

#elif defined(__GNUC__)

#define GET_FUNCTION_NAME __PRETTY_FUNCTION__

#else
#error "Unsupported platform!"
#endif


#define HCL_NAMED(x) hcl::core::frontend::setName(x, #x)

#define HCL_SIGNAL \
        virtual const char *getSignalTypeName() override { return GET_FUNCTION_NAME; }


        
#define HCL_ASSERT(x) { if (!(x)) { throw hcl::utils::InternalError(__FILE__, __LINE__, std::string("Assertion failed: ") + #x); }}
#define HCL_ASSERT_HINT(x, message) { if (!(x)) { throw hcl::utils::InternalError(__FILE__, __LINE__, std::string("Assertion failed: ") + #x + " Hint: " + message); }}


#define HCL_DESIGNCHECK(x) { if (!(x)) { throw hcl::utils::DesignError(__FILE__, __LINE__, std::string("Design failed: ") + #x); }}
#define HCL_DESIGNCHECK_HINT(x, message) { if (!(x)) { throw hcl::utils::DesignError(__FILE__, __LINE__, std::string("Design failed: ") + #x + " Hint: " + message); }}
