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

add_library(metaHDL_vis_qt STATIC
    source/metaHDL_vis_Qt/BaseGraphicsComposite.cpp
    source/metaHDL_vis_Qt/BaseGraphicsComposite.h
    source/metaHDL_vis_Qt/CHCLSyntaxHighlighter.cpp
    source/metaHDL_vis_Qt/CHCLSyntaxHighlighter.h
    source/metaHDL_vis_Qt/MainWindowSimulate.ui
    source/metaHDL_vis_Qt/MainWindowSimulate.cpp
    source/metaHDL_vis_Qt/MainWindowSimulate.h
    source/metaHDL_vis_Qt/CircuitView.cpp
    source/metaHDL_vis_Qt/CircuitView.h
    source/metaHDL_vis_Qt/EdgeTree.cpp
    source/metaHDL_vis_Qt/EdgeTree.h
    source/metaHDL_vis_Qt/GraphLayouting.cpp
    source/metaHDL_vis_Qt/GraphLayouting.h
    source/metaHDL_vis_Qt/Node.cpp
    source/metaHDL_vis_Qt/Node.h
    source/metaHDL_vis_Qt/Node_ElementaryOp.cpp
    source/metaHDL_vis_Qt/Node_ElementaryOp.h
    source/metaHDL_vis_Qt/Node_Entity.cpp
    source/metaHDL_vis_Qt/Node_Entity.h
    source/metaHDL_vis_Qt/Node_Signal.cpp
    source/metaHDL_vis_Qt/Node_Signal.h
#    resources.qrc
)

target_include_directories(metaHDL_vis_qt INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_include_directories(metaHDL_vis_qt PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/metaHDL_vis_qt_autogen/include
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(metaHDL_vis_qt PUBLIC
    Qt5::Widgets
    metaHDL_core
    ${Boost_LIBRARIES}
)

hcl_setup_target(hcl_sim)
