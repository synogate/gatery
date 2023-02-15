cmake_minimum_required (VERSION 3.20.4)

find_package (Boost COMPONENTS system filesystem unit_test_framework REQUIRED)

file(GLOB_RECURSE srcs_unittest_tutorial  "*.cpp" "*.h" "*.c")

add_executable(unittest_tutorial ${srcs_unittest_tutorial})

target_precompile_headers(unittest_tutorial
    PUBLIC
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.h>"
)

target_include_directories (unittest_tutorial PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${Boost_INCLUDE_DIRS}
)

add_definitions (-DBOOST_TEST_DYN_LINK)
target_link_libraries (unittest_tutorial
    gatery
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
)

add_test(core unittest_tutorial)