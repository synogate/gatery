cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Boost COMPONENTS unit_test_framework filesystem serialization date_time REQUIRED)
include_directories (${Boost_INCLUDE_DIRS})

find_program(GHDL_BINARY NAMES "ghdl" HINTS ENV GHDL_PATH PATH_SUFFIXES "bin")
get_filename_component(GHDL_BIN_PATH ${GHDL_BINARY} DIRECTORY)
get_filename_component(GHDL_PATH ${GHDL_BIN_PATH} DIRECTORY)

link_directories("lib")

add_library(vpi_simulation_host SHARED source/vpi_simulation_host.cpp "source/VpiModule.cpp" "source/ClockGenerator.cpp")
target_include_directories(vpi_simulation_host PUBLIC "${GHDL_PATH}/include")

add_library(vpi_simulation_client "source/GhdlSimulation.cpp")

target_link_libraries(vpi_simulation_host 
	${Boost_SERIALIZATION_LIBRARY}
	${Boost_DATE_TIME_LIBRARY}
	)

if(WIN32)
	target_link_libraries(vpi_simulation_host "libghdlvpi.lib")
	target_compile_definitions(vpi_simulation_host PUBLIC PLI_DLLISPEC=)
endif()


add_executable(vpi_simulation_host_test tests/ghdl_test.cpp)
target_include_directories(vpi_simulation_host_test PUBLIC "source")
add_dependencies(vpi_simulation_host_test vpi_simulation_host vpi_simulation_client)
target_link_libraries(vpi_simulation_host_test 
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_SERIALIZATION_LIBRARY}
	${Boost_DATE_TIME_LIBRARY}
	${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
	vpi_simulation_client
	)

add_test(ghdl_test vpi_simulation_host_test)
