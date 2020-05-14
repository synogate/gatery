cmake_minimum_required (VERSION 3.0)
   

add_library(gameoflife
    pch.h
    gameoflife.cpp
    gameoflife.h
)

target_link_libraries(gameoflife PUBLIC
    metaHDL_core
)

   
add_executable(gameoflife_synthesize_vhdl
    synthesize_vhdl.cpp
)

target_link_libraries(gameoflife_synthesize_vhdl
    gameoflife
)

if(${WITH_QT})

add_executable(gameoflife_simulate
    simulate.cpp
)

target_link_libraries(gameoflife_simulate
    gameoflife
    metaHDL_vis_qt
)

endif()
