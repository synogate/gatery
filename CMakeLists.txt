cmake_minimum_required (VERSION 3.0)
project (mhdl_core)

LIST(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

LIST(APPEND CMAKE_CXX_FLAGS "-march=native -ffast-math -Wall -pedantic -Wextra -std=c++2a -Wno-int-in-bool-context -Wno-class-memaccess -Wno-unused-parameter -Wno-deprecated-copy")


set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3 -g")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} -O3 -g")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -Og -g")


add_subdirectory(libs/metaHDL_core/)
add_subdirectory(executables/samples/uart)

