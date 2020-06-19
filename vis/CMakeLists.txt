cmake_minimum_required (VERSION 3.1)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(CMAKE_VERSION VERSION_LESS "3.7.0")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

find_package(Qt5 COMPONENTS Widgets REQUIRED)

find_package(Boost COMPONENTS system filesystem REQUIRED) 


LIST(APPEND HCL_ALL_LIBS hcl_vis)
SET(HCL_ALL_LIBS ${HCL_ALL_LIBS} PARENT_SCOPE)

add_library(hcl_vis STATIC
    source/hcl/vis/BaseGraphicsComposite.cpp
    source/hcl/vis/BaseGraphicsComposite.h
    source/hcl/vis/CHCLSyntaxHighlighter.cpp
    source/hcl/vis/CHCLSyntaxHighlighter.h
    source/hcl/vis/MainWindowSimulate.ui
    source/hcl/vis/MainWindowSimulate.cpp
    source/hcl/vis/MainWindowSimulate.h
    source/hcl/vis/CircuitView.cpp
    source/hcl/vis/CircuitView.h
    source/hcl/vis/EdgeTree.cpp
    source/hcl/vis/EdgeTree.h
    source/hcl/vis/GraphLayouting.cpp
    source/hcl/vis/GraphLayouting.h
    source/hcl/vis/Node.cpp
    source/hcl/vis/Node.h
    source/hcl/vis/Node_ElementaryOp.cpp
    source/hcl/vis/Node_ElementaryOp.h
    source/hcl/vis/Node_Entity.cpp
    source/hcl/vis/Node_Entity.h
    source/hcl/vis/Node_Signal.cpp
    source/hcl/vis/Node_Signal.h
#    resources.qrc
)

target_include_directories(hcl_vis INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_include_directories(hcl_vis PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/hcl_vis_autogen/include
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(hcl_vis PUBLIC
    Qt5::Widgets
    hcl_net
    ${Boost_LIBRARIES}
)

hcl_setup_target(hcl_vis)
