cmake_minimum_required (VERSION 3.0)

add_library(uart
    source/uart.cpp
    source/uart.h
)

target_link_libraries(uart PUBLIC
    metaHDL_core
)

   
add_executable(uart_synthesize_vhdl
    source/synthesize_vhdl.cpp
)

target_link_libraries(uart_synthesize_vhdl
    uart
)

if(${WITH_QT})

add_executable(uart_simulate
    source/simulate.cpp
)

target_link_libraries(uart_simulate
    uart
    metaHDL_vis_qt
)

endif()
