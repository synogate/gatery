cmake_minimum_required (VERSION 3.0)
project (mhdl_core)

option(WITH_QT "Include QT stuff in the build" off)

enable_testing()

LIST(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

if(NOT MSVC)
    LIST(APPEND CMAKE_CXX_FLAGS "-march=native -ffast-math -Wall -pedantic -Wextra -std=c++2a -Wno-int-in-bool-context -Wno-class-memaccess -Wno-unused-parameter -Wno-deprecated-copy")

    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3 -g")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} -O3 -g")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g")
endif()

if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

#add_subdirectory(libs/cppDWARF/)
add_subdirectory(libs/metaHDL_core/)

add_subdirectory(libs/vpi_simulation_host)

if(${WITH_QT})
add_subdirectory(libs/metaHDL_vis_qt/)
endif()

add_subdirectory(executables/samples/uart)
add_subdirectory(executables/samples/gameoflife)
