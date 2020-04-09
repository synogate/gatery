#pragma once

#if defined(_MSC_VER)

#define GET_FUNCTION_NAME __FUNCSIG__

#elif defined(__GNUC__)

#define GET_FUNCTION_NAME __PRETTY_FUNCTION__

#else
#error "Unsupported platform!"
#endif
