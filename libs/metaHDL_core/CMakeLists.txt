cmake_minimum_required (VERSION 3.0)

find_package(Boost COMPONENTS system thread REQUIRED) 

add_library(metaHDL_core STATIC
    source/metaHDL_core/export/vhdl/CodeFormatting.cpp
    source/metaHDL_core/export/vhdl/CodeFormatting.h
    source/metaHDL_core/export/vhdl/VHDL_AST.cpp
    source/metaHDL_core/export/vhdl/VHDL_AST.h
    source/metaHDL_core/export/vhdl/VHDLExport.cpp
    source/metaHDL_core/export/vhdl/VHDLExport.h

    source/metaHDL_core/frontend/Bit.cpp
    source/metaHDL_core/frontend/Bit.h
    source/metaHDL_core/frontend/BitVector.cpp
    source/metaHDL_core/frontend/BitVector.h
    source/metaHDL_core/frontend/Integers.cpp
    source/metaHDL_core/frontend/Integers.h
    source/metaHDL_core/frontend/PriorityConditional.cpp
    source/metaHDL_core/frontend/PriorityConditional.h
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
    source/metaHDL_core/hlim/coreNodes/Node_Compare.cpp
    source/metaHDL_core/hlim/coreNodes/Node_Compare.h
    source/metaHDL_core/hlim/coreNodes/Node_Constant.cpp
    source/metaHDL_core/hlim/coreNodes/Node_Constant.h
    source/metaHDL_core/hlim/coreNodes/Node_Logic.cpp
    source/metaHDL_core/hlim/coreNodes/Node_Logic.h
    source/metaHDL_core/hlim/coreNodes/Node_Multiplexer.cpp
    source/metaHDL_core/hlim/coreNodes/Node_Multiplexer.h
    source/metaHDL_core/hlim/coreNodes/Node_PriorityConditional.cpp
    source/metaHDL_core/hlim/coreNodes/Node_PriorityConditional.h
    source/metaHDL_core/hlim/coreNodes/Node_Register.cpp
    source/metaHDL_core/hlim/coreNodes/Node_Register.h
    source/metaHDL_core/hlim/coreNodes/Node_Rewire.cpp
    source/metaHDL_core/hlim/coreNodes/Node_Rewire.h
    source/metaHDL_core/hlim/coreNodes/Node_Signal.cpp
    source/metaHDL_core/hlim/coreNodes/Node_Signal.h
    
    source/metaHDL_core/hlim/Circuit.cpp
    source/metaHDL_core/hlim/Circuit.h
    source/metaHDL_core/hlim/Clock.cpp
    source/metaHDL_core/hlim/Clock.h
    source/metaHDL_core/hlim/ConnectionType.cpp
    source/metaHDL_core/hlim/ConnectionType.h
    source/metaHDL_core/hlim/Node.cpp
    source/metaHDL_core/hlim/Node.h
    source/metaHDL_core/hlim/NodeCategorization.cpp
    source/metaHDL_core/hlim/NodeCategorization.h
    source/metaHDL_core/hlim/NodeGroup.cpp
    source/metaHDL_core/hlim/NodeGroup.h
    source/metaHDL_core/hlim/NodeIO.cpp
    source/metaHDL_core/hlim/NodeIO.h

    source/metaHDL_core/simulation/BitAllocator.cpp
    source/metaHDL_core/simulation/BitAllocator.h
    source/metaHDL_core/simulation/BitVectorState.cpp
    source/metaHDL_core/simulation/BitVectorState.h
    source/metaHDL_core/simulation/ReferenceSimulator.cpp
    source/metaHDL_core/simulation/ReferenceSimulator.h
    source/metaHDL_core/simulation/Simulator.cpp
    source/metaHDL_core/simulation/Simulator.h
    source/metaHDL_core/simulation/SimulatorControl.cpp
    source/metaHDL_core/simulation/SimulatorControl.h
    
    source/metaHDL_core/utils/BitManipulation.h
    source/metaHDL_core/utils/BitManipulation.cpp
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


target_include_directories(metaHDL_core SYSTEM PUBLIC
    ${Boost_INCLUDE_DIRS}
)

target_include_directories(metaHDL_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_link_libraries(metaHDL_core PUBLIC
    ${Boost_LIBRARIES}
)

target_compile_features(metaHDL_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_definitions(metaHDL_core PUBLIC -DBOOST_STACKTRACE_USE_WINDBG)
    # Boost docu: Uses COM to show debug info. May require linking with ole32 and dbgeng. 
else()
    #target_compile_definitions(metaHDL_core PUBLIC -DBOOST_STACKTRACE_USE_BACKTRACE)
    #target_link_libraries(metaHDL_core PUBLIC -ldl -lbacktrace)
    target_compile_definitions(metaHDL_core PUBLIC -DBOOST_STACKTRACE_USE_ADDR2LINE)
    target_link_libraries(metaHDL_core PUBLIC -ldl)
endif()



