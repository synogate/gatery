#cmake_minimum_required (VERSION 3.24)
cmake_minimum_required (VERSION 3.20)

project(gatery)

IF (USE_CLANG)
    Message("Building with clang!")
    SET (CMAKE_CXX_COMPILER_ID "Clang")
    SET (CMAKE_C_COMPILER             "/usr/bin/clang")
    SET (CMAKE_CXX_COMPILER             "/usr/bin/clang++")
    SET (CMAKE_AR      "/usr/bin/llvm-ar")
    SET (CMAKE_LINKER  "/usr/bin/llvm-ld")
    SET (CMAKE_NM      "/usr/bin/llvm-nm")
    SET (CMAKE_OBJDUMP "/usr/bin/llvm-objdump")
    SET (CMAKE_RANLIB  "/usr/bin/llvm-ranlib")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftime-trace")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

enable_testing()

INCLUDE(cmake/GtryCompileResourceFile.cmake)

############################################################## 

find_package(Boost COMPONENTS system filesystem thread iostreams json REQUIRED)
find_package(yaml-cpp REQUIRED)

file(GLOB_RECURSE srcs_gatery 
    "source/gatery/*.cpp" "source/gatery/*.h" "source/gatery/*.c"
)

file(GLOB_RECURSE srcs_gatery_scl 
    "source/gatery/scl/*.cpp" "source/gatery/scl/*.h" "source/gatery/scl/*.c"
)
list(REMOVE_ITEM srcs_gatery ${srcs_gatery_scl})

file(GLOB_RECURSE data_files_gatery RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "data/*"
)

gtry_compileResourceFile(
    NAMESPACE "gtry::res"
    OUTPUT_PREFIX gen/res/gtry_resources
    BINARY_FILES ${data_files_gatery}
)

add_library(gatery_core STATIC 
    ${srcs_gatery}
    ${CMAKE_CURRENT_BINARY_DIR}/gen/res/gtry_resources.h
    ${CMAKE_CURRENT_BINARY_DIR}/gen/res/gtry_resources.cpp
)

target_precompile_headers(gatery_core
    PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/source/gatery/pch.h>"
)

target_include_directories(gatery_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_BINARY_DIR}/gen
)

target_include_directories(gatery_core SYSTEM PUBLIC
    ${Boost_INCLUDE_DIRS}
    ${YAML_CPP_INCLUDE_DIRS}
)

target_link_directories(gatery_core PUBLIC
    ${Boost_LIBRARY_DIR}
    ${YAML_CPP_LIBRARY_DIR}
)

target_link_libraries(gatery_core PUBLIC
    ${Boost_LIBRARIES}
    ${YAML_CPP_LIBRARIES}
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(gatery_core PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++2b>)
else()
    target_compile_features(gatery_core PUBLIC cxx_std_23)
endif()

target_compile_definitions(gatery_core PUBLIC NOMINMAX)
target_compile_definitions(gatery_core PUBLIC WIN32_LEAN_AND_MEAN)
target_compile_definitions(gatery_core PUBLIC _HAS_DEPRECATED_IS_LITERAL_TYPE=1)

if (APPLE)
    target_compile_definitions(gatery_core PUBLIC _GNU_SOURCE)
    target_compile_options(gatery_core PUBLIC -Wunqualified-std-cast-call)
#    target_link_libraries(gatery PUBLIC -lstdc++20)
endif()

if(WIN32)
    target_compile_definitions(gatery_core PUBLIC -DBOOST_STACKTRACE_USE_WINDBG)
    # Boost docu: Uses COM to show debug info. May require linking with ole32 and dbgeng.
else()
    target_compile_definitions(gatery_core PUBLIC -DBOOST_STACKTRACE_USE_BACKTRACE)
    target_link_libraries(gatery_core PUBLIC -ldl -lbacktrace)
    #target_compile_definitions(gatery_core PUBLIC -DBOOST_STACKTRACE_USE_ADDR2LINE)
    #target_link_libraries(gatery_core PUBLIC -ldl)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    else()
        target_compile_options(gatery_core PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-fcoroutines>)
    endif ()
endif()



add_library(gatery_scl STATIC 
    ${srcs_gatery_scl}
)

target_precompile_headers(gatery_scl
    PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/source/gatery/scl_pch.h>"
)

target_link_libraries(gatery_scl PUBLIC
    gatery_core
)

add_library(gatery INTERFACE)

target_link_libraries(gatery INTERFACE
    # Must be a -start-group -end-group thing because the two have cyclic dependencies
#    "$<LINK_GROUP:RESCAN,gatery_core;gatery_scl>"
    -Wl,--start-group
    gatery_scl
    gatery_core
    -Wl,--end-group
)


############################################################## 

add_subdirectory(tests/frontend)
add_subdirectory(tests/scl)
add_subdirectory(tests/tutorial)

#############################################################


file(GLOB_RECURSE srcs_driverUtils
    "source/gatery/scl/driver/*.cpp" "source/gatery/scl/driver/*.h" "source/gatery/scl/driver/*.c"
)

add_library(gatery_driver_utils STATIC ${srcs_driverUtils})

target_include_directories(gatery_driver_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)


target_include_directories(gatery_driver_utils SYSTEM PUBLIC
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(gatery_driver_utils PUBLIC
    ${Boost_LIBRARIES}
)


target_compile_features(gatery_driver_utils PUBLIC cxx_std_23)

target_compile_definitions(gatery_driver_utils PUBLIC NOMINMAX)
target_compile_definitions(gatery_driver_utils PUBLIC WIN32_LEAN_AND_MEAN)
target_compile_definitions(gatery_driver_utils PUBLIC _HAS_DEPRECATED_IS_LITERAL_TYPE=1)
