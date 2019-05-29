project(lib)

include(CheckLibraryExists)

# LibXml2 Dependency
find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})

# LLVM Dependency
message(STATUS "LLVM Hint: ${LLVM_HINT}")
find_package(LLVM REQUIRED CONFIG HINTS ${LLVM_HINT})
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
include_directories(${LLVM_INCLUDE_DIRS})

# Explicitly select components to link against
llvm_map_components_to_libnames(LLVM_LINK_LIBRARIES
  support
  object)

# LLVM compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fno-rtti -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS")

# Xar
set(BUILD_WITH_XAR ON CACHE BOOL "Build with xar support")
if(BUILD_WITH_XAR)
  find_library(XAR_LIBRARY_NAME NAMES xar)
  if(XAR_LIBRARY_NAME)
    set(HAVE_LIBXAR true)
    set(XAR_LIBRARY ${XAR_LIBRARY_NAME})
  else ()
    check_library_exists(xar xar_open "" HAVE_LIBXAR)
    if(HAVE_LIBXAR)
      set(XAR_LIBRARY xar)
    endif()
  endif()
endif()

if(HAVE_LIBXAR)
  message(STATUS "Building with libxar: " ${XAR_LIBRARY})
else()
  message(STATUS "Building without libxar")
endif()

# Configure header
configure_file (
  "${lib_SOURCE_DIR}/include/ebc/Config.h.in"
  "${lib_BINARY_DIR}/ebc/Config.h")

# UUID
if(UNIX AND NOT APPLE)
  set(UUID_LIBRARY "uuid")
else()
  set(ICONV_LIBRARY "iconv")
endif()

# LibEBC
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE SOURCE_FILES ${SOURCE_DIR}/*.cpp)

add_library(ebc ${SOURCE_FILES})
target_link_libraries(ebc
  ${UUID_LIBRARY}
  ${ICONV_LIBRARY}
  ${XAR_LIBRARY}
  ${LLVM_LINK_LIBRARIES}
  ${LIBXML2_LIBRARIES}
  ${PC_LIBXML_STATIC_LIBRARIES})

target_include_directories(ebc PRIVATE  ${PROJECT_BINARY_DIR})
target_include_directories(ebc PUBLIC ${HEADER_DIR})

# Use C++14
set_property(TARGET ebc PROPERTY CXX_STANDARD 14)
