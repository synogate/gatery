cmake_minimum_required (VERSION 3.0)

option(HCL_WITH_QT "Include QT stuff in the build" off)
option(HCL_WITH_GHDL "Include GHDL stuff in the build" off)

enable_testing()

LIST(APPEND CMAKE_MODULE_PATH cmake/)


function(hcl_setup_target target)
    target_compile_features(${target} PUBLIC cxx_std_20)

    if(MSVC)
        target_compile_definitions(${target} PUBLIC -DBOOST_STACKTRACE_USE_WINDBG)
        # Boost docu: Uses COM to show debug info. May require linking with ole32 and dbgeng. 
    else()
        #target_compile_definitions(hcl PUBLIC -DBOOST_STACKTRACE_USE_BACKTRACE)
        #target_link_libraries(hcl PUBLIC -ldl -lbacktrace)
        target_compile_definitions(${target} PUBLIC -DBOOST_STACKTRACE_USE_ADDR2LINE)
        target_link_libraries(${target} PUBLIC -ldl)
    endif()
endfunction()

add_compile_definitions(NOMINMAX)
add_compile_definitions(WIN32_LEAN_AND_MEAN)
add_compile_definitions(_HAS_DEPRECATED_IS_LITERAL_TYPE=1)

add_subdirectory(net/)
add_subdirectory(frontend/)
add_subdirectory(stl/)

if(${HCL_WITH_GHDL})
add_subdirectory(vpi_sim)
endif()

if(${HCL_WITH_QT})
add_subdirectory(vis/)
endif()


set(HCL_ALL_LIBS ${HCL_ALL_LIBS} PARENT_SCOPE)
set(HCL_WITH_QT ${HCL_WITH_QT} PARENT_SCOPE)
set(HCL_WITH_GHDL ${HCL_WITH_GHDL} PARENT_SCOPE)
