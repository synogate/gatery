cmake_minimum_required (VERSION 2.6)

find_package (Boost COMPONENTS system filesystem unit_test_framework REQUIRED)

add_executable(unitTestCore
    source/basics.cpp
    source/main.cpp
)

target_include_directories (unitTestCore PUBLIC
    ${Boost_INCLUDE_DIRS}
)

add_definitions (-DBOOST_TEST_DYN_LINK)
target_link_libraries (unitTestCore
    hcl_net
    hcl_frontend
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
)
