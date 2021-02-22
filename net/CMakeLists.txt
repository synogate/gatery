cmake_minimum_required (VERSION 3.0)

find_package(Boost COMPONENTS system thread REQUIRED)

LIST(APPEND HCL_ALL_LIBS hcl_net)
SET(HCL_ALL_LIBS ${HCL_ALL_LIBS} PARENT_SCOPE)

add_library(hcl_net STATIC
    source/hcl/export/vhdl/AST.cpp
    source/hcl/export/vhdl/AST.h
    source/hcl/export/vhdl/BaseGrouping.cpp
    source/hcl/export/vhdl/BaseGrouping.h
    source/hcl/export/vhdl/BasicBlock.cpp
    source/hcl/export/vhdl/BasicBlock.h
    source/hcl/export/vhdl/Block.cpp
    source/hcl/export/vhdl/Block.h
    source/hcl/export/vhdl/CodeFormatting.cpp
    source/hcl/export/vhdl/CodeFormatting.h
    source/hcl/export/vhdl/Entity.cpp
    source/hcl/export/vhdl/Entity.h
    source/hcl/export/vhdl/GenericMemoryEntity.cpp
    source/hcl/export/vhdl/GenericMemoryEntity.h
    source/hcl/export/vhdl/NamespaceScope.cpp
    source/hcl/export/vhdl/NamespaceScope.h
    source/hcl/export/vhdl/Process.cpp
    source/hcl/export/vhdl/Process.h
    source/hcl/export/vhdl/TestbenchRecorder.cpp
    source/hcl/export/vhdl/TestbenchRecorder.h
    source/hcl/export/vhdl/VHDLExport.cpp
    source/hcl/export/vhdl/VHDLExport.h
    source/hcl/export/DotExport.cpp
    source/hcl/export/DotExport.h

    source/hcl/hlim/coreNodes/Node_Arithmetic.cpp
    source/hcl/hlim/coreNodes/Node_Arithmetic.h
    source/hcl/hlim/coreNodes/Node_Clk2Signal.cpp
    source/hcl/hlim/coreNodes/Node_Clk2Signal.h
    source/hcl/hlim/coreNodes/Node_Compare.cpp
    source/hcl/hlim/coreNodes/Node_Compare.h
    source/hcl/hlim/coreNodes/Node_Constant.cpp
    source/hcl/hlim/coreNodes/Node_Constant.h
    source/hcl/hlim/coreNodes/Node_Logic.cpp
    source/hcl/hlim/coreNodes/Node_Logic.h
    source/hcl/hlim/coreNodes/Node_Multiplexer.cpp
    source/hcl/hlim/coreNodes/Node_Multiplexer.h
    source/hcl/hlim/coreNodes/Node_Pin.cpp
    source/hcl/hlim/coreNodes/Node_Pin.h
    source/hcl/hlim/coreNodes/Node_PriorityConditional.cpp
    source/hcl/hlim/coreNodes/Node_PriorityConditional.h
    source/hcl/hlim/coreNodes/Node_Register.cpp
    source/hcl/hlim/coreNodes/Node_Register.h
    source/hcl/hlim/coreNodes/Node_Rewire.cpp
    source/hcl/hlim/coreNodes/Node_Rewire.h
    source/hcl/hlim/coreNodes/Node_Signal.cpp
    source/hcl/hlim/coreNodes/Node_Signal.h

    source/hcl/hlim/supportNodes/Node_External.cpp
    source/hcl/hlim/supportNodes/Node_External.h
    source/hcl/hlim/supportNodes/Node_Memory.cpp
    source/hcl/hlim/supportNodes/Node_Memory.h
    #source/hcl/hlim/supportNodes/Node_MemReadPort.cpp
    #source/hcl/hlim/supportNodes/Node_MemReadPort.h
    #source/hcl/hlim/supportNodes/Node_MemWritePort.cpp
    #source/hcl/hlim/supportNodes/Node_MemWritePort.h
    source/hcl/hlim/supportNodes/Node_MemPort.cpp
    source/hcl/hlim/supportNodes/Node_MemPort.h
    source/hcl/hlim/supportNodes/Node_SignalGenerator.cpp
    source/hcl/hlim/supportNodes/Node_SignalGenerator.h
    source/hcl/hlim/supportNodes/Node_SignalTap.cpp
    source/hcl/hlim/supportNodes/Node_SignalTap.h

    source/hcl/hlim/Circuit.cpp
    source/hcl/hlim/Circuit.h
    source/hcl/hlim/Clock.cpp
    source/hcl/hlim/Clock.h
    source/hcl/hlim/GraphExploration.cpp
    source/hcl/hlim/GraphExploration.h
    source/hcl/hlim/ConnectionType.cpp
    source/hcl/hlim/ConnectionType.h
    source/hcl/hlim/Node.cpp
    source/hcl/hlim/Node.h
    source/hcl/hlim/NodeCategorization.cpp
    source/hcl/hlim/NodeCategorization.h
    source/hcl/hlim/NodeGroup.cpp
    source/hcl/hlim/NodeGroup.h
    source/hcl/hlim/NodeIO.cpp
    source/hcl/hlim/NodeIO.h
    source/hcl/hlim/MemoryDetector.cpp
    source/hcl/hlim/MemoryDetector.h
    source/hcl/hlim/SignalGroup.cpp
    source/hcl/hlim/SignalGroup.h

    source/hcl/simulation/simProc/SimulationProcess.cpp
    source/hcl/simulation/simProc/SimulationProcess.h
    source/hcl/simulation/simProc/WaitClock.cpp
    source/hcl/simulation/simProc/WaitClock.h
    source/hcl/simulation/simProc/WaitFor.cpp
    source/hcl/simulation/simProc/WaitFor.h
    source/hcl/simulation/simProc/WaitUntil.cpp
    source/hcl/simulation/simProc/WaitUntil.h

    source/hcl/simulation/BitAllocator.cpp
    source/hcl/simulation/BitAllocator.h
    source/hcl/simulation/BitVectorState.cpp
    source/hcl/simulation/BitVectorState.h
    source/hcl/simulation/ConstructionTimeSimulationContext.cpp
    source/hcl/simulation/ConstructionTimeSimulationContext.h
    source/hcl/simulation/ReferenceSimulator.cpp
    source/hcl/simulation/ReferenceSimulator.h
    source/hcl/simulation/RunTimeSimulationContext.cpp
    source/hcl/simulation/RunTimeSimulationContext.h
    source/hcl/simulation/SigHandle.cpp
    source/hcl/simulation/SigHandle.h
    source/hcl/simulation/SimulationContext.cpp
    source/hcl/simulation/SimulationContext.h
    source/hcl/simulation/Simulator.cpp
    source/hcl/simulation/Simulator.h
    source/hcl/simulation/SimulatorCallbacks.cpp
    source/hcl/simulation/SimulatorCallbacks.h
    source/hcl/simulation/SimulatorControl.cpp
    source/hcl/simulation/SimulatorControl.h
    source/hcl/simulation/UnitTestSimulationFixture.cpp
    source/hcl/simulation/UnitTestSimulationFixture.h
    source/hcl/simulation/WaveformRecorder.cpp
    source/hcl/simulation/WaveformRecorder.h
    source/hcl/simulation/waveformFormats/VCDSink.cpp
    source/hcl/simulation/waveformFormats/VCDSink.h

    source/hcl/utils/BitManipulation.h
    source/hcl/utils/BitManipulation.cpp
    source/hcl/utils/Enumerate.h
    source/hcl/utils/Exceptions.cpp
    source/hcl/utils/Exceptions.h
    source/hcl/utils/LinkedList.cpp
    source/hcl/utils/LinkedList.h
    source/hcl/utils/Preprocessor.h
    source/hcl/utils/Range.h
    source/hcl/utils/StackTrace.cpp
    source/hcl/utils/StackTrace.h
    source/hcl/utils/Traits.h
 "source/hcl/hlim/coreNodes/Node_Shift.h" "source/hcl/hlim/coreNodes/Node_Shift.cpp")

set(data_files
)

foreach(file ${data_files})
    configure_file(${file} ${PROJECT_BINARY_DIR}/${file} COPYONLY)
endforeach(file)


target_include_directories(hcl_net SYSTEM PUBLIC
    ${Boost_INCLUDE_DIRS}
)

target_include_directories(hcl_net INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_link_libraries(hcl_net PUBLIC
    ${Boost_LIBRARIES}
)

hcl_setup_target(hcl_net)

