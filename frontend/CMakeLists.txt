cmake_minimum_required (VERSION 3.0)

find_package(Boost COMPONENTS system thread REQUIRED) 

LIST(APPEND HCL_ALL_LIBS hcl_frontend)
SET(HCL_ALL_LIBS ${HCL_ALL_LIBS} PARENT_SCOPE)

add_library(hcl_frontend STATIC
    source/hcl/frontend/Bit.cpp
    source/hcl/frontend/Bit.h
    source/hcl/frontend/BitVector.cpp
    source/hcl/frontend/BitVector.h
    source/hcl/frontend/Comments.cpp
    source/hcl/frontend/Comments.h
    source/hcl/frontend/ConditionalScope.cpp
    source/hcl/frontend/ConditionalScope.h
    source/hcl/frontend/Constant.h
    source/hcl/frontend/PriorityConditional.cpp
    source/hcl/frontend/PriorityConditional.h
    source/hcl/frontend/Registers.cpp
    source/hcl/frontend/Registers.h
    source/hcl/frontend/Scope.cpp
    source/hcl/frontend/Scope.h
    source/hcl/frontend/Signal.cpp
    source/hcl/frontend/Signal.h
    source/hcl/frontend/SignalArithmeticOp.cpp
    source/hcl/frontend/SignalArithmeticOp.h
    source/hcl/frontend/SignalBitshiftOp.cpp
    source/hcl/frontend/SignalBitshiftOp.h
    source/hcl/frontend/SignalCompareOp.h
    source/hcl/frontend/SignalDelay.cpp
    source/hcl/frontend/SignalDelay.h
    source/hcl/frontend/SignalGenerator.cpp
    source/hcl/frontend/SignalGenerator.h
    source/hcl/frontend/SignalLogicOp.cpp
    source/hcl/frontend/SignalLogicOp.h
    source/hcl/frontend/SignalMiscOp.cpp
    source/hcl/frontend/SignalMiscOp.h
    source/hcl/frontend.h
)



set(data_files 
)

foreach(file ${data_files})
    configure_file(${file} ${PROJECT_BINARY_DIR}/${file} COPYONLY)
endforeach(file)

target_include_directories(hcl_frontend INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)


target_include_directories(hcl_frontend SYSTEM PUBLIC
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(hcl_frontend PUBLIC
    hcl_net
    ${Boost_LIBRARIES}
)

hcl_setup_target(hcl_frontend)

add_subdirectory(unitTests)
