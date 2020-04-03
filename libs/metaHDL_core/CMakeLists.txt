cmake_minimum_required (VERSION 3.0)

find_package(Boost COMPONENTS system thread REQUIRED) 

add_library(metaHDL_core STATIC
    source/metaHDL_core/frontend/BitVector.cpp
    source/metaHDL_core/frontend/BitVector.h
    source/metaHDL_core/frontend/Integers.cpp
    source/metaHDL_core/frontend/Integers.h
    source/metaHDL_core/frontend/Registers.cpp
    source/metaHDL_core/frontend/Registers.h
    source/metaHDL_core/frontend/Signal.cpp
    source/metaHDL_core/frontend/Signal.h
    source/metaHDL_core/frontend/SignalDelay.cpp
    source/metaHDL_core/frontend/SignalDelay.h

    
    source/metaHDL_core/hlim/Node.cpp
    source/metaHDL_core/hlim/Node.h
    source/metaHDL_core/hlim/Connection.cpp
    source/metaHDL_core/hlim/Connection.h
    source/metaHDL_core/hlim/Circuit.cpp
    source/metaHDL_core/hlim/Circuit.h
)

set(data_files 
)

foreach(file ${data_files})
    configure_file(${file} ${PROJECT_BINARY_DIR}/${file} COPYONLY)
endforeach(file)


target_include_directories(metaHDL_core PUBLIC
    ${Boost_INCLUDE_DIRS}
)

target_include_directories(metaHDL_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_link_libraries(metaHDL_core PUBLIC
    ${Boost_LIBRARIES}
)


