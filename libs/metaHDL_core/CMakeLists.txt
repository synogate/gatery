cmake_minimum_required (VERSION 3.0)

find_package(Boost COMPONENTS system thread REQUIRED) 

add_library(metaHDL_core STATIC
    source/metaHDL_core/frontend/Bit.cpp
    source/metaHDL_core/frontend/Bit.h
    source/metaHDL_core/frontend/BitVector.cpp
    source/metaHDL_core/frontend/BitVector.h
    source/metaHDL_core/frontend/Integers.cpp
    source/metaHDL_core/frontend/Integers.h
    source/metaHDL_core/frontend/Registers.cpp
    source/metaHDL_core/frontend/Registers.h
    source/metaHDL_core/frontend/Scope.cpp
    source/metaHDL_core/frontend/Scope.h
    source/metaHDL_core/frontend/Signal.cpp
    source/metaHDL_core/frontend/Signal.h
    source/metaHDL_core/frontend/SignalArithmeticOp.cpp
    source/metaHDL_core/frontend/SignalArithmeticOp.h
    source/metaHDL_core/frontend/SignalBitshiftOp.cpp
    source/metaHDL_core/frontend/SignalBitshiftOp.h
    source/metaHDL_core/frontend/SignalDelay.cpp
    source/metaHDL_core/frontend/SignalDelay.h
    source/metaHDL_core/frontend/SignalLogicOp.cpp
    source/metaHDL_core/frontend/SignalLogicOp.h

    source/metaHDL_core/hlim/coreNodes/Node_Arithmetic.cpp
    source/metaHDL_core/hlim/coreNodes/Node_Arithmetic.h
    source/metaHDL_core/hlim/coreNodes/Node_Logic.cpp
    source/metaHDL_core/hlim/coreNodes/Node_Logic.h
    source/metaHDL_core/hlim/coreNodes/Node_Register.cpp
    source/metaHDL_core/hlim/coreNodes/Node_Register.h
    source/metaHDL_core/hlim/coreNodes/Node_Rewire.cpp
    source/metaHDL_core/hlim/coreNodes/Node_Rewire.h
    source/metaHDL_core/hlim/coreNodes/Node_Signal.cpp
    source/metaHDL_core/hlim/coreNodes/Node_Signal.h
    
    source/metaHDL_core/hlim/Circuit.cpp
    source/metaHDL_core/hlim/Circuit.h
    source/metaHDL_core/hlim/ConnectionType.cpp
    source/metaHDL_core/hlim/ConnectionType.h
    source/metaHDL_core/hlim/Node.cpp
    source/metaHDL_core/hlim/Node.h
    source/metaHDL_core/hlim/NodeGroup.cpp
    source/metaHDL_core/hlim/NodeGroup.h

    source/metaHDL_core/utils/Enumerate.h
    source/metaHDL_core/utils/Exceptions.cpp
    source/metaHDL_core/utils/Exceptions.h
    source/metaHDL_core/utils/LinkedList.cpp
    source/metaHDL_core/utils/LinkedList.h
    source/metaHDL_core/utils/Preprocessor.h
    source/metaHDL_core/utils/Range.h
    source/metaHDL_core/utils/StackTrace.cpp
    source/metaHDL_core/utils/StackTrace.h
    source/metaHDL_core/utils/Traits.h
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


