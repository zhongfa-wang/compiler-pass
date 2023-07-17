cmake_minimum_required(VERSION 3.1)
#This sets the name of the project to "Skeleton". This will be used for generated libraries/executables.
project(Skeleton)

# support C++14 features used by LLVM 10.0.0
set(CMAKE_CXX_STANDARD 14)

#This finds and imports the LLVM package. REQUIRED means it will error if LLVM is not found. CONFIG uses LLVM's built-in CMake config.
find_package(LLVM REQUIRED CONFIG)

# Adds LLVM compiler definitions needed to build against LLVM headers properly.
add_definitions(${LLVM_DEFINITIONS})

# Adds LLVM include directories to the search path, so headers like Pass.h can be found.
include_directories(${LLVM_INCLUDE_DIRS})

#Adds LLVM library locations to the linker search path.
link_directories(${LLVM_LIBRARY_DIRS})

#This appends LLVM's CMake module path to CMAKE_MODULE_PATH. This allows including LLVM provided CMake modules like AddLLVM.
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")

#Includes the AddLLVM module which provides helpful functions for adding LLVM libraries to a project.
include(AddLLVM)

# Adds required LLVM compiler definitions for building against the LLVM headers.
add_definitions(${LLVM_DEFINITIONS})

# Adds the necessary LLVM include directories to the compiler's header search path.
include_directories(${LLVM_INCLUDE_DIRS})

# Use your pass name here.
add_subdirectory(<InstCount>) 