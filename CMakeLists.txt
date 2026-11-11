cmake_minimum_required(VERSION 3.16)
project(AerospaceAdaptiveFT LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(LLVM 15 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} in ${LLVM_INSTALL_PREFIX}")

include_directories(${LLVM_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include)
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# 1. 编译 LLVM Pass 插件: libAdaptiveFTPass.so
add_library(AdaptiveFTPass MODULE passes/AdaptiveFTPass.cpp)
set_target_properties(AdaptiveFTPass PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
    PREFIX "lib"
)

# 2. 编译运行时支持库: libatft_rt.a
add_library(atft_rt STATIC runtime/atft_runtime.c)
target_include_directories(atft_rt PUBLIC ${CMAKE_SOURCE_DIR}/include)
