cmake_minimum_required (VERSION 3.5)
project (ReadTemperatureSensor)
find_package(Boost 1.76.0 REQUIRED COMPONENTS system)
include_directories(${Boost_INCLUDE_DIRS}) 

add_executable(hdc1000 "hdc1000.cpp" BitBangEngineDriver.h)
set_property(TARGET hdc1000 PROPERTY CXX_STANDARD 20)
target_link_libraries(hdc1000 ${Boost_LIBRARIES})

add_executable(lm71 "lm71.cpp" BitBangEngineDriver.h)
set_property(TARGET lm71 PROPERTY CXX_STANDARD 20)
target_link_libraries(lm71 ${Boost_LIBRARIES})

add_executable(LIS2DH12 "LIS2DH12.cpp" BitBangEngineDriver.h)
set_property(TARGET LIS2DH12 PROPERTY CXX_STANDARD 20)
target_link_libraries(LIS2DH12 ${Boost_LIBRARIES})
