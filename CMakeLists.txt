cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project (LibEBC)

# Version
set(VERSION_MAJOR 1)
set(VERSION_MINOR 2)

# Git commit hash
execute_process(
  COMMAND git log -1 --format=%h
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE)

# Export compile database
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Link against dynamic/static dependencies
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build with shared libraries dependencies")
if(NOT BUILD_SHARED_LIBS)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif(NOT BUILD_SHARED_LIBS)

# Address Sanitizers
set(SANITIZE OFF CACHE BOOL "Build with sanitizers")
if(SANITIZE)
  message(STATUS "Building with sanitizers")
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
else(SANITIZE)
  message(STATUS "Building without sanitizers")
endif(SANITIZE)

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -Wall -g -O0 -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")

add_subdirectory(lib)
add_subdirectory(test)
add_subdirectory(tool)
