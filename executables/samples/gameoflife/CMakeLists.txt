cmake_minimum_required (VERSION 3.0)
   
add_executable(gameoflife
    gameoflife.cpp
    pch.h
)

target_link_libraries(gameoflife PUBLIC
    metaHDL_core
)
