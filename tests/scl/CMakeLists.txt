cmake_minimum_required (VERSION 3.20.4)


find_package (Boost CONFIG COMPONENTS system filesystem unit_test_framework REQUIRED)

file(GLOB_RECURSE srcs_unittest_scl  "*.cpp" "*.h" "*.c")


file(GLOB_RECURSE test_data_files_gatery RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "../data/*"
)

gtry_compileResourceFile(
    NAMESPACE "gtry::scl_test_res"
    OUTPUT_PREFIX gen/res/gtry_test_resources
    BINARY_FILES ${test_data_files_gatery}
)

add_executable(unittest_scl 
    ${CMAKE_CURRENT_BINARY_DIR}/gen/res/gtry_test_resources.h
    ${CMAKE_CURRENT_BINARY_DIR}/gen/res/gtry_test_resources.cpp
    ${srcs_unittest_scl}
)


target_precompile_headers(unittest_scl
    PUBLIC
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.h>"
)

target_include_directories (unittest_scl PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_BINARY_DIR}/gen
    ${Boost_INCLUDE_DIRS}
)


add_definitions (-DBOOST_TEST_DYN_LINK)
target_link_libraries (unittest_scl
    gatery
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
)


add_custom_command(TARGET unittest_scl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../external
            ${CMAKE_BINARY_DIR}/external
)

add_custom_command(TARGET unittest_scl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../export
            ${CMAKE_BINARY_DIR}/export
)

add_custom_command(TARGET unittest_scl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../data
            ${CMAKE_BINARY_DIR}/data
)


add_test(core unittest_scl)
