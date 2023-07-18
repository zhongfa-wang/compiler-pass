cmake_minimum_required(VERSION 3.22)

set(CMAKE_LINKER "llvm-link")
set(CMAKE_CXX_FLAGS "-Wall -fno-rtti -Wuninitialized -Wunused-variable -DLLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1")

project(llvm_pass)

find_package(LLVM REQUIRED CONFIG)
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)
add_definitions(${LLVM_DEFINITIONS_LIST})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
# message(${LLVM_CMAKE_DIR})
# message(${LLVM_DEFINITIONS})

#添加调试符号
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
add_subdirectory(InstCount)