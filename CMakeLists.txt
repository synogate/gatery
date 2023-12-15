cmake_minimum_required (VERSION 3.20.4)

enable_testing()

# LIST(APPEND CMAKE_MODULE_PATH cmake/)

############################################################## 

find_package(Boost COMPONENTS system filesystem thread iostreams json REQUIRED)
find_package(yaml-cpp REQUIRED)

file(GLOB_RECURSE srcs_gatery 
    "source/gatery/*.cpp" "source/gatery/*.h" "source/gatery/*.c"
)

add_library(gatery STATIC ${srcs_gatery})


target_precompile_headers(gatery
    PUBLIC
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/source/gatery/pch.h>"
)

set(data_files_gatery
)

foreach(file ${data_files_gatery})
    configure_file(${file} ${PROJECT_BINARY_DIR}/${file} COPYONLY)
endforeach(file)

target_include_directories(gatery PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_include_directories(gatery SYSTEM PUBLIC
    ${Boost_INCLUDE_DIRS}
    ${YAML_CPP_INCLUDE_DIRS}
)

target_link_libraries(gatery PUBLIC
    ${Boost_LIBRARIES}
    ${YAML_CPP_LIBRARIES}
)

target_compile_features(gatery PUBLIC cxx_std_23)

target_compile_definitions(gatery PUBLIC NOMINMAX)
target_compile_definitions(gatery PUBLIC WIN32_LEAN_AND_MEAN)
target_compile_definitions(gatery PUBLIC _HAS_DEPRECATED_IS_LITERAL_TYPE=1)

if(WIN32)
    target_compile_definitions(gatery PUBLIC -DBOOST_STACKTRACE_USE_WINDBG)
    # Boost docu: Uses COM to show debug info. May require linking with ole32 and dbgeng.
else()
    target_compile_definitions(gatery PUBLIC -DBOOST_STACKTRACE_USE_BACKTRACE)
    target_link_libraries(gatery PUBLIC -ldl -lbacktrace)
    #target_compile_definitions(gatery PUBLIC -DBOOST_STACKTRACE_USE_ADDR2LINE)
    #target_link_libraries(gatery PUBLIC -ldl)
    target_compile_options(gatery PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-fcoroutines>)
endif()


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
