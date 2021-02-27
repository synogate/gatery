cmake_minimum_required (VERSION 3.0)

find_package(Boost COMPONENTS system thread REQUIRED) 

LIST(APPEND HCL_ALL_LIBS hcl_stl)
SET(HCL_ALL_LIBS ${HCL_ALL_LIBS} PARENT_SCOPE)

add_library(hcl_stl STATIC
    source/hcl/stl/buildCtrl/TargetVendor.h
    source/hcl/stl/buildCtrl/TargetVendor.cpp
    source/hcl/stl/hardCores/BlockRam.h
    source/hcl/stl/hardCores/BlockRam.cpp
    source/hcl/stl/hardCores/blockRam/XilinxSimpleDualPortBlockRam.cpp
    source/hcl/stl/hardCores/blockRam/XilinxSimpleDualPortBlockRam.h
    source/hcl/stl/ExternalNodeHandlers.h
    source/hcl/stl/io/HDMITransmitter.h
    source/hcl/stl/io/HDMITransmitter.cpp
    source/hcl/stl/utils/BitCount.h
    source/hcl/stl/utils/BitCount.cpp
    source/hcl/stl/algorithm/TreeReduce.h
    source/hcl/stl/algorithm/TreeReduce.cpp
    "source/hcl/stl/algorithm/GCD.h" 
    "source/hcl/stl/algorithm/GCD.cpp" "source/hcl/stl/algorithm/Stream.h" "source/hcl/stl/utils/OneHot.h" "source/hcl/stl/utils/OneHot.cpp" "source/hcl/stl/hardCores/AsyncRam.h" "source/hcl/stl/hardCores/AsyncRam.cpp" "source/hcl/stl/kvs/TCAM.h" "source/hcl/stl/kvs/TCAM.cpp" "source/hcl/stl/crypto/sha1.h" "source/hcl/stl/crypto/sha1.cpp" "source/hcl/stl/Adder.cpp" "source/hcl/stl/crypto/HashEngine.h" "source/hcl/stl/crypto/md5.h" "source/hcl/stl/crypto/sha2.h" "source/hcl/stl/crypto/SipHash.h" "source/hcl/stl/crypto/SipHash.cpp" "source/hcl/stl/kvs/TinyCuckoo.h" "source/hcl/stl/kvs/TinyCuckoo.cpp")

set(data_files 
)

foreach(file ${data_files})
    configure_file(${file} ${PROJECT_BINARY_DIR}/${file} COPYONLY)
endforeach(file)


target_include_directories(hcl_stl SYSTEM PUBLIC
    ${Boost_INCLUDE_DIRS}
)

target_include_directories(hcl_stl INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_link_libraries(hcl_stl PUBLIC
    hcl_net
    hcl_frontend
    ${Boost_LIBRARIES}
)

hcl_setup_target(hcl_stl)

add_subdirectory(unitTests)
